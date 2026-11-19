// Host functions the guest can call through a vtable or a callback pointer: each one gets a synthetic VA in
// 0x7b000000..0x7b0fffff (16-byte steps), resolved by recomp_lookup_import. Used by the D3DX constant table and XAudio2.
constexpr uint32_t hostMethodBase=0x7b000000;
std::vector<recomp_func_t> hostMethods;
uint32_t hostMethod(recomp_func_t fn){hostMethods.push_back(fn);return hostMethodBase+uint32_t(hostMethods.size()-1)*16;}
recomp_func_t hostMethodAt(uint32_t va){
    if(va<hostMethodBase||(va-hostMethodBase)%16)return nullptr;uint32_t i=(va-hostMethodBase)/16;
    return i<hostMethods.size()?hostMethods[i]:nullptr;
}
// A guest COM-like object: [vtable pointer][host id]; the vtable is filled with host method VAs.
uint32_t guestObject(const std::vector<uint32_t>& vtable,uint32_t id,uint32_t extra=0){
    uint32_t table=allocate(uint32_t(vtable.size()*4)),object=allocate(8+extra);
    for(size_t i=0;i<vtable.size();++i)write32(table+uint32_t(i)*4,vtable[i]);
    write32(object,table);write32(object+4,id);if(extra)std::memset(ptr(object+8,extra),0,extra);return object;
}
uint32_t objectId(uint32_t object){return read32(object+4);}
float argf(uint32_t i){uint32_t v=arg(i);float f;std::memcpy(&f,&v,4);return f;}
float readf(uint32_t a){uint32_t v=read32(a);float f;std::memcpy(&f,&v,4);return f;}
void writef(uint32_t a,float f){uint32_t v;std::memcpy(&v,&f,4);write32(a,v);}
// Call a method of a guest object (stdcall, this first).
uint32_t callMethod(uint32_t object,uint32_t slot,std::vector<uint32_t> args){
    args.insert(args.begin(),object);return callGuest(read32(read32(object)+slot*4),args);
}
