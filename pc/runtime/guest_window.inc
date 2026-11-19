struct GuestWindowClass {uint32_t atom,procedure,instance,style,extra;std::string name;};
struct GuestWindow {uint32_t procedure,instance,style,exstyle,parent,user=0,id=0,extra;int32_t x,y,width,height;bool visible;std::map<uint32_t,uint32_t> data;};
std::map<std::string,GuestWindowClass> windowClasses;
std::map<uint32_t,GuestWindow> windows;
uint32_t nextClassAtom=1,nextWindow=0xf0000301;
void registerClassEx(){
    uint32_t value=arg(0);if(!value||read32(value)!=48){lastError=87;ret(0,1);return;}
    auto name=guestString(read32(value+40));uint32_t procedure=read32(value+8);
    if(name.empty()||!recomp_lookup(procedure)){lastError=87;ret(0,1);return;}
    if(windowClasses.count(name)){lastError=1410;ret(0,1);return;}
    GuestWindowClass record{nextClassAtom++,procedure,read32(value+20),read32(value+4),read32(value+16),name};
    uint32_t atom=record.atom;windowClasses.emplace(name,std::move(record));ret(atom,1);
}
uint32_t windowMessage(uint32_t handle,uint32_t message,uint32_t wparam,uint32_t lparam){
    auto window=windows.find(handle);if(window==windows.end())return 0;
    return callGuest(window->second.procedure,{handle,message,wparam,lparam});
}
void createWindowEx(){
    const GuestWindowClass* cls=nullptr;uint32_t name=arg(1);
    if(name<=0xffff){for(auto& record:windowClasses)if(record.second.atom==name){cls=&record.second;break;}}
    else {auto record=windowClasses.find(guestString(name));if(record!=windowClasses.end())cls=&record->second;}
    if(!cls){lastError=1407;ret(0,12);return;}
    int32_t x=arg(4),y=arg(5),width=arg(6),height=arg(7);
    if(static_cast<uint32_t>(x)==0x80000000)x=0;if(static_cast<uint32_t>(y)==0x80000000)y=0;
    if(static_cast<uint32_t>(width)==0x80000000)width=640;if(static_cast<uint32_t>(height)==0x80000000)height=480;
    uint32_t handle=nextWindow++,structure=allocate(48),parameters[12]={arg(11),arg(10),arg(9),arg(8),static_cast<uint32_t>(height),static_cast<uint32_t>(width),static_cast<uint32_t>(y),static_cast<uint32_t>(x),arg(3),arg(2),arg(1),arg(0)};
    windows.emplace(handle,GuestWindow{cls->procedure,arg(10),arg(3),arg(0),arg(8),0,arg(9),cls->extra,x,y,width,height,(arg(3)&0x10000000)!=0,{}});
    std::memcpy(ptr(structure,48),parameters,48);
    if(!windowMessage(handle,0x81,0,structure)||windowMessage(handle,1,0,structure)==UINT32_MAX){
        windows.erase(handle);heaps->free(GuestHeaps::process,structure);lastError=1407;ret(0,12);return;
    }
    heaps->free(GuestHeaps::process,structure);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Guest window %08x class=%s size=%dx%d proc=%08x",handle,cls->name.c_str(),width,height,cls->procedure);
    ret(handle,12);
}
void defaultWindowProc(){ret(arg(1)==0x81||arg(1)==0x14?1:0,4);}
void windowLong(bool set){
    auto window=windows.find(arg(0));if(window==windows.end()){lastError=1400;ret(0,set?3:2);return;}
    auto& value=window->second;int32_t index=arg(1);uint32_t* field=nullptr;
    switch(index){case -4:field=&value.procedure;break;case -6:field=&value.instance;break;case -8:field=&value.parent;break;case -12:field=&value.id;break;case -16:field=&value.style;break;case -20:field=&value.exstyle;break;case -21:field=&value.user;break;
    default:if(index>=0&&static_cast<uint32_t>(index)+4<=value.extra)field=&value.data[index];break;}
    if(!field){lastError=1413;ret(0,set?3:2);return;}uint32_t old=*field;if(set)*field=arg(2);lastError=0;ret(old,set?3:2);
}
void getWindowLong(){windowLong(false);}void setWindowLong(){windowLong(true);}
void adjustWindowRect(){if(!arg(0)){lastError=87;ret(0,3);return;}if(arg(2))throw GuestStop("Android guest window menu sizing pending");ret(1,3);}
void showWindow(){
    auto window=windows.find(arg(0));if(window==windows.end()){lastError=1400;ret(0,2);return;}
    bool previous=window->second.visible;window->second.visible=arg(1)!=0;
    windowMessage(arg(0),0x18,window->second.visible,0);ret(previous,2);
}
void setWindowPos(){
    uint32_t handle=arg(0),flags=arg(6);auto window=windows.find(handle);if(window==windows.end()){lastError=1400;ret(0,7);return;}
    uint32_t structure=allocate(28),values[7]={handle,arg(1),arg(2),arg(3),arg(4),arg(5),flags};std::memcpy(ptr(structure,28),values,28);
    if(!(flags&0x400))windowMessage(handle,0x46,0,structure);
    std::memcpy(values,ptr(structure,28),28);flags=values[6];auto& value=windows.at(handle);
    if(!(flags&2)){value.x=values[2];value.y=values[3];}if(!(flags&1)){value.width=values[4];value.height=values[5];}
    if(flags&0x40)value.visible=true;if(flags&0x80)value.visible=false;
    windowMessage(handle,0x47,0,structure);
    if(!(flags&1))windowMessage(handle,5,0,(uint32_t(value.height)<<16)|(uint32_t(value.width)&0xffff));
    heaps->free(GuestHeaps::process,structure);ret(1,7);
}
void updateWindow(){if(!windows.count(arg(0))){lastError=1400;ret(0,1);return;}windowMessage(arg(0),0xf,0,0);ret(1,1);}
std::map<uint32_t,uint32_t> paintContexts;uint32_t nextPaintContext=0xf0070000;
void beginPaint(){
    auto window=windows.find(arg(0));if(window==windows.end()||!arg(1)){lastError=1400;ret(0,2);return;}
    uint32_t context=nextPaintContext++;paintContexts.emplace(context,arg(0));uint32_t out=arg(1);
    std::memset(ptr(out,64),0,64);write32(out,context);write32(out+16,window->second.width);write32(out+20,window->second.height);ret(context,2);
}
void endPaint(){bool ok=arg(1)&&paintContexts.erase(read32(arg(1)));if(!ok)lastError=6;ret(ok,2);}
uint32_t currentCursor{},activeWindow{};int32_t cursorVisibility{};
void setCursor(){uint32_t old=currentCursor;currentCursor=arg(0);ret(old,1);}
void showCursor(){cursorVisibility+=arg(0)?1:-1;ret(cursorVisibility,1);}
void getActiveWindow(){ret(activeWindow,0);}
void setActiveWindow(){
    uint32_t handle=arg(0);if(handle&&!windows.count(handle)){lastError=1400;ret(0,1);return;}
    uint32_t old=activeWindow;activeWindow=handle;if(old)windowMessage(old,8,handle,0);if(handle)windowMessage(handle,7,old,0);ret(old,1);
}
void setForegroundWindow(){uint32_t handle=arg(0);if(!windows.count(handle)){lastError=1400;ret(0,1);return;}activeWindow=handle;windowMessage(handle,0x1c,1,guestThreadId);windowMessage(handle,6,1,0);ret(1,1);}
struct GuestMessage {uint32_t hwnd,message,wparam,lparam,time;int32_t x=0,y=0;};
std::mutex messageMutex;
std::deque<GuestMessage> windowMessages;
uint32_t messageTime(){return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
void enqueueGuestKey(unsigned scan,bool down){
    uint32_t key=0;
    switch(scan){case 1:key=0x1b;break;case 0xe:key=8;break;case 0x1c:key=13;break;case 0xf:key=9;break;case 0x39:key=32;break;case 0x2a:key=16;break;case 0x1d:key=17;break;case 0x38:key=18;break;
        case 0xc8:key=0x26;break;case 0xd0:key=0x28;break;case 0xcb:key=0x25;break;case 0xcd:key=0x27;break;
        default:{const unsigned letters[]={0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c};
            for(unsigned i=0;i<26;++i)if(scan==letters[i])key='A'+i;
            if(scan>=2&&scan<=0xb)key=scan==0xb?'0':'1'+scan-2;break;}}
    if(!key)return;uint32_t flags=1|((scan&0x7f)<<16)|((scan&0x80)?1u<<24:0)|(!down?0xc0000000u:0);
    std::lock_guard<std::mutex> lock(messageMutex);windowMessages.push_back({0,down?0x100u:0x101u,key,flags,messageTime()});
}
// Android text entry: the game's window procedure reads typed text from WM_CHAR
// (wParam is the character as-is, 8 = backspace), so post the characters directly.
void enqueueGuestText(const std::vector<uint32_t>& characters){
    std::lock_guard<std::mutex> lock(messageMutex);
    for(uint32_t ch:characters)windowMessages.push_back({0,0x102u,ch,1u,messageTime()});
}
// Touch as mouse: Android writes the pointer as a fraction of the game view; it is
// turned into client coordinates of the active window under the machine lock.
std::atomic<float> guestPointerX{0.5f},guestPointerY{0.5f};
constexpr uint32_t pointerPending=0xffffffffu;  // lParam placeholder of queued mouse messages
uint32_t captureWindow{};
std::pair<int32_t,int32_t> pointerInWindow(){
    auto window=windows.find(activeWindow);
    if(window==windows.end())return {0,0};
    int32_t width=std::max(window->second.width,1),height=std::max(window->second.height,1);
    return {std::min(int32_t(guestPointerX.load()*float(width)),width-1),std::min(int32_t(guestPointerY.load()*float(height)),height-1)};
}
void enqueueGuestPointer(int phase){  // 0 down, 1 move, 2 up
    std::lock_guard<std::mutex> lock(messageMutex);
    windowMessages.push_back({0,0x200u,phase==2?0u:1u,pointerPending,messageTime()});
    if(phase==0)windowMessages.push_back({0,0x201u,1u,pointerPending,messageTime()});
    if(phase==2)windowMessages.push_back({0,0x202u,0u,pointerPending,messageTime()});
}
void setCapture(){uint32_t old=captureWindow;captureWindow=windows.count(arg(0))?arg(0):0;ret(old,1);}
void releaseCapture(){captureWindow=0;ret(1,0);}
bool takeWindowMessage(uint32_t output,uint32_t handle,uint32_t minimum,uint32_t maximum,bool remove){
    std::lock_guard<std::mutex> lock(messageMutex);
    for(auto it=windowMessages.begin();it!=windowMessages.end();++it){
        GuestMessage value=*it;if(!value.hwnd&&((value.message>=0x100&&value.message<=0x109)||(value.message>=0x200&&value.message<=0x20d)))value.hwnd=activeWindow;
        if(value.message>=0x200&&value.message<=0x20d&&value.lparam==pointerPending){
            auto point=pointerInWindow();value.lparam=uint32_t(point.first&0xffff)|uint32_t(point.second&0xffff)<<16;
            if(auto window=windows.find(value.hwnd);window!=windows.end()){value.x=window->second.x+point.first;value.y=window->second.y+point.second;}
        }
        if(value.message!=0x12&&((handle&&handle!=value.hwnd)||((minimum||maximum)&&(value.message<minimum||value.message>maximum))))continue;
        std::memcpy(ptr(output,28),&value,28);if(remove)windowMessages.erase(it);return true;
    }
    return false;
}
void peekMessage(){
    if(!arg(0)||(arg(1)&&arg(1)!=UINT32_MAX&&!windows.count(arg(1)))){lastError=1400;ret(0,5);return;}
    if(arg(4)&0xfffcu)throw GuestStop("PeekMessage flags pending: "+hex(arg(4)));
    ret(takeWindowMessage(arg(0),arg(1),arg(2),arg(3),arg(4)&1),5);
}
void getMessage(){
    if(!arg(0)||(arg(1)&&arg(1)!=UINT32_MAX&&!windows.count(arg(1)))){lastError=1400;ret(UINT32_MAX,4);return;}
    while(!takeWindowMessage(arg(0),arg(1),arg(2),arg(3),true)){checkGuestProgress();yieldGuestMachine(1);}
    ret(read32(arg(0)+4)==0x12?0:1,4);
}
void dispatchMessage(){uint32_t message=arg(0);if(!message){ret(0,1);return;}ret(windowMessage(read32(message),read32(message+4),read32(message+8),read32(message+12)),1);}
void translateMessage(){
    uint32_t address=arg(0);if(!address){ret(0,1);return;}uint32_t type=read32(address+4),key=read32(address+8);
    if(type!=0x100&&type!=0x104){ret(0,1);return;}
    if((key>='A'&&key<='Z')||(key>='0'&&key<='9')||key==13||key==9||key==32||key==8){
        if(key>='A'&&key<='Z')key+='a'-'A';
        std::lock_guard<std::mutex> lock(messageMutex);windowMessages.push_back({read32(address),0x102,key,read32(address+12),messageTime()});
    }
    ret(1,1);
}
void postMessage(){uint32_t hwnd=arg(0);if(hwnd&&!windows.count(hwnd)){lastError=1400;ret(0,4);return;}
    std::lock_guard<std::mutex> lock(messageMutex);windowMessages.push_back({hwnd,arg(1),arg(2),arg(3),messageTime()});ret(1,4);}
void postQuitMessage(){std::lock_guard<std::mutex> lock(messageMutex);windowMessages.push_back({0,0x12,arg(0),0,messageTime()});ret(0,1);}
void sendMessage(){if(!windows.count(arg(0))){lastError=1400;ret(0,4);return;}ret(windowMessage(arg(0),arg(1),arg(2),arg(3)),4);}
