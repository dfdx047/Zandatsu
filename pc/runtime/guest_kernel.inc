// Included inside the portable runtime namespace: all pointers use ptr(),
// every return follows the 32-bit stdcall stack contract.
uint32_t guestCommandLine{}, guestEnvironmentA{}, guestEnvironmentW{}, exceptionFilter{};
uint32_t processErrorMode{};
void setErrorMode(){uint32_t old=processErrorMode;processErrorMode=arg(0);ret(old,1);}
std::array<bool,1088> tlsUsed{};
thread_local std::array<uint32_t,1088> tlsValues{};
std::map<uint32_t,std::shared_ptr<std::recursive_mutex>> criticalSections;
std::map<uint32_t,bool> pendingCriticalDeletes;
uint32_t ansiString(const std::string& value) {
    auto address=allocate(static_cast<uint32_t>(value.size()+1));
    std::memcpy(ptr(address,value.size()+1),value.c_str(),value.size()+1); return address;
}
std::vector<uint16_t> wideString(uint32_t address) {
    std::vector<uint16_t> result;
    for(uint32_t i=0;i<32768;++i) {
        uint16_t c; std::memcpy(&c,ptr(address+i*2,2),2);
        if(!c) return result;
        result.push_back(c);
    }
    throw GuestStop("Unterminated UTF16 string");
}
uint32_t utf16String(const std::string& ascii) {
    uint32_t out=allocate(static_cast<uint32_t>((ascii.size()+1)*2));
    for(uint32_t i=0;i<ascii.size();++i) { uint16_t c=static_cast<unsigned char>(ascii[i]); std::memcpy(ptr(out+i*2,2),&c,2); }
    return out;
}
void getLastError() { ret(lastError,0); }
void setLastError() { lastError=arg(0); if(g_fs_base) write32(g_fs_base+0x34,lastError); ret(0,1); }
void tlsAlloc() {
    for(uint32_t i=0;i<tlsUsed.size();++i) if(!tlsUsed[i]) {tlsUsed[i]=true; tlsValues[i]=0; ret(i,0); return;}
    lastError=259; ret(UINT32_MAX,0);
}
void tlsFree() { uint32_t i=arg(0); bool ok=i<tlsUsed.size() && tlsUsed[i]; if(ok){tlsUsed[i]=false; tlsValues[i]=0;} else lastError=87; ret(ok,1); }
void tlsGet() {uint32_t i=arg(0); if(i>=tlsUsed.size()||!tlsUsed[i]){lastError=87;ret(0,1);}else{lastError=0;ret(tlsValues[i],1);}}
void tlsSet() {uint32_t i=arg(0); if(i>=tlsUsed.size()||!tlsUsed[i]){lastError=87;ret(0,2);}else{tlsValues[i]=arg(1);ret(1,2);}}
// The table of guest critical sections has its own lock: in MT mode Enter/LeaveCriticalSection run without the machine
// (~45k calls/s in MGR's scenes), while Initialize/Delete still come through the machine.
std::mutex criticalTableMutex;
void initCritical(uint32_t address,uint32_t spin) {
    {std::lock_guard<std::mutex> table(criticalTableMutex);
    if(criticalSections.count(address)&&read32(address+8))throw GuestStop("Reinitializing active critical section");
    pendingCriticalDeletes.erase(address);
    criticalSections[address]=std::make_shared<std::recursive_mutex>();}
    std::memset(ptr(address,24),0,24); write32(address+4,UINT32_MAX); write32(address+20,spin);
}
void initializeCritical() {initCritical(arg(0),0);ret(0,1);}
void initializeCriticalSpin() {initCritical(arg(0),arg(1));ret(1,2);}
std::shared_ptr<std::recursive_mutex> findCritical(uint32_t address){
    std::lock_guard<std::mutex> table(criticalTableMutex);auto cs=criticalSections.find(address);
    return cs==criticalSections.end()?nullptr:cs->second;
}
void enterCritical() {
    uint32_t address=arg(0); auto mutex=findCritical(address);
    if(!mutex) throw GuestStop("EnterCriticalSection without initialization");
    // Contended: hand the machine over (the fair lock gives it to the waiting
    // owner) instead of sleeping 1 ms per attempt; sleep only if it persists.
    if(!mutex->try_lock()){
        WaitScope scope("EnterCriticalSection",address);
        if(mtMode()){
            // MT: the owner runs without the machine, so sleep on the mutex itself (futex), never holding the machine.
            {auto& trace=currentTrace();trace.func=g_cur_func;trace.esp=g_esp;}
            if(machine.ownedByMe()){
                if(!machineLease||!machineLease->owns_lock())throw GuestStop("Guest scheduler ownership missing");
                State saved;saved.save();machineLease->unlock();
                mutex->lock();
                machineLease->lock();saved.load();
            }else mutex->lock();
        }else
        for(unsigned spins=0;!mutex->try_lock();++spins){checkGuestProgress();yieldGuestMachine(spins<64?0:1);}
    }
    if(findCritical(address)!=mutex){mutex->unlock();throw GuestStop("Critical section deleted while waiting");}
    auto count=read32(address+8)+1; write32(address+8,count); write32(address+4,count-1); write32(address+12,guestThreadId);
    if(count==1)currentTrace().hold(address);
    ret(0,1);
}
void leaveCritical() {
    uint32_t address=arg(0); auto mutex=findCritical(address); uint32_t count=read32(address+8);
    if(!mutex||!count||read32(address+12)!=guestThreadId) throw GuestStop("Unbalanced LeaveCriticalSection");
    write32(address+8,--count);write32(address+4,count-1);if(!count){write32(address+12,0);currentTrace().release(address);}
    if(!count){std::lock_guard<std::mutex> table(criticalTableMutex);if(pendingCriticalDeletes.erase(address))criticalSections.erase(address);}
    mutex->unlock();
    ret(0,1);
}
void deleteCritical() {
    uint32_t address=arg(0);
    std::lock_guard<std::mutex> table(criticalTableMutex);
    if(read32(address+8)){
        NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Deferring deletion of active guest critical section %08x owner=%u count=%u",address,read32(address+12),read32(address+8));
        pendingCriticalDeletes[address]=true;
    }else{criticalSections.erase(address);pendingCriticalDeletes.erase(address);}
    ret(0,1);
}
void startupInfo() {std::memset(ptr(arg(0),68),0,68);write32(arg(0),68);ret(0,1);}
void setHandleCount() {ret(arg(0),1);}
void commandLine() {if(!guestCommandLine)guestCommandLine=ansiString("\"C:\\nfsu2\\METAL GEAR RISING REVENGEANCE.exe\"");ret(guestCommandLine,0);}
void moduleHandleW() {
    if(!arg(0)){ret(imageBase,1);return;}
    auto chars=wideString(arg(0));std::string name;
    for(auto c:chars){if(c>127){lastError=126;ret(0,1);return;}name.push_back(static_cast<char>(c));}
    name=lower(name);if(name=="metal gear rising revengeance.exe"){ret(imageBase,1);return;}
    if(name.find('.')==std::string::npos)name+=".dll";
    auto found=modules.find(name);if(found==modules.end())lastError=126;ret(found==modules.end()?0:found->second,1);
}
std::map<uint32_t,uint32_t> moduleReferences;
void loadLibrary(){
    auto name=lower(guestString(arg(0)));auto slash=name.find_last_of("/\\");if(slash!=std::string::npos)name=name.substr(slash+1);if(name.find('.')==std::string::npos)name+=".dll";
    static std::set<std::string> loggedLoads;
    if(loggedLoads.insert(name).second)
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Guest LoadLibrary %s",name.c_str());
    auto module=modules.find(name);
    if(module==modules.end()){uint32_t loaded=loadGuestModule(name);if(!loaded)lastError=126;ret(loaded,1);return;}
    auto& count=moduleReferences[module->second];if(!count)count=1;++count;ret(module->second,1);
}
void freeLibrary(){auto module=std::find_if(modules.begin(),modules.end(),[](const auto& item){return item.second==arg(0);});
    if(module==modules.end()){if(freeGuestModule(arg(0))){ret(1,1);return;}lastError=6;ret(0,1);return;}
    auto found=moduleReferences.find(arg(0));if(found!=moduleReferences.end()&&found->second>1)--found->second;ret(1,1);}
void environmentA() {
    if(!guestEnvironmentA) {std::string s="PATH=C:\\nfsu2";s.push_back(0);s+="TEMP=C:\\nfsu2";s.push_back(0);s.push_back(0);guestEnvironmentA=ansiString(s);}
    ret(guestEnvironmentA,0);
}
void environmentW() {
    if(!guestEnvironmentW) {std::string s="PATH=C:\\nfsu2";s.push_back(0);s+="TEMP=C:\\nfsu2";s.push_back(0);s.push_back(0);guestEnvironmentW=utf16String(s);}
    ret(guestEnvironmentW,0);
}
void freeEnvironmentA() {bool ok=arg(0)==guestEnvironmentA && guestEnvironmentA;if(ok){heaps->free(GuestHeaps::process,guestEnvironmentA);guestEnvironmentA=0;}ret(ok,1);}
void freeEnvironmentW() {bool ok=arg(0)==guestEnvironmentW && guestEnvironmentW;if(ok){heaps->free(GuestHeaps::process,guestEnvironmentW);guestEnvironmentW=0;}ret(ok,1);}
void moduleFileName() {
    uint32_t module=arg(0),out=arg(1),capacity=arg(2);
    if(module && module!=imageBase) {lastError=126;ret(0,3);return;}
    const std::string path="C:\\nfsu2\\METAL GEAR RISING REVENGEANCE.exe";
    uint32_t count=std::min(capacity,static_cast<uint32_t>(path.size()));
    if(count)std::memcpy(ptr(out,count),path.data(),count);
    if(capacity>count)*static_cast<char*>(ptr(out+count,1))=0;else lastError=122;
    ret(count,3);
}
void currentDirectory() {const std::string path=guestWorkingDirectory; uint32_t cap=arg(0),out=arg(1);if(cap<=path.size())ret(static_cast<uint32_t>(path.size()+1),2);else{std::memcpy(ptr(out,path.size()+1),path.c_str(),path.size()+1);ret(static_cast<uint32_t>(path.size()),2);}}
void stdHandle() {uint32_t n=arg(0);ret(n>=0xfffffff4u&&n<=0xfffffff6u?0xf0000200u+(0xfffffff6u-n)*4:UINT32_MAX,1);}
void consoleMode() {lastError=6;ret(0,2);}
void currentProcess() {ret(UINT32_MAX,0);}
void currentThread() {ret(0xfffffffeu,0);}
void processId() {ret(static_cast<uint32_t>(getpid()),0);}
void threadId() {ret(guestThreadId,0);}
void version() {ret(5u|(1u<<8)|(2600u<<16),0);}
void isDebugger() {ret(0,0);}
void processorFeature() {
    // Match the virtual x86 CPUID supplied to lifted code, not the ARM host.
    uint32_t feature=arg(0);
    ret(feature==2||feature==3||feature==6||feature==8||feature==10,1);
}
void setExceptionFilter() {uint32_t old=exceptionFilter;exceptionFilter=arg(0);ret(old,1);}
void encodePointer() {ret(arg(0)^0x51c39a71u,1);}
void getACP() {ret(1252,0);}
void getOEMCP() {ret(437,0);}
void threadLocale() {ret(0x409,0);}
void cpInfo() {
    uint32_t cp=arg(0);if(cp!=0&&cp!=1252&&cp!=437&&cp!=65001){lastError=87;ret(0,2);return;}
    uint32_t out=arg(1);std::memset(ptr(out,20),0,20);write32(out,cp==65001?4:1);*static_cast<char*>(ptr(out+4,1))='?';ret(1,2);
}
// Lab MGR_QPC_SCALE=<f>: the guest clock runs f times real time (does the game's logic follow the clock or the frame count?).
uint64_t counter() {
    uint64_t now=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    static const double scale=[]{const char* s=std::getenv("MGR_QPC_SCALE");return s?std::atof(s):1.0;}();
    static const uint64_t origin=now;
    return scale==1.0?now:origin+static_cast<uint64_t>(double(now-origin)*scale);
}
void tickCount() {ret(static_cast<uint32_t>(counter()/1000000),0);}
void performanceCounter() {uint64_t now=counter();std::memcpy(ptr(arg(0),8),&now,8);ret(1,1);}
void performanceFrequency() {uint64_t frequency=1000000000;std::memcpy(ptr(arg(0),8),&frequency,8);ret(1,1);}
void systemFileTime() {uint64_t ticks=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count()/100)+116444736000000000ull;std::memcpy(ptr(arg(0),8),&ticks,8);ret(0,1);}
void fileTimeToLocalFileTime(){
    uint32_t input=arg(0),output=arg(1);
    if(input==output||!memory->readable(input,8)||!memory->writable(output,8)){lastError=87;ret(0,2);return;}
    // GetTimeZoneInformation exposes UTC for the virtual Windows machine.
    std::memcpy(ptr(output,8),ptr(input,8),8);lastError=0;ret(1,2);
}
void fileTimeToSystemTime(){
    uint32_t input=arg(0),output=arg(1);
    if(!memory->readable(input,8)||!memory->writable(output,16)){lastError=87;ret(0,2);return;}
    uint64_t ticks;std::memcpy(&ticks,ptr(input,8),8);
    if(ticks>=0x8000000000000000ull){lastError=87;ret(0,2);return;}
    int64_t seconds=static_cast<int64_t>(ticks/10000000ull)-11644473600ll;
    time_t unixTime=static_cast<time_t>(seconds);std::tm utc{};
    if(!gmtime_r(&unixTime,&utc)||utc.tm_year+1900>65535){lastError=87;ret(0,2);return;}
    uint16_t fields[]={uint16_t(utc.tm_year+1900),uint16_t(utc.tm_mon+1),uint16_t(utc.tm_wday),
        uint16_t(utc.tm_mday),uint16_t(utc.tm_hour),uint16_t(utc.tm_min),uint16_t(utc.tm_sec),
        uint16_t((ticks%10000000ull)/10000ull)};
    std::memcpy(ptr(output,16),fields,16);lastError=0;ret(1,2);
}
void systemInfo() {uint32_t out=arg(0);std::memset(ptr(out,36),0,36);write32(out+4,4096);write32(out+8,0x10000);write32(out+12,0x7ffeffff);write32(out+16,1);write32(out+20,1);write32(out+24,586);write32(out+28,65536);uint16_t level=6;std::memcpy(ptr(out+32,2),&level,2);ret(0,1);}
void sleepShim() {
    uint32_t ms=arg(0);if(ms==UINT32_MAX)throw GuestStop("Infinite Sleep requires guest scheduler");
    WaitScope scope("Sleep",ms);
    while(ms){checkGuestProgress();auto chunk=std::min(ms,50u);yieldGuestMachine(chunk);ms-=chunk;}
    ret(0,1);
}
void sleepEx(){
    uint32_t ms=arg(0);if(!ms){checkGuestProgress();ret(0,2);return;}
    while(ms){checkGuestProgress();auto chunk=std::min(ms,50u);yieldGuestMachine(chunk);if(ms!=UINT32_MAX)ms-=chunk;}
    ret(0,2);
}
void exitProcess() {throw GuestStop("Guest requested ExitProcess("+std::to_string(arg(0))+") from "+hex(g_cur_func));}
void debugString() {NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Guest: %s",guestString(arg(0)).c_str());ret(0,1);}
void deleteGdiObject(){lastError=6;ret(0,1);} // No GDI-created objects are registered yet.
std::map<uint32_t,uint32_t> timerPeriods;
void timeCaps(){if(!arg(0)||arg(1)<8){ret(97,2);return;}write32(arg(0),1);write32(arg(0)+4,1000000);ret(0,2);}
void timeBegin(){uint32_t period=arg(0);if(!period||period>1000000){ret(97,1);return;}++timerPeriods[period];ret(0,1);}
void timeEnd(){auto period=timerPeriods.find(arg(0));if(period==timerPeriods.end()){ret(97,1);return;}if(!--period->second)timerPeriods.erase(period);ret(0,1);}
const std::map<std::string,recomp_func_t> timerFunctions={
    {"timeSetEvent",timeSetEvent},{"timeKillEvent",timeKillEvent},
    {"timeGetDevCaps",timeCaps},{"timeBeginPeriod",timeBegin},{"timeEndPeriod",timeEnd},{"timeGetTime",tickCount}
};
struct ModuleSnapshot {uint32_t flags;bool consumed=false,processConsumed=false;};
std::map<uint32_t,ModuleSnapshot> moduleSnapshots;
uint32_t nextSnapshot=0xf0010000;
std::map<uint32_t,uint32_t> kernelHandleAliases;
uint32_t nextKernelHandle=0xf0020000;
uint32_t guestPriorityClass=0x20;
bool processHandle(uint32_t handle){auto alias=kernelHandleAliases.find(handle);return handle==UINT32_MAX||(alias!=kernelHandleAliases.end()&&alias->second==UINT32_MAX);}
void getPriorityClass(){if(!processHandle(arg(0))){lastError=6;ret(0,1);return;}ret(guestPriorityClass,1);}
void isBadReadPtr(){ret(memory->readable(arg(0),arg(1))?0:1,2);}
void isBadWritePtr(){ret(memory->writable(arg(0),arg(1))?0:1,2);}
void virtualProtect(){
    uint32_t output=arg(3),old=0,flags=arg(2);
    if(!output||!memory->writable(output,4)){lastError=87;ret(0,4);return;}
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Guest VirtualProtect address=%08x bytes=%u protection=%x",arg(0),arg(1),flags);
    if(flags&~0xffu)throw GuestStop("VirtualProtect modifiers pending: "+hex(flags));
    if(!memory->protect(arg(0),arg(1),flags,old)){lastError=87;ret(0,4);return;}
    if(!memory->writable(output,4))throw GuestStop("VirtualProtect old-protection output became inaccessible");
    write32(output,old);ret(1,4);
}
void getTimeZoneInformation(){
    uint32_t output=arg(0);if(!output){lastError=87;ret(UINT32_MAX,1);return;}
    // The virtual Windows machine uses UTC with no daylight-saving transition.
    std::memset(ptr(output,172),0,172);
    const uint16_t name[]={u'U',u'T',u'C',0};
    std::memcpy(ptr(output+4,sizeof name),name,sizeof name);
    std::memcpy(ptr(output+88,sizeof name),name,sizeof name);ret(0,1);
}
void getProcessAffinityMask(){
    if(!processHandle(arg(0))){lastError=6;ret(0,3);return;}
    if(!arg(1)||!arg(2)){lastError=87;ret(0,3);return;}
    // The serialized guest machine exposes one virtual x86 processor.
    write32(arg(1),1);write32(arg(2),1);ret(1,3);
}
void setProcessAffinityMask(){
    if(!processHandle(arg(0))){lastError=6;ret(0,2);return;}
    if(arg(1)!=1){lastError=87;ret(0,2);return;}ret(1,2);
}
void setPriorityClass(){
    if(!processHandle(arg(0))){lastError=6;ret(0,2);return;}uint32_t priority=arg(1);
    if(priority!=0x20&&priority!=0x40&&priority!=0x80&&priority!=0x100&&priority!=0x4000&&priority!=0x8000){lastError=87;ret(0,2);return;}
    guestPriorityClass=priority;ret(1,2);
}
void getThreadPriority(){
    auto thread=threadHandles.find(arg(0));if(thread!=threadHandles.end()){ret(thread->second->priority,1);return;}
    auto alias=kernelHandleAliases.find(arg(0));if(arg(0)==UINT32_MAX-1||(alias!=kernelHandleAliases.end()&&alias->second==UINT32_MAX-1)){ret(0,1);return;}
    lastError=6;ret(0x7fffffff,1);
}
void duplicateHandle(){
    uint32_t source=arg(1),out=arg(3);
    if(arg(0)!=UINT32_MAX||arg(2)!=UINT32_MAX||!out){lastError=6;ret(0,7);return;}
    auto alias=kernelHandleAliases.find(source);uint32_t object=alias==kernelHandleAliases.end()?source:alias->second;
    if(object!=UINT32_MAX&&object!=UINT32_MAX-1){lastError=6;ret(0,7);return;}
    uint32_t handle=nextKernelHandle++;kernelHandleAliases.emplace(handle,object);write32(out,handle);
    if(arg(6)&1)kernelHandleAliases.erase(source);
    ret(1,7);
}
void toolhelpSnapshot(){
    uint32_t flags=arg(0),pid=arg(1);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Toolhelp snapshot flags=%x pid=%u",flags,pid);
    if((flags&~0x8000001au)||!(flags&0x1a))throw GuestStop("Toolhelp snapshot flags pending: "+hex(flags));
    if(pid&&pid!=1){lastError=87;ret(UINT32_MAX,2);return;}
    uint32_t handle=nextSnapshot++;moduleSnapshots.emplace(handle,ModuleSnapshot{flags});ret(handle,2);
}
void moduleEntry(bool first){
    auto snapshot=moduleSnapshots.find(arg(0));
    if(snapshot==moduleSnapshots.end()){lastError=6;ret(0,2);return;}
    if(!(snapshot->second.flags&0x18)){lastError=18;ret(0,2);return;}
    if(first)snapshot->second.consumed=false;
    if(snapshot->second.consumed){lastError=18;ret(0,2);return;}
    uint32_t out=arg(1);if(read32(out)<548){lastError=24;ret(0,2);return;}
    std::memset(ptr(out,548),0,548);write32(out,548);write32(out+4,1);write32(out+8,1);
    write32(out+12,1);write32(out+16,1);write32(out+20,imageBase);write32(out+24,imageSize);write32(out+28,imageBase);
    const char name[]="METAL GEAR RISING REVENGEANCE.exe",path[]="C:\\nfsu2\\METAL GEAR RISING REVENGEANCE.exe";
    std::memcpy(ptr(out+32,sizeof name),name,sizeof name);std::memcpy(ptr(out+288,sizeof path),path,sizeof path);
    snapshot->second.consumed=true;ret(1,2);
}
void moduleFirst(){moduleEntry(true);}void moduleNext(){moduleEntry(false);}
void processEntry(bool first){
    auto snapshot=moduleSnapshots.find(arg(0));
    if(snapshot==moduleSnapshots.end()){lastError=6;ret(0,2);return;}
    if(first)snapshot->second.processConsumed=false;
    if(!(snapshot->second.flags&2)||snapshot->second.processConsumed){lastError=18;ret(0,2);return;}
    uint32_t out=arg(1);if(read32(out)<296){lastError=24;ret(0,2);return;}
    std::memset(ptr(out,296),0,296);write32(out,296);write32(out+8,1);write32(out+20,1);write32(out+28,8);
    const char name[]="METAL GEAR RISING REVENGEANCE.exe";std::memcpy(ptr(out+36,sizeof name),name,sizeof name);
    snapshot->second.processConsumed=true;ret(1,2);
}
void processFirst(){processEntry(true);}void processNext(){processEntry(false);}
bool closeMapping(uint32_t handle);
bool closeSyncHandle(uint32_t h){std::lock_guard<std::mutex> sync(guestSyncMutex);return threadHandles.erase(h)||eventHandles.erase(h)||mutexHandles.erase(h)||semaphoreHandles.erase(h);}
void closeHandle(){if(closeGuestFile(arg(0))||moduleSnapshots.erase(arg(0))||kernelHandleAliases.erase(arg(0))||closeSyncHandle(arg(0))||closeMapping(arg(0))){ret(1,1);return;}lastError=6;ret(0,1);}
struct RegistryValue {uint32_t type;std::vector<uint8_t> bytes;};
std::map<std::string,RegistryValue> gameRegistry;
// The game keeps its video options in the registry (g_CarLodLevel,
// g_RoadReflectionEnable, ...): Windows keeps them across launches, so they
// are stored in the game folder. The launcher owns the install path and the
// language; the render resolution and antialiasing are not restored, so a
// value that crashes a GPU cannot crash every following start.
std::string registryFile(){return hostGameRoot+"/android_registry.dat";}
bool registryPersisted(const std::string& name){
    return name!="install dir"&&name!="language"&&name!="g_racingresolution"&&name!="g_fsaalevel";
}
void loadPersistedRegistry(){
    std::ifstream file(registryFile(),std::ios::binary);if(!file)return;
    auto read=[&](void* data,size_t bytes){return bool(file.read(static_cast<char*>(data),std::streamsize(bytes)));};
    uint32_t count=0,restored=0;if(!read(&count,4)||count>4096)return;
    for(uint32_t i=0;i<count;++i){
        uint32_t nameBytes=0,type=0,size=0;
        if(!read(&nameBytes,4)||nameBytes==0||nameBytes>256)return;
        std::string name(nameBytes,'\0');if(!read(name.data(),nameBytes)||!read(&type,4)||!read(&size,4)||size>65536)return;
        RegistryValue value{type,std::vector<uint8_t>(size)};if(size&&!read(value.bytes.data(),size))return;
        if(registryPersisted(name)){gameRegistry[name]=std::move(value);++restored;}
    }
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFS","Registry: restored %u saved game settings",restored);
}
void savePersistedRegistry(){
    std::string target=registryFile(),temporary=target+".tmp";
    {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);if(!file)return;
        uint32_t count=0;for(auto& [name,value]:gameRegistry)if(registryPersisted(name))++count;
        file.write(reinterpret_cast<const char*>(&count),4);
        for(auto& [name,value]:gameRegistry){if(!registryPersisted(name))continue;
            uint32_t nameBytes=uint32_t(name.size()),size=uint32_t(value.bytes.size());
            file.write(reinterpret_cast<const char*>(&nameBytes),4);file.write(name.data(),nameBytes);
            file.write(reinterpret_cast<const char*>(&value.type),4);file.write(reinterpret_cast<const char*>(&size),4);
            if(size)file.write(reinterpret_cast<const char*>(value.bytes.data()),size);}
        if(!file)return;}
    std::rename(temporary.c_str(),target.c_str());
}
std::map<uint32_t,bool> registryHandles;
uint32_t nextRegistryHandle=0xf0050000;
void registryOpen(uint32_t output,uint32_t argc){
    auto key=lower(guestString(arg(1)));
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Registry open %08x\\%s",arg(0),key.c_str());
    if(arg(0)!=0x80000002u||(key!="software\\ea games\\need for speed underground 2"&&key!="software\\electronic arts\\ea games\\need for speed underground 2")){
        if(output)write32(output,0);ret(2,argc);return;
    }
    if(gameRegistry.empty()){
        const char path[]="C:\\nfsu2\\";gameRegistry.emplace("install dir",RegistryValue{1,{std::begin(path),std::end(path)}});
        auto language=guestLanguage;language.push_back('\0');gameRegistry.emplace("language",RegistryValue{1,{language.begin(),language.end()}});
        loadPersistedRegistry();
    }
    uint32_t handle=nextRegistryHandle++;registryHandles.emplace(handle,true);write32(output,handle);ret(0,argc);
}
void regOpenKeyA(){registryOpen(arg(2),3);}void regOpenKeyExA(){registryOpen(arg(4),5);}
void regCloseKey(){ret(registryHandles.erase(arg(0))?0:6,1);}
void regQueryValueExA(){
    auto name=lower(guestString(arg(1)));NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Registry query %08x value=%s",arg(0),name.c_str());
    if(!registryHandles.count(arg(0))){ret(6,6);return;}
    auto value=gameRegistry.find(name);if(value==gameRegistry.end()){ret(2,6);return;}
    if(!arg(5)){ret(87,6);return;}uint32_t size=read32(arg(5));write32(arg(5),value->second.bytes.size());
    if(arg(3))write32(arg(3),value->second.type);
    if(arg(4)){if(size<value->second.bytes.size()){ret(234,6);return;}std::memcpy(ptr(arg(4),value->second.bytes.size()),value->second.bytes.data(),value->second.bytes.size());}
    ret(0,6);
}
void regSetValueExA(){
    if(!registryHandles.count(arg(0))){ret(6,6);return;}
    uint32_t size=arg(5);if(size>1048576){ret(87,6);return;}
    RegistryValue value{arg(3),std::vector<uint8_t>(size)};if(size)std::memcpy(value.bytes.data(),ptr(arg(4),size),size);
    auto name=lower(guestString(arg(1)));gameRegistry[name]=std::move(value);
    if(registryPersisted(name))savePersistedRegistry();
    ret(0,6);
}
#include "guest_charset.inc"
#include "guest_resources.inc"
#include "guest_window.inc"
#include "guest_format.inc"
static constexpr uint32_t desktopWindow=0xf0000300;
void desktopHandle(){ret(desktopWindow,0);}
void windowRect(){
    int32_t rectangle[4]={0,0,static_cast<int32_t>(displayWidth.load()),static_cast<int32_t>(displayHeight.load())};
    if(arg(0)!=desktopWindow){auto window=windows.find(arg(0));if(window==windows.end()){lastError=1400;ret(0,2);return;}auto& value=window->second;rectangle[0]=value.x;rectangle[1]=value.y;rectangle[2]=value.x+value.width;rectangle[3]=value.y+value.height;}
    std::memcpy(ptr(arg(1),16),rectangle,16);ret(1,2);
}
// Screen position of the touch pointer (the game maps it through GetWindowRect/GetClientRect).
void cursorPosition(){
    auto point=pointerInWindow();int32_t screen[2]={point.first,point.second};
    if(auto window=windows.find(activeWindow);window!=windows.end()){screen[0]+=window->second.x;screen[1]+=window->second.y;}
    std::memcpy(ptr(arg(0),8),screen,8);ret(1,1);
}
void clientRect(){auto window=windows.find(arg(0));if(window==windows.end()){lastError=1400;ret(0,2);return;}int32_t rect[4]={0,0,window->second.width,window->second.height};std::memcpy(ptr(arg(1),16),rect,16);ret(1,2);}
void loadIcon(){lastError=1813;ret(0,2);} // PE icon resources are not materialized as Android objects.
void loadCursor(){
    uint32_t id=arg(1);
    if(arg(0)==0&&id>=32512&&id<=32651){ret(0xf0000400+id,2);return;}
    lastError=1813;ret(0,2);
}
void systemMetrics(){
    uint32_t index=arg(0),value;
    switch(index){
    case 0:case 16:case 59:case 61:case 78:value=displayWidth.load();break;
    case 1:case 17:case 60:case 62:case 79:value=displayHeight.load();break;
    case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 32:case 33:case 76:case 77:value=0;break;
    case 19:case 23:case 43:value=0;break; // No physical mouse declared by the Android host yet.
    case 80:value=1;break;
    default:throw GuestStop("GetSystemMetrics index pending: "+std::to_string(index));
    }
    ret(value,1);
}
// MessageBoxA(hwnd, text, caption, type): no dialog on Android. The text goes to
// the log (tester reports) and the box answers its default button.
void messageBox(){
    auto text=guestString(arg(1)),caption=guestString(arg(2));uint32_t type=arg(3)&0xf;
    NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Guest MessageBoxA from %08x: [%s] %s",g_cur_func,caption.c_str(),text.c_str());
    // MB_OK/OKCANCEL -> IDOK, ABORTRETRYIGNORE -> IDIGNORE, YESNO(CANCEL) -> IDYES,
    // RETRYCANCEL -> IDCANCEL, CANCELTRYCONTINUE -> IDCONTINUE.
    static constexpr uint32_t answers[]={1,1,5,6,6,2,11};
    ret(type<7?answers[type]:1,4);
}
const std::map<std::string,recomp_func_t> windowFunctions={
    {"MessageBoxA",messageBox},
    {"PeekMessageA",peekMessage},{"GetMessageA",getMessage},{"TranslateMessage",translateMessage},{"DispatchMessageA",dispatchMessage},
    {"PostMessageA",postMessage},{"PostQuitMessage",postQuitMessage},{"SendMessageA",sendMessage},
    {"wvsprintfA",winFormatA},
    {"SetCursor",setCursor},{"ShowCursor",showCursor},{"GetActiveWindow",getActiveWindow},{"GetForegroundWindow",getActiveWindow},{"SetActiveWindow",setActiveWindow},
    {"SetFocus",setActiveWindow},{"GetFocus",getActiveWindow},
    {"SetForegroundWindow",setForegroundWindow},
    {"BeginPaint",beginPaint},{"EndPaint",endPaint},
    {"AdjustWindowRect",adjustWindowRect},{"ShowWindow",showWindow},{"SetWindowPos",setWindowPos},{"UpdateWindow",updateWindow},
    {"RegisterClassExA",registerClassEx},{"CreateWindowExA",createWindowEx},{"DefWindowProcA",defaultWindowProc},
    {"GetWindowLongA",getWindowLong},{"SetWindowLongA",setWindowLong},
    {"LoadIconA",loadIcon},{"LoadCursorA",loadCursor},
    {"GetDesktopWindow",desktopHandle},{"GetWindowRect",windowRect},
    {"SetCapture",setCapture},{"ReleaseCapture",releaseCapture},
    {"GetClientRect",clientRect},{"GetCursorPos",cursorPosition},{"GetSystemMetrics",systemMetrics}
};
// Guest threads run one at a time under the machine lock, so plain accesses are atomic to them.
// Host atomics on guest memory: with MGR_MT=1 lifted `lock cmpxchg`/`lock xadd` on other threads race with these.
uint32_t* atomicGuest32(uint32_t va){return static_cast<uint32_t*>(ptr(va,4));}
void interlockedExchange(){ret(__atomic_exchange_n(atomicGuest32(arg(0)),arg(1),__ATOMIC_SEQ_CST),2);}
void interlockedIncrement(){ret(__atomic_add_fetch(atomicGuest32(arg(0)),1u,__ATOMIC_SEQ_CST),1);}
void interlockedDecrement(){ret(__atomic_sub_fetch(atomicGuest32(arg(0)),1u,__ATOMIC_SEQ_CST),1);}
void interlockedCompareExchange(){uint32_t expect=arg(2);__atomic_compare_exchange_n(atomicGuest32(arg(0)),&expect,arg(1),false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);ret(expect,3);}
const std::map<std::string,recomp_func_t> kernelFunctions={
    {"IsBadReadPtr",isBadReadPtr},{"IsBadWritePtr",isBadWritePtr},{"VirtualProtect",virtualProtect},
    {"CreateMutexA",createMutex},{"ReleaseMutex",releaseMutex},
    {"GetTimeZoneInformation",getTimeZoneInformation},
    {"GetProcessAffinityMask",getProcessAffinityMask},{"SetProcessAffinityMask",setProcessAffinityMask},
    {"GetPriorityClass",getPriorityClass},{"SetPriorityClass",setPriorityClass},{"GetThreadPriority",getThreadPriority},
    {"LoadLibraryA",loadLibrary},{"FreeLibrary",freeLibrary},
    {"SetErrorMode",setErrorMode},
    {"CreateEventA",createEvent},{"SetEvent",setEvent},{"ResetEvent",resetEvent},
    {"CreateThread",createThread},{"ResumeThread",resumeThread},{"GetExitCodeThread",threadExitCode},{"WaitForSingleObject",waitThread},{"WaitForMultipleObjects",waitMultiple},{"ExitThread",exitThread},
    {"InterlockedExchange",interlockedExchange},{"InterlockedIncrement",interlockedIncrement},{"InterlockedDecrement",interlockedDecrement},{"InterlockedCompareExchange",interlockedCompareExchange},{"SetThreadPriority",setThreadPriority},
    {"CreateToolhelp32Snapshot",toolhelpSnapshot},{"Module32First",moduleFirst},{"Module32Next",moduleNext},{"CloseHandle",closeHandle},
    {"Process32First",processFirst},{"Process32Next",processNext},
    {"DuplicateHandle",duplicateHandle},
    {"GetProcAddress",getProcAddress},{"GetModuleHandleW",moduleHandleW},{"GetLastError",getLastError},{"SetLastError",setLastError},
    {"TlsAlloc",tlsAlloc},{"TlsFree",tlsFree},{"TlsGetValue",tlsGet},{"TlsSetValue",tlsSet},
    {"InitializeCriticalSection",initializeCritical},{"InitializeCriticalSectionAndSpinCount",initializeCriticalSpin},
    {"EnterCriticalSection",enterCritical},{"LeaveCriticalSection",leaveCritical},{"DeleteCriticalSection",deleteCritical},
    {"GetStartupInfoA",startupInfo},{"SetHandleCount",setHandleCount},{"GetCommandLineA",commandLine},{"GetEnvironmentStrings",environmentA},
    {"GetEnvironmentStringsA",environmentA},{"GetEnvironmentStringsW",environmentW},
    {"FreeEnvironmentStringsA",freeEnvironmentA},{"FreeEnvironmentStringsW",freeEnvironmentW},
    {"GetModuleFileNameA",moduleFileName},{"GetCurrentDirectoryA",currentDirectory},
    {"GetStdHandle",stdHandle},{"GetFileType",fileType},{"GetConsoleMode",consoleMode},
    {"GetCurrentProcess",currentProcess},{"GetCurrentThread",currentThread},{"GetCurrentProcessId",processId},
    {"GetCurrentThreadId",threadId},{"GetVersion",version},{"IsDebuggerPresent",isDebugger},{"IsProcessorFeaturePresent",processorFeature},
    {"SetUnhandledExceptionFilter",setExceptionFilter},{"EncodePointer",encodePointer},{"DecodePointer",encodePointer},
    {"GetACP",getACP},{"GetOEMCP",getOEMCP},{"GetCPInfo",cpInfo},{"GetThreadLocale",threadLocale},
    {"WideCharToMultiByte",wideToMulti},{"MultiByteToWideChar",multiToWide},{"GetStringTypeW",stringTypeW},{"LCMapStringW",mapStringW},
    {"GetTickCount",tickCount},{"QueryPerformanceCounter",performanceCounter},
    {"QueryPerformanceFrequency",performanceFrequency},{"GetSystemTimeAsFileTime",systemFileTime},
    {"FileTimeToLocalFileTime",fileTimeToLocalFileTime},{"FileTimeToSystemTime",fileTimeToSystemTime},
    {"GetSystemInfo",systemInfo},{"Sleep",sleepShim},{"SleepEx",sleepEx},{"ExitProcess",exitProcess},{"OutputDebugStringA",debugString}
};
