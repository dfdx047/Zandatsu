#include "guest_host_methods.inc"
#include <iconv.h>
#include <cctype>
#include <cstring>
// Win32 imports of the MGR:R PC executable that the base runtime (adapted from codepdbh/nfsu2-recompiled, MIT) did not
// answer. Included inside the runtime namespace after guest_files.inc, guest_threads.inc and guest_kernel.inc.
// Every shim follows the 32-bit stdcall contract: ret(value, argument count).

// ---- asynchronous file I/O: ReadFileEx/WriteFileEx complete at once; the completion routine is an APC that runs on
// the next alertable wait of the same guest thread (SleepEx / WaitForMultipleObjectsEx with bAlertable).
struct GuestApc {uint32_t routine,error,bytes,overlapped;};
thread_local std::deque<GuestApc> pendingApcs;
uint64_t overlappedOffset(uint32_t overlapped){return uint64_t(read32(overlapped+8))|(uint64_t(read32(overlapped+12))<<32);}
void completeOverlapped(uint32_t overlapped,uint32_t error,uint32_t bytes){
    write32(overlapped,error?0xC0000011u:0);write32(overlapped+4,bytes);
    if(uint32_t event=read32(overlapped+16)){auto found=eventHandles.find(event);if(found!=eventHandles.end())found->second->signaled=true;}
}
bool transferAt(uint32_t handle,uint32_t buffer,uint32_t size,uint64_t offset,bool writing,uint32_t& done,uint32_t& error){
    done=0;error=0;auto file=fileHandles.find(handle);if(file==fileHandles.end()){error=6;return false;}
    auto* data=static_cast<uint8_t*>(ptr(buffer,size));
    while(done<size){
        ssize_t count=writing?::pwrite(file->second,data+done,size-done,off_t(offset+done)) : ::pread(file->second,data+done,size-done,off_t(offset+done));
        if(count<0){if(errno==EINTR)continue;error=fileError();return false;}
        if(!count)break;done+=uint32_t(count);
    }
    if(!writing&&!done&&size){error=38;}   // ERROR_HANDLE_EOF: a read at or past the end
    return true;
}
void fileEx(bool writing){
    uint32_t handle=arg(0),buffer=arg(1),size=arg(2),overlapped=arg(3),routine=arg(4),done,error;
    if(!overlapped){lastError=87;ret(0,5);return;}
    if(!transferAt(handle,buffer,size,overlappedOffset(overlapped),writing,done,error)){lastError=error;ret(0,5);return;}
    completeOverlapped(overlapped,error,done);
    if(routine)pendingApcs.push_back({routine,error,done,overlapped});
    lastError=0;ret(1,5);
}
void readFileEx(){fileEx(false);}
void writeFileEx(){fileEx(true);}
void cancelIo(){ret(fileHandles.count(arg(0))?1:0,1);}
bool runApcs(){
    if(pendingApcs.empty())return false;
    while(!pendingApcs.empty()){auto apc=pendingApcs.front();pendingApcs.pop_front();callGuest(apc.routine,{apc.error,apc.bytes,apc.overlapped});}
    return true;
}
void sleepExAlertable(){
    uint32_t ms=arg(0);bool alertable=arg(1)!=0;
    if(alertable&&runApcs()){ret(0xC0,2);return;}   // WAIT_IO_COMPLETION
    if(!ms){checkGuestProgress();yieldGuestMachine(0);ret(0,2);return;}
    while(ms){checkGuestProgress();auto chunk=std::min(ms,10u);yieldGuestMachine(chunk);if(alertable&&runApcs()){ret(0xC0,2);return;}if(ms!=UINT32_MAX)ms-=chunk;}
    ret(0,2);
}

// ---- semaphores, waited on through the same paths as events (see waitableReady/acquireWaitable patches)
void createSemaphore(){
    int32_t initial=int32_t(arg(1)),maximum=int32_t(arg(2));
    if(maximum<=0||initial<0||initial>maximum){lastError=87;ret(0,4);return;}
    uint32_t handle=nextSemaphoreHandle++;semaphoreHandles.emplace(handle,std::make_shared<GuestSemaphore>(GuestSemaphore{initial,maximum}));
    lastError=0;ret(handle,4);
}
void releaseSemaphore(){
    auto found=semaphoreHandles.find(arg(0));if(found==semaphoreHandles.end()){lastError=6;ret(0,3);return;}
    auto& s=*found->second;int32_t add=int32_t(arg(1));if(add<=0||s.count+add>s.maximum){lastError=298;ret(0,3);return;}
    if(arg(2))write32(arg(2),uint32_t(s.count));s.count+=add;ret(1,3);
}
void waitMultipleEx(){
    uint32_t count=arg(0),list=arg(1),timeout=arg(3);bool all=arg(2)!=0,alertable=arg(4)!=0;
    if(alertable&&runApcs()){ret(0xC0,5);return;}
    if(!count||count>64||!list){lastError=87;ret(UINT32_MAX,5);return;}
    std::vector<uint32_t> handles(count);for(uint32_t i=0;i<count;++i)handles[i]=read32(list+i*4);
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    WaitScope scope("WaitForMultipleObjectsEx",handles[0]);
    for(;;){
        if(all){if(std::all_of(handles.begin(),handles.end(),waitableReady)){bool ab=false;for(auto h:handles)ab|=acquireWaitable(h);ret(ab?128:0,5);return;}}
        else for(uint32_t i=0;i<count;++i)if(waitableReady(handles[i])){ret((acquireWaitable(handles[i])?128:0)+i,5);return;}
        if(alertable&&runApcs()){ret(0xC0,5);return;}
        if(timeout!=UINT32_MAX&&std::chrono::steady_clock::now()>=deadline){ret(258,5);return;}
        checkGuestProgress();yieldGuestMachine(1);
    }
}

// ---- file mapping (read-only views copied into guest memory; the game maps small files)
struct GuestMapping {uint32_t file;uint64_t size;};
std::map<uint32_t,GuestMapping> mappingHandles;uint32_t nextMappingHandle=0xf00b0000;
bool closeMapping(uint32_t handle){return mappingHandles.erase(handle)>0;}
std::map<uint32_t,uint32_t> mappedViews;   // guest address -> size
void createFileMapping(){
    uint32_t file=arg(0);auto found=fileHandles.find(file);if(found==fileHandles.end()){lastError=6;ret(0,6);return;}
    struct stat status{};fstat(found->second,&status);uint64_t size=(uint64_t(arg(3))<<32)|arg(4);if(!size)size=uint64_t(status.st_size);
    uint32_t handle=nextMappingHandle++;mappingHandles.emplace(handle,GuestMapping{file,size});lastError=0;ret(handle,6);
}
void mapViewOfFile(){
    auto found=mappingHandles.find(arg(0));if(found==mappingHandles.end()){lastError=6;ret(0,5);return;}
    uint64_t offset=(uint64_t(arg(2))<<32)|arg(3);uint32_t size=arg(4)?arg(4):uint32_t(found->second.size-offset);
    uint32_t view=allocate(size);uint32_t done,error;
    if(!transferAt(found->second.file,view,size,offset,false,done,error)&&error!=38){heaps->free(GuestHeaps::process,view);lastError=error;ret(0,5);return;}
    mappedViews[view]=size;lastError=0;ret(view,5);
}
void unmapViewOfFile(){auto found=mappedViews.find(arg(0));if(found==mappedViews.end()){lastError=487;ret(0,1);return;}heaps->free(GuestHeaps::process,found->first);mappedViews.erase(found);ret(1,1);}

// ---- wide-character and locale calls of the VS2010 CRT
std::string narrowFromWide(uint32_t address){std::string out;if(!address)return out;for(auto c:wideString(address))out.push_back(c<128?char(c):'?');return out;}
void createFileW(){
    // Rewrite the path argument as an ANSI copy and reuse CreateFileA (same 7 arguments).
    uint32_t ansi=ansiString(narrowFromWide(arg(0)));write32(g_esp+4,ansi);createFile();
}
void createEventW(){write32(g_esp+16,arg(3)?ansiString(narrowFromWide(arg(3))):0);createEvent();}
void loadLibraryW(){write32(g_esp+4,ansiString(narrowFromWide(arg(0))));loadLibrary();}
void moduleFileNameW(){
    // GetModuleFileNameW(module, buffer, size): the path of the exe, as UTF-16.
    std::string path="C:\\nfsu2\\METAL GEAR RISING REVENGEANCE.exe";uint32_t size=arg(2),n=std::min<uint32_t>(uint32_t(path.size()),size?size-1:0);
    for(uint32_t i=0;i<n;++i){uint16_t c=uint8_t(path[i]);std::memcpy(ptr(arg(1)+i*2,2),&c,2);}
    if(size){uint16_t z=0;std::memcpy(ptr(arg(1)+n*2,2),&z,2);}ret(n,3);
}
void startupInfoW(){std::memset(ptr(arg(0),68),0,68);write32(arg(0),68);ret(0,1);}
void compareStringW(){
    auto a=wideString(arg(2)),b=wideString(arg(4));int32_t la=int32_t(arg(3)),lb=int32_t(arg(5));
    if(la>=0&&size_t(la)<a.size())a.resize(la);if(lb>=0&&size_t(lb)<b.size())b.resize(lb);
    bool ignoreCase=arg(1)&1;auto fold=[&](uint16_t c){return ignoreCase&&c>='a'&&c<='z'?uint16_t(c-32):c;};
    int r=0;for(size_t i=0;i<std::min(a.size(),b.size())&&!r;++i)r=int(fold(a[i]))-int(fold(b[i]));
    if(!r)r=int(a.size())-int(b.size());ret(r<0?1:r>0?3:2,6);
}
void userDefaultLCID(){ret(0x409,0);}
void isValidLocale(){ret(1,2);}
void isValidCodePage(){uint32_t cp=arg(0);ret(cp==1252||cp==65001||cp==437||cp==0,1);}
void enumSystemLocales(){ret(1,2);}   // no callback: the CRT only validates a user locale with it
void localeInfo(bool wide){
    // GetLocaleInfoA/W(lcid, type, buffer, size): the few LCTYPEs the CRT asks, for en-US.
    uint32_t type=arg(1)&0xfff,buffer=arg(2),size=arg(3);std::string value;
    switch(type){case 0x1:value="0409";break;case 0x2:case 0x1001:value="English";break;case 0x3:value="ENU";break;case 0x7:case 0x1002:value="United States";break;
        case 0xb:value="437";break;case 0x1004:value="1252";break;case 0xe:value=".";break;case 0xf:value=",";break;case 0x14:value="$";break;
        case 0x59:value="en";break;case 0x5a:value="US";break;default:value="";}
    uint32_t need=uint32_t(value.size()+1);if(!size){ret(need,4);return;}if(size<need){lastError=122;ret(0,4);return;}
    for(uint32_t i=0;i<need;++i){char c=i<value.size()?value[i]:0;if(wide){uint16_t w=uint8_t(c);std::memcpy(ptr(buffer+i*2,2),&w,2);}else std::memcpy(ptr(buffer+i,1),&c,1);}
    ret(need,4);
}
void localeInfoA(){localeInfo(false);}
void localeInfoW(){localeInfo(true);}
void consoleCP(){ret(437,0);}
void writeConsoleW(){if(arg(3))write32(arg(3),arg(2));ret(1,5);}
void setStdHandle(){ret(1,2);}
void setConsoleCtrlHandler(){ret(1,2);}
void setEnvironmentVariableA(){ret(1,2);}
void fatalAppExit(){throw GuestStop("FatalAppExitA: "+guestString(arg(1)));}
void terminateProcess(){throw GuestStop("TerminateProcess("+std::to_string(arg(1))+") from "+hex(g_cur_func));}
void unhandledExceptionFilter(){ret(1,1);}   // EXCEPTION_EXECUTE_HANDLER
void raiseException(){
    uint32_t code=arg(0),count=arg(2),list=arg(3);std::string info;
    // 0x406D1388 (MS_VC_EXCEPTION): SetThreadName for a debugger, inside __try/__except(EXCEPTION_EXECUTE_HANDLER).
    // With no debugger the handler swallows it; returning has the same effect. THREADNAME_INFO: type, name, thread id, flags.
    if(code==0x406D1388u&&count>=3){NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","Thread name %08x = %s",read32(list+8),guestString(read32(list+4)).c_str());ret(0,4);return;}
    for(uint32_t i=0;i<std::min(count,4u);++i)info+=" "+hex(read32(list+i*4));
    throw GuestStop("RaiseException "+hex(code)+info+" from "+hex(g_cur_func)+" (guest C++/SEH exceptions not wired yet)");
}
void rtlUnwind(){ret(0,4);}
void localTime(){
    std::time_t now=std::time(nullptr);std::tm t{};localtime_r(&now,&t);
    uint16_t fields[]={uint16_t(t.tm_year+1900),uint16_t(t.tm_mon+1),uint16_t(t.tm_wday),uint16_t(t.tm_mday),uint16_t(t.tm_hour),uint16_t(t.tm_min),uint16_t(t.tm_sec),0};
    std::memcpy(ptr(arg(0),16),fields,16);ret(0,1);
}
void heapSetInformation(){ret(1,4);}
void heapQueryInformation(){if(arg(2)&&arg(3)>=4)write32(arg(2),2);if(arg(4))write32(arg(4),4);ret(1,5);}   // LFH
void setCriticalSectionSpinCount(){ret(0,2);}
void interlockedExchangeAdd(){uint32_t target=arg(0),old=read32(target);write32(target,old+arg(1));ret(old,2);}
void setThreadAffinityMask(){ret(1,2);}
void setThreadIdealProcessor(){ret(0,2);}
void openThread(){auto found=std::find_if(threadHandles.begin(),threadHandles.end(),[&](auto& e){return e.second->id==arg(2);});ret(found==threadHandles.end()?0:found->first,3);}
void virtualQuery(){
    // VirtualQuery(address, info, size): one committed read/write region around the address (the CRT probes its stack).
    uint32_t a=arg(0)&~0xfffu,out=arg(1);if(arg(2)<28){ret(0,3);return;}
    write32(out,a);write32(out+4,a);write32(out+8,4);write32(out+12,0x1000);write32(out+16,0x1000);write32(out+20,4);write32(out+24,0x20000);ret(28,3);
}
void globalAlloc(){uint32_t size=arg(1)?arg(1):1,p=allocate(size);if(arg(0)&0x40)std::memset(ptr(p,size),0,size);ret(p,2);}
void globalFree(){if(arg(0))heaps->free(GuestHeaps::process,arg(0));ret(0,1);}
void globalSize(){ret(arg(0)?heaps->size(GuestHeaps::process,arg(0)):0,1);}
void copyFileA(){
    std::error_code e;auto from=gamePath(guestString(arg(0))),to=gamePath(guestString(arg(1)));
    auto opt=arg(2)?std::filesystem::copy_options::none:std::filesystem::copy_options::overwrite_existing;
    bool ok=std::filesystem::copy_file(from,to,opt,e);lastError=ok?0:(e?fileError():80);ret(ok,3);
}
void fileInformationByHandle(){
    auto file=fileHandles.find(arg(0));struct stat s{};if(file==fileHandles.end()||fstat(file->second,&s)){lastError=6;ret(0,2);return;}
    uint32_t o=arg(1);std::memset(ptr(o,52),0,52);write32(o,S_ISDIR(s.st_mode)?0x10:0x80);
    uint64_t t=uint64_t(s.st_mtim.tv_sec)*10000000ull+116444736000000000ull;for(uint32_t f=4;f<=20;f+=8){write32(o+f,uint32_t(t));write32(o+f+4,uint32_t(t>>32));}
    write32(o+32,uint32_t(uint64_t(s.st_size)>>32));write32(o+36,uint32_t(s.st_size));write32(o+40,1);ret(1,2);
}
void setCurrentDirectoryA(){ret(1,1);}   // paths are resolved against the game root
void createProcessA(){lastError=2;ret(0,10);}
void exitCodeProcess(){lastError=6;ret(0,2);}
void getModuleHandleA(){moduleHandle();}

#include "guest_d3dx9.inc"
#include "guest_xaudio2.inc"

// ---- user32 extras (one window, no desktop)
void windowLongW(){getWindowLong();}
void setWindowLongW(){setWindowLong();}
void callWindowProcW(){ret(callGuest(arg(0),{arg(1),arg(2),arg(3),arg(4)}),5);}
// Guest return addresses on the stack (diagnostics): dwords in .text from esp upward.
void logGuestStack(const char* why){
    std::string line;uint32_t found=0;
    uint32_t top=read32(g_fs_base+4);for(uint32_t a=g_esp;a+4<=top&&found<40;a+=4){uint32_t v=read32(a);if(v>=0x401000u&&v<0x15fd000u){char b[16];std::snprintf(b,sizeof b," %08x",v);line+=b;++found;}}
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","%s: stack%s",why,line.c_str());
}
void destroyWindow(){logGuestStack("DestroyWindow");ret(1,1);}
void unregisterClass(){ret(1,2);}
void findWindowA(){ret(0,2);}   // the game looks for a running copy of itself
void isIconic(){ret(0,1);}
void clientToScreen(){ret(1,2);}
void clipCursor(){ret(1,1);}
void setCursorPos(){ret(1,2);}
void getDC(){ret(0xf00c0001u,1);}
void registerDeviceNotification(){ret(0xf00c0002u,3);}
void unregisterDeviceNotification(){ret(1,1);}

const std::map<std::string,recomp_func_t> mgrKernelFunctions={
    {"ReadFileEx",readFileEx},{"WriteFileEx",writeFileEx},{"CancelIo",cancelIo},{"SleepEx",sleepExAlertable},
    {"CreateSemaphoreA",createSemaphore},{"ReleaseSemaphore",releaseSemaphore},{"WaitForMultipleObjectsEx",waitMultipleEx},
    {"CreateFileMappingA",createFileMapping},{"MapViewOfFile",mapViewOfFile},{"UnmapViewOfFile",unmapViewOfFile},
    {"CreateFileW",createFileW},{"CreateEventW",createEventW},{"LoadLibraryW",loadLibraryW},{"GetModuleFileNameW",moduleFileNameW},
    {"GetStartupInfoW",startupInfoW},{"CompareStringW",compareStringW},{"GetUserDefaultLCID",userDefaultLCID},
    {"IsValidLocale",isValidLocale},{"IsValidCodePage",isValidCodePage},{"EnumSystemLocalesA",enumSystemLocales},
    {"GetLocaleInfoA",localeInfoA},{"GetLocaleInfoW",localeInfoW},{"GetConsoleCP",consoleCP},{"WriteConsoleW",writeConsoleW},
    {"SetStdHandle",setStdHandle},{"SetConsoleCtrlHandler",setConsoleCtrlHandler},{"SetEnvironmentVariableA",setEnvironmentVariableA},
    {"FatalAppExitA",fatalAppExit},{"TerminateProcess",terminateProcess},{"UnhandledExceptionFilter",unhandledExceptionFilter},
    {"RaiseException",raiseException},{"RtlUnwind",rtlUnwind},{"GetLocalTime",localTime},
    {"HeapSetInformation",heapSetInformation},{"HeapQueryInformation",heapQueryInformation},
    {"SetCriticalSectionSpinCount",setCriticalSectionSpinCount},{"InterlockedExchangeAdd",interlockedExchangeAdd},
    {"SetThreadAffinityMask",setThreadAffinityMask},{"SetThreadIdealProcessor",setThreadIdealProcessor},{"OpenThread",openThread},
    {"VirtualQuery",virtualQuery},{"GlobalAlloc",globalAlloc},{"GlobalFree",globalFree},{"GlobalSize",globalSize},
    {"CopyFileA",copyFileA},{"GetFileInformationByHandle",fileInformationByHandle},{"SetCurrentDirectoryA",setCurrentDirectoryA},
    {"CreateProcessA",createProcessA},{"GetExitCodeProcess",exitCodeProcess},{"GetModuleHandleA",getModuleHandleA},
};
const std::map<std::string,recomp_func_t> mgrWindowFunctions={
    {"GetWindowLongW",windowLongW},{"SetWindowLongW",setWindowLongW},{"CallWindowProcW",callWindowProcW},
    {"DestroyWindow",destroyWindow},{"UnregisterClassA",unregisterClass},{"FindWindowA",findWindowA},{"IsIconic",isIconic},
    {"ClientToScreen",clientToScreen},{"ClipCursor",clipCursor},{"SetCursorPos",setCursorPos},{"GetDC",getDC},
    {"RegisterDeviceNotificationW",registerDeviceNotification},{"UnregisterDeviceNotification",unregisterDeviceNotification},
};


// ---- The game's own debug print (0x00DD5650, 0x00DD56A0: `ret` in the retail exe, cdecl format + varargs). Overridden
// (pc/config/overrides.txt) to print through our log: free diagnostics in the game's words (Shift-JIS, converted).
std::string guestPrintf(uint32_t format,uint32_t args){
    std::string out,fmt=guestString(format);
    for(size_t i=0;i<fmt.size();++i){
        if(fmt[i]!='%'){out+=fmt[i];continue;}
        size_t j=i+1;std::string spec="%";
        while(j<fmt.size()&&strchr("-+ #0",fmt[j]))spec+=fmt[j++];
        while(j<fmt.size()&&(isdigit(uint8_t(fmt[j]))||fmt[j]=='.'||fmt[j]=='*')){if(fmt[j]=='*'){spec+=std::to_string(int32_t(read32(args)));args+=4;++j;}else spec+=fmt[j++];}
        bool wide64=false;
        while(j<fmt.size()&&strchr("hlLIwjzt346",fmt[j])){if(fmt.compare(j,3,"I64")==0||fmt.compare(j,2,"ll")==0){wide64=true;j+=fmt[j]=='I'?3:2;}else ++j;}
        if(j>=fmt.size())break;char c=fmt[j];char buf[512];buf[0]=0;
        switch(c){
        case '%':out+='%';break;
        case 'd':case 'i':case 'u':case 'x':case 'X':case 'o':case 'c':
            if(wide64){uint64_t v=uint64_t(read32(args))|(uint64_t(read32(args+4))<<32);args+=8;std::snprintf(buf,sizeof buf,(spec+"ll"+c).c_str(),(long long)v);}
            else{uint32_t v=read32(args);args+=4;std::snprintf(buf,sizeof buf,(spec+c).c_str(),c=='d'||c=='i'?int(int32_t(v)):int(v));}
            out+=buf;break;
        case 'p':std::snprintf(buf,sizeof buf,"%08x",read32(args));args+=4;out+=buf;break;
        case 'f':case 'F':case 'e':case 'E':case 'g':case 'G':{double d;uint64_t v=uint64_t(read32(args))|(uint64_t(read32(args+4))<<32);args+=8;std::memcpy(&d,&v,8);
            std::snprintf(buf,sizeof buf,(spec+c).c_str(),d);out+=buf;break;}
        case 's':{uint32_t a=read32(args);args+=4;out+=a&&memory->readable(a,1)?guestString(a):std::string("(null)");break;}
        default:out+=spec+c;break;
        }
        i=j;
    }
    return out;
}
std::string fromShiftJis(const std::string& text){
#if defined(__ANDROID__) && __ANDROID_API__ < 28
    return text;
#else
    iconv_t cd=iconv_open("UTF-8","CP932");if(cd==(iconv_t)-1)return text;
    std::string out(text.size()*3+4,'\0');char* in=const_cast<char*>(text.data());size_t inLeft=text.size();char* o=out.data();size_t outLeft=out.size();
    while(inLeft&&iconv(cd,&in,&inLeft,&o,&outLeft)==(size_t)-1){if(errno==EILSEQ||errno==EINVAL){if(outLeft){*o++='?';--outLeft;}++in;--inLeft;}else break;}
    iconv_close(cd);out.resize(out.size()-outLeft);return out;
#endif
}
extern "C" void mgr_game_log(void){
    // The original body is a bare `ret`: pop the (dummy) return address, leave eax and everything else alone.
    struct PopReturn{~PopReturn(){g_esp+=4;}} pop;
    static uint32_t lines=0;if(lines>=5000)return;   // a log in a per-frame loop must not fill the disk
    std::string text=fromShiftJis(guestPrintf(arg(0),g_esp+8));while(!text.empty()&&(text.back()=='\n'||text.back()==' '))text.pop_back();
    for(auto& ch:text)if(ch=='\n')ch='|';
    ++lines;NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGRGAME","%s",text.c_str());
}
// ---- steam_api (cdecl): Steam does not exist on Android. SteamAPI_Init answers false: the game logs "SteamAPI_Init() is
// failed" and then leaves its main loop on the first frame (see below).
void steamInit(){NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","SteamAPI_Init: Steam not available");ret(0,0);}
// The game quits when SteamAPI_Init fails (0x00A530C8 sets the quit request 0x01BE8E3E). What the shipped port does about
// it is the owner's decision (PCPorts drm-legal); a local, untracked build may supply its own Steam functions here.
#ifdef MGR_LAB_INCLUDE
#include MGR_LAB_INCLUDE
#endif
#ifndef MGR_STEAM_INIT
#define MGR_STEAM_INIT steamInit
#endif
void steamVoid(){ret(0,0);}
void steamNull(){ret(0,0);}
// ---- XInput 1.3 by ordinal: #2 XInputGetState, #3 XInputSetState, #4 XInputGetCapabilities
struct XInputSnapshot {uint16_t buttons;uint8_t leftTrigger,rightTrigger;int16_t lx,ly,rx,ry;};
bool gamepadConnected(){return inputPadConnected;}
XInputSnapshot gamepadSnapshot(){
    // guest_input.inc keeps the pad as DirectInput sees it: buttons A B X Y LB RB Back Start LS RS (bits 0-9), axes
    // X Y Z(triggers: LT negative, RT positive) Rx Ry Rz in -1..1, Y down positive; XInput wants Y up positive.
    static const uint16_t bit[10]={0x1000,0x2000,0x4000,0x8000,0x100,0x200,0x20,0x10,0x40,0x80};
    XInputSnapshot s{};for(int i=0;i<10;++i)if(inputPad.buttons&(1u<<i))s.buttons|=bit[i];
    if(inputPad.hatY<0)s.buttons|=1;if(inputPad.hatY>0)s.buttons|=2;if(inputPad.hatX<0)s.buttons|=4;if(inputPad.hatX>0)s.buttons|=8;
    auto axis=[](float v,bool flip){v=std::clamp(flip?-v:v,-1.f,1.f);return int16_t(v*32767.f);};
    s.lx=axis(inputPad.axis[0],false);s.ly=axis(inputPad.axis[1],true);s.rx=axis(inputPad.axis[3],false);s.ry=axis(inputPad.axis[4],true);
    float z=inputPad.axis[2];s.leftTrigger=uint8_t(std::clamp(-z,0.f,1.f)*255.f);s.rightTrigger=uint8_t(std::clamp(z,0.f,1.f)*255.f);
    return s;
}
uint32_t xinputPacket=0;
void xinputGetState(){
    uint32_t user=arg(0),state=arg(1);if(user!=0||!gamepadConnected()){ret(1167,2);return;}   // ERROR_DEVICE_NOT_CONNECTED
    XInputSnapshot s=gamepadSnapshot();write32(state,++xinputPacket);
    uint16_t buttons=s.buttons;std::memcpy(ptr(state+4,2),&buttons,2);
    uint8_t lt=s.leftTrigger,rt=s.rightTrigger;std::memcpy(ptr(state+6,1),&lt,1);std::memcpy(ptr(state+7,1),&rt,1);
    int16_t axes[4]={s.lx,s.ly,s.rx,s.ry};std::memcpy(ptr(state+8,8),axes,8);ret(0,2);
}
void xinputSetState(){ret(arg(0)==0&&gamepadConnected()?0:1167,2);}   // vibration ignored for now
void xinputCaps(){
    if(arg(0)!=0||!gamepadConnected()){ret(1167,3);return;}
    uint32_t c=arg(2);std::memset(ptr(c,20),0,20);uint8_t type=1,sub=1;std::memcpy(ptr(c,1),&type,1);std::memcpy(ptr(c+1,1),&sub,1);
    uint16_t all=0xF3FF;std::memcpy(ptr(c+4,2),&all,2);ret(0,3);
}
// ---- ws2_32 by ordinal -> the base runtime's names; anything else fails like a machine without network
const std::map<uint32_t,const char*> ws2Ordinals={{1,"accept"},{2,"bind"},{3,"closesocket"},{4,"connect"},{9,"htons"},{10,"ioctlsocket"},
    {11,"inet_addr"},{12,"inet_ntoa"},{13,"listen"},{15,"ntohs"},{16,"recv"},{18,"select"},{19,"send"},{21,"setsockopt"},{23,"socket"},
    {52,"gethostbyname"},{57,"gethostname"},{101,"WSAAsyncSelect"},{111,"WSAGetLastError"},{115,"WSAStartup"},{116,"WSACleanup"},{151,"__WSAFDIsSet"}};
// ---- ole32
void coInitialize(){ret(0,2);}
void coUninitialize(){ret(0,0);}
void oleInitialize(){ret(0,1);}
void oleUninitialize(){ret(0,0);}
std::string guidString(uint32_t g){char b[64];uint32_t d1=read32(g);uint16_t d2,d3;std::memcpy(&d2,ptr(g+4,2),2);std::memcpy(&d3,ptr(g+6,2),2);
    auto* d4=static_cast<uint8_t*>(ptr(g+8,8));std::snprintf(b,sizeof b,"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",d1,d2,d3,d4[0],d4[1],d4[2],d4[3],d4[4],d4[5],d4[6],d4[7]);return b;}
recomp_func_t (*comFactory)(const std::string& clsid)=nullptr;   // XAudio2 2.7 server registers here (guest_xaudio2.inc)
uint32_t createXAudio2(uint32_t out);
void coCreateInstance(){
    auto clsid=guidString(arg(0));uint32_t out=arg(4);
    // XAudio2 2.7 (June 2010 SDK): CLSID_XAudio2 5A508685-A254-4FBA-9B82-9A24B00306AF, debug DB05EA35-0329-4D4B-A53A-6DEAD03D3852
    if(clsid=="5A508685-A254-4FBA-9B82-9A24B00306AF"||clsid=="DB05EA35-0329-4D4B-A53A-6DEAD03D3852"){ret(createXAudio2(out),5);return;}
    NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"MGR","CoCreateInstance %s iid %s: class not registered",clsid.c_str(),guidString(arg(3)).c_str());
    if(out)write32(out,0);ret(0x80040154u,5);
}
void createStreamOnHGlobal(){NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"MGR","CreateStreamOnHGlobal not implemented");if(arg(2))write32(arg(2),0);ret(0x80004001u,3);}
void olePro32(){NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"MGR","OLEPRO32 #251 called from %08x",g_cur_func);ret(0x80004001u,4);}
// ---- gdi32
void gdiGetObject(){ret(0,3);}
void gdiDeviceCaps(){uint32_t i=arg(1),v=0;switch(i){case 8:v=1280;break;case 10:v=720;break;case 12:v=32;break;case 14:v=1;break;case 88:case 90:v=96;break;case 116:v=60;break;}ret(v,2);}

const std::map<std::string,recomp_func_t> mgrSteam={
    {"SteamAPI_Init",MGR_STEAM_INIT},{"SteamAPI_Shutdown",steamVoid},{"SteamAPI_RunCallbacks",steamVoid},{"SteamAPI_RegisterCallback",steamVoid},
    {"SteamAPI_UnregisterCallback",steamVoid},{"SteamUser",steamNull},{"SteamUserStats",steamNull},{"SteamRemoteStorage",steamNull},{"SteamUtils",steamNull}};
const std::map<std::string,recomp_func_t> mgrXInput={{"#2",xinputGetState},{"#3",xinputSetState},{"#4",xinputCaps},
    {"XInputGetState",xinputGetState},{"XInputSetState",xinputSetState},{"XInputGetCapabilities",xinputCaps}};
const std::map<std::string,recomp_func_t> mgrOle={{"CoInitializeEx",coInitialize},{"CoUninitialize",coUninitialize},{"OleInitialize",oleInitialize},
    {"OleUninitialize",oleUninitialize},{"CoCreateInstance",coCreateInstance},{"CreateStreamOnHGlobal",createStreamOnHGlobal}};
const std::map<std::string,recomp_func_t> mgrOlePro={{"#251",olePro32}};
const std::map<std::string,recomp_func_t> mgrGdi={{"GetObjectA",gdiGetObject},{"GetDeviceCaps",gdiDeviceCaps}};
const std::map<std::string,recomp_func_t> mgrNone;
recomp_func_t ws2ByOrdinal(const std::string& name){
    if(name.empty()||name[0]!='#')return nullptr;auto o=ws2Ordinals.find(uint32_t(std::stoul(name.substr(1))));if(o==ws2Ordinals.end())return nullptr;
    auto f=networkFunctions.find(o->second);return f==networkFunctions.end()?nullptr:f->second;
}
const std::map<std::string,recomp_func_t>& mgrOtherFunctions(const std::string& dll){
    if(dll=="steam_api.dll")return mgrSteam;if(dll=="xinput1_3.dll")return mgrXInput;if(dll=="ole32.dll")return mgrOle;
    if(dll=="olepro32.dll")return mgrOlePro;if(dll=="gdi32.dll")return mgrGdi;if(dll=="d3dx9_43.dll")return d3dx9Functions;return mgrNone;
}
recomp_func_t mgrLookup(const std::string& dll,const std::string& name){
    if(dll=="ws2_32.dll")if(auto f=ws2ByOrdinal(name))return f;
    const auto& table=dll=="kernel32.dll"?mgrKernelFunctions:dll=="user32.dll"?mgrWindowFunctions:mgrOtherFunctions(dll);
    auto found=table.find(name);return found==table.end()?nullptr:found->second;
}
