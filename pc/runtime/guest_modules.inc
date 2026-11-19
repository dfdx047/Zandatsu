// Game DLLs that run as lifted code next to SPEED2.EXE. server.dll is EA's LAN
// game server: hosting a LAN race does LoadLibrary("server.dll") and calls its
// StartServer/StopServer/IsServerRunning exports.
//
// Lifted code bakes absolute addresses, so each DLL is lifted (run_lift.py
// --prefix) from a copy relocated to a fixed guest base (prepare_module.py) and
// mapped here at that same base: the original file from the game folder, with
// its base relocations applied for that base. The layout check below ties the
// file to the generated code.
struct GuestModuleSpec {const char* name;uint32_t base,preferredBase,imageSize;};
constexpr GuestModuleSpec guestModuleSpecs[]={
    {"server.dll",0x00a00000u,0x10000000u,0x58000u},  // between SPEED2.EXE (ends 0x932000) and the first heap arena
};
struct GuestModule {
    const GuestModuleSpec* spec;uint32_t entry=0,references=0;
    std::map<std::string,uint32_t> exports;  // name or "#ordinal" -> VA
};
std::map<uint32_t,GuestModule> guestModules;  // by base (the HMODULE)

GuestModule* findGuestModule(uint32_t handle){auto found=guestModules.find(handle);return found==guestModules.end()?nullptr:&found->second;}
GuestModule* findGuestModule(const std::string& name){
    for(auto& [base,module]:guestModules)if(module.spec->name==name)return &module;
    return nullptr;
}
bool isGuestModule(uint32_t handle){return findGuestModule(handle)!=nullptr;}
uint32_t guestModuleHandle(const std::string& name){auto* module=findGuestModule(name);return module?module->spec->base:0;}
bool guestModuleExport(uint32_t handle,const std::string& name,uint32_t& va){
    auto* module=findGuestModule(handle);if(!module)return false;
    auto found=module->exports.find(name);if(found==module->exports.end())return false;
    va=found->second;return true;
}
void applyRelocations(uint32_t base,uint32_t size,uint32_t rva,uint32_t bytes,int64_t delta){
    moduleRange(size,rva,bytes);
    for(uint32_t offset=0;offset+8<=bytes;){
        uint32_t page=read32(base+rva+offset),blockSize=read32(base+rva+offset+4);
        if(blockSize<8||offset+blockSize>bytes)throw GuestStop("Bad base relocation block");
        for(uint32_t entry=8;entry+2<=blockSize;entry+=2){
            uint16_t value;std::memcpy(&value,ptr(base+rva+offset+entry,2),2);
            uint32_t type=value>>12,target=page+(value&0xfff);
            if(type==0)continue;  // IMAGE_REL_BASED_ABSOLUTE padding
            if(type!=3)throw GuestStop("Unsupported base relocation type "+std::to_string(type));
            moduleRange(size,target,4);write32(base+target,uint32_t(int64_t(read32(base+target))+delta));
        }
        offset+=blockSize;
    }
}
// Maps, relocates and binds the DLL, then runs DllMain(PROCESS_ATTACH). Returns the HMODULE or 0.
uint32_t loadGuestModule(const std::string& name){
    if(auto* loaded=findGuestModule(name)){++loaded->references;return loaded->spec->base;}
    const GuestModuleSpec* spec=nullptr;
    for(auto& candidate:guestModuleSpecs)if(name==candidate.name)spec=&candidate;
    if(!spec)return 0;
#ifndef NFS_SERVER_DLL
    NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","%s: this build has no lifted code for it",name.c_str());
    return 0;
#endif
    std::ifstream stream(gamePath(std::string("C:\\nfsu2\\")+name),std::ios::binary);
    if(!stream){NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","%s is missing from the game folder",name.c_str());return 0;}
    std::vector<unsigned char> file((std::istreambuf_iterator<char>(stream)),{});
    auto dos=fileStruct<pe_dos_header>(file,0);
    if(dos.e_magic!=0x5a4d||dos.e_lfanew<0)throw GuestStop(name+": invalid DOS header");
    auto nt=fileStruct<pe_nt_headers32>(file,dos.e_lfanew);auto opt=nt.OptionalHeader;
    if(nt.Signature!=0x4550||nt.FileHeader.Machine!=0x14c||opt.Magic!=0x10b||!(nt.FileHeader.Characteristics&0x2000))
        throw GuestStop(name+": expected an x86 PE32 DLL");
    uint32_t base=spec->base,size=opt.SizeOfImage,entry=base+opt.AddressOfEntryPoint;
    if(opt.ImageBase!=spec->preferredBase||size!=spec->imageSize||!recomp_lookup(entry)){
        // Another build of server.dll than the one the code was lifted from: hosting a LAN
        // race fails as with a missing DLL (the game survives that) instead of stopping
        // the game. Its identity goes to the log so the build can be supported later.
        if(name=="server.dll"){
            uint32_t sum=0;for(unsigned char byte:file)sum=sum*31+byte;
            NFS_RUNTIME_LOG(ANDROID_LOG_ERROR,"NFSU2","%s: not the build the code was lifted from (base %s, size %s, entry %s, timestamp %08x, %zu bytes, sum %08x); LoadLibrary fails",
                name.c_str(),hex(opt.ImageBase).c_str(),hex(size).c_str(),hex(opt.AddressOfEntryPoint).c_str(),nt.FileHeader.TimeDateStamp,file.size(),sum);
            postGuestNotice("server.dll");return 0;
        }
        throw GuestStop(name+": file layout does not match the lifted code (base "+hex(opt.ImageBase)+", size "+hex(size)+")");
    }
    memory->commit(base,size);
    std::memset(ptr(base,size),0,size);
    if(opt.SizeOfHeaders>file.size()||opt.SizeOfHeaders>size)throw GuestStop(name+": truncated PE headers");
    std::memcpy(ptr(base,opt.SizeOfHeaders),file.data(),opt.SizeOfHeaders);
    uint64_t sections=uint64_t(dos.e_lfanew)+24+nt.FileHeader.SizeOfOptionalHeader;
    for(uint32_t i=0;i<nt.FileHeader.NumberOfSections;++i){
        auto section=fileStruct<pe_section_header>(file,sections+i*sizeof(pe_section_header));
        moduleRange(size,section.VirtualAddress,std::max(section.VirtualSize,section.SizeOfRawData));
        if(uint64_t(section.PointerToRawData)+section.SizeOfRawData>file.size())throw GuestStop(name+": truncated PE section");
        std::memcpy(ptr(base+section.VirtualAddress,section.SizeOfRawData),file.data()+section.PointerToRawData,section.SizeOfRawData);
    }
    auto relocations=opt.DataDirectory[PE_DIR_BASERELOC];
    if(!relocations.Size)throw GuestStop(name+": no base relocations to move it to "+hex(base));
    applyRelocations(base,size,relocations.VirtualAddress,relocations.Size,int64_t(base)-int64_t(opt.ImageBase));
    bindImports(base,size,opt.DataDirectory[PE_DIR_IMPORT].VirtualAddress,opt.DataDirectory[PE_DIR_IMPORT].Size);
    GuestModule module{spec,entry,1,{}};
    auto exports=opt.DataDirectory[0];
    if(exports.Size){
        moduleRange(size,exports.VirtualAddress,40);uint32_t directory=base+exports.VirtualAddress;
        uint32_t ordinalBase=read32(directory+16),functions=read32(directory+20),names=read32(directory+24);
        uint32_t addressTable=read32(directory+28),nameTable=read32(directory+32),ordinalTable=read32(directory+36);
        moduleRange(size,addressTable,uint64_t(functions)*4);moduleRange(size,nameTable,uint64_t(names)*4);moduleRange(size,ordinalTable,uint64_t(names)*2);
        for(uint32_t i=0;i<functions;++i)if(uint32_t rva=read32(base+addressTable+i*4))module.exports["#"+std::to_string(ordinalBase+i)]=base+rva;
        for(uint32_t i=0;i<names;++i){
            uint16_t index;std::memcpy(&index,ptr(base+ordinalTable+i*2,2),2);
            if(index<functions)module.exports[moduleString(base,size,read32(base+nameTable+i*4))]=base+read32(base+addressTable+index*4u);
        }
    }
    guestModules[base]=module;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","%s mapped at %08x (preferred %08x), %zu exports; running DllMain",name.c_str(),base,opt.ImageBase,module.exports.size());
    if(!callGuest(entry,{base,1,0})){  // DLL_PROCESS_ATTACH
        guestModules.erase(base);
        NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","%s: DllMain refused PROCESS_ATTACH",name.c_str());
        return 0;
    }
    return base;
}
// FreeLibrary: the last reference runs DllMain(PROCESS_DETACH); the next load maps a fresh copy.
bool freeGuestModule(uint32_t handle){
    auto* module=findGuestModule(handle);if(!module)return false;
    if(--module->references)return true;
    uint32_t entry=module->entry;
    callGuest(entry,{handle,0,0});  // DLL_PROCESS_DETACH
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","%s unloaded",module->spec->name);
    guestModules.erase(handle);
    return true;
}
