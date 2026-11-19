// DirectInput 8 ANSI ABI. Guest pointers and vtables remain 32-bit offsets.
constexpr uint32_t inputMethodBase=0x7d000000;
constexpr uint32_t diInvalid=0x80070057,diNotAcquired=0x8007000c;
std::mutex inputMutex;
std::array<uint8_t,256> inputKeys{};
std::atomic<bool> inputDrivingMode{false};
std::array<uint8_t,8> inputMouseButtons{};
int32_t inputMouseX{},inputMouseY{},inputMouseZ{};
struct InputFormatObject {uint32_t offset,type,bytes=1,user=UINT32_MAX;};
struct InputKeyEvent {uint64_t sequence;uint32_t scan,data,time;};
struct InputBufferedEvent {uint32_t offset,data,time,sequence,user;};
std::deque<InputKeyEvent> inputKeyEvents;uint64_t inputSequence=0;
void recordInputKey(unsigned scan,bool down){
    uint32_t time=uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    inputKeyEvents.push_back({++inputSequence,scan,down?0x80u:0u,time});if(inputKeyEvents.size()>4096)inputKeyEvents.pop_front();
}
struct InputObject {
    uint32_t kind,table,references=1,size=0,cooperative=0,bufferSize=0;
    bool acquired=false;
    std::vector<InputFormatObject> format;
    uint64_t sequence=0;bool overflow=false;
    std::deque<InputBufferedEvent> pending;
    // Joystick (kind 4): per-axis range/dead zone/saturation and the values last
    // reported per format item, for buffered data.
    std::array<int32_t,6> rangeMin{{0,0,0,0,0,0}},rangeMax{{65535,65535,65535,65535,65535,65535}};
    std::array<uint32_t,6> deadZone{},saturation{{10000,10000,10000,10000,10000,10000}};
    std::vector<uint32_t> lastValues;
};
// The Android gamepad as a DirectInput joystick (kind 4), laid out like an Xbox
// pad under DirectInput: X/Y left stick, Z triggers (LT up, RT down), Rx/Ry right
// stick, Rz centred; buttons A B X Y LB RB Back Start LS RS; one POV hat.
struct InputPad {float axis[6]{};uint32_t buttons=0;int hatX=0,hatY=0;};
InputPad inputPad;bool inputPadConnected=false;
std::atomic<int> inputPadAcquired{0};
std::array<uint8_t,16> directInputGuid(uint32_t data1){
    std::array<uint8_t,16> guid={0,0,0,0,0xf3,0xc9,0xcf,0x11,0xbf,0xc7,0x44,0x45,0x53,0x54,0,0};
    std::memcpy(guid.data(),&data1,4);return guid;
}
const std::array<uint8_t,16> padGuid={0x20,0x50,0x41,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x41,0x4e,0x44,0x52,0x4f,0x49};
// guidProduct of a HID device: PID<<16|VID, then "PIDVID".
const std::array<uint8_t,16> padProductGuid={0x5e,0x04,0x8e,0x02,0,0,0,0,0,0,0x50,0x49,0x44,0x56,0x49,0x44};
struct PadObject {uint32_t guidData1,offset,type,flags;const char* name;};
// Offsets are those of c_dfDIJoystick / DIJOYSTATE.
const PadObject padObjects[]={
    {0xa36d02e0u,0,0x0002u|(0u<<8),0x100u,"X Axis"},{0xa36d02e1u,4,0x0002u|(1u<<8),0x100u,"Y Axis"},
    {0xa36d02e2u,8,0x0002u|(2u<<8),0x100u,"Z Axis"},{0xa36d02f4u,12,0x0002u|(3u<<8),0x100u,"X Rotation"},
    {0xa36d02f5u,16,0x0002u|(4u<<8),0x100u,"Y Rotation"},{0xa36d02e3u,20,0x0002u|(5u<<8),0x100u,"Z Rotation"},
    {0xa36d02f2u,32,0x0010u|(0u<<8),0,"Hat Switch"},
    {0xa36d02f0u,48,0x0004u|(0u<<8),0,"Button 0"},{0xa36d02f0u,49,0x0004u|(1u<<8),0,"Button 1"},
    {0xa36d02f0u,50,0x0004u|(2u<<8),0,"Button 2"},{0xa36d02f0u,51,0x0004u|(3u<<8),0,"Button 3"},
    {0xa36d02f0u,52,0x0004u|(4u<<8),0,"Button 4"},{0xa36d02f0u,53,0x0004u|(5u<<8),0,"Button 5"},
    {0xa36d02f0u,54,0x0004u|(6u<<8),0,"Button 6"},{0xa36d02f0u,55,0x0004u|(7u<<8),0,"Button 7"},
    {0xa36d02f0u,56,0x0004u|(8u<<8),0,"Button 8"},{0xa36d02f0u,57,0x0004u|(9u<<8),0,"Button 9"},
};
// The value DirectInput would report for a resolved format item (type: class
// in the low byte, our object index in bits 8..23, 0xffff = not on this pad).
uint32_t padItemValue(const InputObject& object,const InputFormatObject& item){
    uint32_t index=(item.type>>8)&0xffff,category=item.type&0xff;
    if(category&3){
        if(index>5)return 0;
        float v=inputPad.axis[index];
        float dz=object.deadZone[index]/10000.f,sat=std::max(object.saturation[index]/10000.f,dz+0.01f);
        float magnitude=std::fabs(v);magnitude=magnitude<=dz?0.f:std::min(1.f,(magnitude-dz)/(sat-dz));
        v=v<0?-magnitude:magnitude;
        double lo=object.rangeMin[index],hi=object.rangeMax[index];
        return static_cast<uint32_t>(static_cast<int32_t>(std::lround(lo+(v+1.0)*0.5*(hi-lo))));
    }
    if(category&0x10){
        if(index||(!inputPad.hatX&&!inputPad.hatY))return 0xffffffffu;
        static const uint32_t angles[3][3]={{31500,0,4500},{27000,0xffffffffu,9000},{22500,18000,13500}};
        return angles[inputPad.hatY+1][inputPad.hatX+1];
    }
    return index<10&&(inputPad.buttons>>index&1)?0x80u:0u;
}
std::map<uint32_t,InputObject> inputObjects;
std::map<uint32_t,uint32_t> inputMethodCalls;
uint32_t inputToken{};
const std::array<uint8_t,16> keyboardGuid={0x61,0x2b,0x1d,0x6f,0xa0,0xd5,0xcf,0x11,0xbf,0xc7,0x44,0x45,0x53,0x54,0,0};
const std::array<uint8_t,16> mouseGuid={0x60,0x2b,0x1d,0x6f,0xa0,0xd5,0xcf,0x11,0xbf,0xc7,0x44,0x45,0x53,0x54,0,0};
const std::array<uint8_t,16> input8Guid={0x30,0x80,0x79,0xbf,0x3a,0x48,0xa2,0x4d,0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0};
const std::array<uint8_t,16> device8Guid={0x80,0x10,0xd4,0x54,0x15,0xdc,0x33,0x48,0xa4,0x1b,0x74,0x8f,0x73,0xa3,0x81,0x79};
bool inputGuidEquals(uint32_t address,const std::array<uint8_t,16>& guid){return address&&!std::memcmp(ptr(address,16),guid.data(),16);}
uint32_t createInputObject(uint32_t kind){
    uint32_t object=allocate(4),table=allocate(32*4);
    for(uint32_t slot=0;slot<32;++slot)write32(table+slot*4,inputMethodBase+kind*0x1000+slot*16);
    write32(object,table);inputObjects.emplace(object,InputObject{kind,table});return object;
}
void inputDeviceInstance(uint32_t address,uint32_t kind,uint32_t bytes){
    std::memset(ptr(address,bytes),0,bytes);write32(address,bytes);
    if(kind==4){
        std::memcpy(ptr(address+4,16),padGuid.data(),16);std::memcpy(ptr(address+20,16),padProductGuid.data(),16);
        write32(address+36,0x10215);  // DI8DEVTYPE_GAMEPAD, standard subtype, HID
        const char* name="Android Gamepad";
        std::memcpy(ptr(address+40,std::strlen(name)+1),name,std::strlen(name)+1);
        std::memcpy(ptr(address+300,std::strlen(name)+1),name,std::strlen(name)+1);
        if(bytes>=580){*static_cast<uint16_t*>(ptr(address+576,2))=1;*static_cast<uint16_t*>(ptr(address+578,2))=5;}
        return;
    }
    auto& guid=kind==2?keyboardGuid:mouseGuid;
    std::memcpy(ptr(address+4,16),guid.data(),16);std::memcpy(ptr(address+20,16),guid.data(),16);
    write32(address+36,kind==2?0x413:0x212);
    const char* name=kind==2?"Android keyboard":"Android pointer";
    std::memcpy(ptr(address+40,std::strlen(name)+1),name,std::strlen(name)+1);
    std::memcpy(ptr(address+300,std::strlen(name)+1),name,std::strlen(name)+1);
}
void directInputCreate(){
    uint32_t output=arg(3);if(!output){ret(0x80004003,5);return;}write32(output,0);
    if(arg(4)){ret(0x80040110,5);return;}
    if(arg(1)!=0x800){ret(diInvalid,5);return;}
    if(!inputGuidEquals(arg(2),input8Guid)){ret(0x80004002,5);return;}
    write32(output,createInputObject(1));ret(0,5);
}
bool validActionFormat(uint32_t format){
    if(!format||!memory->readable(format,328)||read32(format)!=328||read32(format+4)!=40)return false;
    uint32_t count=read32(format+12),actions=read32(format+16);
    return count&&count<=1024&&read32(format+8)==count*4&&actions&&memory->writable(actions,count*40ull);
}
unsigned buildKeyboardActions(uint32_t format){
    uint32_t count=read32(format+12),actions=read32(format+16);unsigned mapped=0;
    for(uint32_t i=0;i<count;++i){uint32_t action=actions+i*40,semantic=read32(action+4),flags=read32(action+8);
        if(flags&4)continue;
        if((flags&2)&&read32(action+36)){if(inputGuidEquals(action+16,keyboardGuid))++mapped;continue;}
        if((semantic&0xffffff00u)==0x81000400u){
            std::memcpy(ptr(action+16,16),keyboardGuid.data(),16);write32(action+32,((semantic&255)<<8)|4);write32(action+36,0x20);++mapped;
        }else if(inputGuidEquals(action+16,keyboardGuid))++mapped;
    }return mapped;
}
void releaseInputObject(uint32_t address){auto found=inputObjects.find(address);if(found==inputObjects.end())return;
    if(--found->second.references)return;heaps->free(GuestHeaps::process,found->second.table);heaps->free(GuestHeaps::process,address);inputObjects.erase(found);
}
// IDirectInputDevice8 methods whose joystick behaviour differs from the
// keyboard/mouse path. Returns false for the shared ones (Acquire, cooperative
// level, device info, ...).
bool joystickMethod(InputObject& object,uint32_t self,uint32_t method){
    (void)self;
    auto objectFor=[&](uint32_t how,uint32_t id)->int{
        for(unsigned i=0;i<std::size(padObjects);++i){
            if(how==1){  // DIPH_BYOFFSET: offset in the installed data format
                for(auto& item:object.format)if(item.offset==id&&((item.type>>8)&0xffff)==((padObjects[i].type>>8)&0xffff)&&
                    ((item.type&padObjects[i].type&0x1f)))return int(i);
            }else if(how==2&&((id&0xffff1f)==(padObjects[i].type&0xffff1f)))return int(i);
        }return -1;};
    auto fillObject=[&](uint32_t output,const PadObject& entry){
        uint32_t bytes=read32(output);std::memset(ptr(output+4,bytes-4),0,bytes-4);
        auto guid=directInputGuid(entry.guidData1);std::memcpy(ptr(output+4,16),guid.data(),16);
        write32(output+20,entry.offset);write32(output+24,entry.type);write32(output+28,entry.flags);
        std::memcpy(ptr(output+32,std::strlen(entry.name)+1),entry.name,std::strlen(entry.name)+1);
    };
    switch(method){
    case 3:{uint32_t output=arg(1);if(!output||read32(output)!=44){ret(diInvalid,2);return true;}
        std::memset(ptr(output+4,40),0,40);write32(output+4,1);write32(output+8,0x10215);
        write32(output+12,6);write32(output+16,10);write32(output+20,1);ret(0,2);return true;}
    case 4:{uint32_t callback=arg(1),context=arg(2),flags=arg(3);if(!callback){ret(diInvalid,4);return true;}
        uint32_t instance=allocate(316);
        try{for(auto& entry:padObjects){
            uint32_t category=flags&0x1f;
            if(category&&!(entry.type&category))continue;
            write32(instance,316);fillObject(instance,entry);
            if(!callGuest(callback,{instance,context}))break;
        }}catch(...){heaps->free(GuestHeaps::process,instance);throw;}
        heaps->free(GuestHeaps::process,instance);ret(0,4);return true;}
    case 5:case 6:{uint32_t property=arg(1),header=arg(2);
        if(!header||read32(header+4)!=16){ret(diInvalid,3);return true;}
        uint32_t size=read32(header),objectId=read32(header+8),how=read32(header+12);
        if(property==1){  // DIPROP_BUFFERSIZE
            if(size!=20||how){ret(diInvalid,3);return true;}
            if(method==6){if(object.acquired){ret(0x800700aa,3);return true;}if(read32(header+16)>4096){ret(diInvalid,3);return true;}object.bufferSize=read32(header+16);}
            else write32(header+16,object.bufferSize);ret(0,3);return true;}
        if(property==2){if(size!=20){ret(diInvalid,3);return true;}if(method==5)write32(header+16,0);ret(method==6&&read32(header+16)?0x80004001:0,3);return true;}
        if(property==9){if(size!=20){ret(diInvalid,3);return true;}if(method==5)write32(header+16,0);ret(0,3);return true;}
        if(property!=4&&property!=5&&property!=6){ret(0x80004001,3);return true;}
        if(size!=(property==4?24u:20u)){ret(diInvalid,3);return true;}
        std::vector<unsigned> axes;
        if(how==0)axes={0,1,2,3,4,5};
        else{int found=objectFor(how,objectId);if(found<0||found>5){ret(0x80070002,3);return true;}axes={unsigned(found)};}
        std::lock_guard<std::mutex> lock(inputMutex);
        if(method==6){
            if(property==4){int32_t lo=static_cast<int32_t>(read32(header+16)),hi=static_cast<int32_t>(read32(header+20));if(lo>=hi){ret(diInvalid,3);return true;}
                for(auto a:axes){object.rangeMin[a]=lo;object.rangeMax[a]=hi;}}
            else{uint32_t value=read32(header+16);if(value>10000){ret(diInvalid,3);return true;}for(auto a:axes)(property==5?object.deadZone:object.saturation)[a]=value;}
            if(object.acquired){object.lastValues.clear();for(auto& item:object.format)object.lastValues.push_back(padItemValue(object,item));}
        }else{
            if(how==0&&property==4){ret(diInvalid,3);return true;}
            unsigned a=axes.front();
            if(property==4){write32(header+16,static_cast<uint32_t>(object.rangeMin[a]));write32(header+20,static_cast<uint32_t>(object.rangeMax[a]));}
            else write32(header+16,(property==5?object.deadZone:object.saturation)[a]);
        }
        ret(0,3);return true;}
    case 9:{if(!object.acquired){ret(diNotAcquired,3);return true;}
        if(arg(1)!=object.size||!arg(2)){ret(diInvalid,3);return true;}
        std::lock_guard<std::mutex> lock(inputMutex);std::memset(ptr(arg(2),object.size),0,object.size);
        for(auto& item:object.format){uint32_t value=padItemValue(object,item);
            if(item.bytes==4)write32(arg(2)+item.offset,value);else *static_cast<uint8_t*>(ptr(arg(2)+item.offset,1))=uint8_t(value);}
        ret(0,3);return true;}
    case 10:{uint32_t bytes=arg(1),out=arg(2),count=arg(3),flags=arg(4);
        if(!object.bufferSize){ret(0x80040207,5);return true;}if(!object.acquired){ret(diNotAcquired,5);return true;}
        if((bytes!=16&&bytes!=20)||!count||!memory->writable(count,4)||(flags&~1u)){ret(diInvalid,5);return true;}
        std::lock_guard<std::mutex> lock(inputMutex);
        uint32_t time=uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if(object.lastValues.size()!=object.format.size())object.lastValues.assign(object.format.size(),0);
        for(size_t i=0;i<object.format.size();++i){uint32_t value=padItemValue(object,object.format[i]);
            if(value==object.lastValues[i])continue;object.lastValues[i]=value;
            if(object.pending.size()>=object.bufferSize){object.pending.pop_front();object.overflow=true;}
            object.pending.push_back({object.format[i].offset,value,time,uint32_t(++inputSequence),object.format[i].user});}
        uint32_t amount=uint32_t(std::min<uint64_t>(read32(count),object.pending.size()));
        if(out&&!memory->writable(out,uint64_t(amount)*bytes)){ret(diInvalid,5);return true;}
        if(out)for(uint32_t i=0;i<amount;++i)std::memcpy(ptr(out+i*bytes,bytes),&object.pending[i],bytes);
        uint32_t status=object.overflow?1:0;write32(count,amount);
        if(!(flags&1)){for(uint32_t i=0;i<amount;++i)object.pending.pop_front();object.overflow=false;}
        ret(status,5);return true;}
    case 11:{uint32_t format=arg(1);if(object.acquired){ret(0x800700aa,2);return true;}
        if(!format||read32(format)!=24||read32(format+4)!=16){ret(diInvalid,2);return true;}
        uint32_t size=read32(format+12),count=read32(format+16),array=read32(format+20);
        if(!size||size>4096||count>256||(count&&!array)){ret(diInvalid,2);return true;}
        std::vector<InputFormatObject> items;std::array<bool,6> axisUsed{};std::array<bool,10> buttonUsed{};bool povUsed=false;
        for(uint32_t i=0;i<count;++i){
            uint32_t guidAddress=read32(array+i*16),offset=read32(array+i*16+4),type=read32(array+i*16+8);
            uint32_t category=type&0x1f,instance=(type>>8)&0xffff,index=0xffff;
            uint32_t data1=guidAddress&&memory->readable(guidAddress,16)?read32(guidAddress):0;
            uint32_t bytes=(category&3)||(category&0x10)?4:1;
            if(category&3){
                static const uint32_t axisGuids[6]={0xa36d02e0u,0xa36d02e1u,0xa36d02e2u,0xa36d02f4u,0xa36d02f5u,0xa36d02e3u};
                if(instance!=0xffff){if(instance<6)index=instance;}
                else if(data1){for(uint32_t a=0;a<6;++a)if(axisGuids[a]==data1&&!axisUsed[a]){index=a;break;}}
                else for(uint32_t a=0;a<6;++a)if(!axisUsed[a]){index=a;break;}
                if(index<6)axisUsed[index]=true;
            }else if(category&0x10){
                if((instance==0xffff||instance==0)&&!povUsed){index=0;povUsed=true;}
            }else if(category&12){
                if(instance!=0xffff){if(instance<10)index=instance;}
                else for(uint32_t b=0;b<10;++b)if(!buttonUsed[b]){index=b;break;}
                if(index<10)buttonUsed[index]=true;
            }else{ret(diInvalid,2);return true;}
            if(offset>size||bytes>size-offset){ret(diInvalid,2);return true;}
            // A required object this pad lacks fails like DirectInput does.
            if(index==0xffff&&!(type&0x80000000u)){ret(diInvalid,2);return true;}
            items.push_back({offset,(category&3?2u:category&0x10?0x10u:4u)|(index<<8),bytes});
        }
        object.size=size;object.format=std::move(items);
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Gamepad data format: %u bytes, %zu objects",size,object.format.size());
        ret(0,2);return true;}
    case 14:{uint32_t output=arg(1),objectId=arg(2),how=arg(3);
        if(!output||(read32(output)!=316&&read32(output)!=280)){ret(diInvalid,4);return true;}
        int found=objectFor(how,objectId);if(found<0){ret(0x80070002,4);return true;}
        fillObject(output,padObjects[found]);ret(0,4);return true;}
    case 19:ret(0,4);return true;   // EnumEffects: no force feedback
    case 25:ret(object.acquired?0:diNotAcquired,1);return true;
    case 29:ret(0x80004001,4);return true;
    case 30:ret(0x80004001,4);return true;
    case 16:case 17:case 18:case 20:case 21:case 22:case 23:case 24:case 26:case 27:case 28:case 31:{
        // Argument counts including `this`, for the stdcall cleanup.
        static constexpr uint32_t args[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,4,5,0,3,2,2,4,2,0,5,5,5,0,0,2};
        ret(0x80004001,args[method]);return true;}
    default:return false;
    }
}
// Key names for GetObjectInfo (the controls screen shows them next to each action).
std::string keyboardKeyName(uint32_t dik){
    static const char* const row[]={nullptr,"Esc","1","2","3","4","5","6","7","8","9","0","-","=","Retroceso","Tab",
        "Q","W","E","R","T","Y","U","I","O","P","[","]","Intro","Ctrl izq.","A","S","D","F","G","H","J","K","L",";","'","`",
        "Mayus izq.","\\","Z","X","C","V","B","N","M",",",".","/","Mayus der.","* (num)","Alt izq.","Espacio","Bloq Mayus",
        "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","Bloq Num","Bloq Despl","7 (num)","8 (num)","9 (num)","- (num)",
        "4 (num)","5 (num)","6 (num)","+ (num)","1 (num)","2 (num)","3 (num)","0 (num)",". (num)"};
    if(dik<sizeof row/sizeof row[0]&&row[dik])return row[dik];
    switch(dik){
    case 0x57:return "F11";case 0x58:return "F12";case 0x9c:return "Intro (num)";case 0x9d:return "Ctrl der.";
    case 0xb5:return "/ (num)";case 0xb8:return "Alt der.";case 0xc5:return "Pausa";case 0xc7:return "Inicio";
    case 0xc8:return "Arriba";case 0xc9:return "Re Pag";case 0xcb:return "Izquierda";case 0xcd:return "Derecha";
    case 0xcf:return "Fin";case 0xd0:return "Abajo";case 0xd1:return "Av Pag";case 0xd2:return "Insert";case 0xd3:return "Supr";
    }
    char text[16];std::snprintf(text,sizeof text,"Tecla %02X",dik);return text;
}
// IDirectInputDevice8::GetObjectInfo(DIDEVICEOBJECTINSTANCEA*, dwObj, dwHow) for the
// keyboard (objects are DIK codes, c_dfDIKeyboard offsets) and the mouse (DIMOUSESTATE2).
void keyboardMouseObjectInfo(uint32_t kind){
    uint32_t output=arg(1),objectId=arg(2),how=arg(3);
    if(!output||(read32(output)!=316&&read32(output)!=280)){ret(diInvalid,4);return;}
    if(how!=1&&how!=2){ret(diInvalid,4);return;}  // DIPH_BYOFFSET / DIPH_BYID
    uint32_t offset=how==1?objectId:(objectId>>8)&0xffff;
    std::string name;uint32_t type,guidData1;uint16_t guidData2=0xc9f3;
    if(kind==2){
        if(offset>255){ret(0x80070002,4);return;}  // DIERR_NOTFOUND
        if(how==2&&!(objectId&0x0c)){ret(0x80070002,4);return;}
        name=keyboardKeyName(offset);type=0x0cu|(offset<<8);guidData1=0x55728220u;guidData2=0xd33c;  // GUID_Key
    }else{
        static const char* const axes[]={"Eje X","Eje Y","Rueda"};
        if(how==1&&offset<12&&offset%4==0){name=axes[offset/4];type=(offset==8?0x02u:0x01u)|((offset/4)<<8);guidData1=0xa36d02e0u+offset/4;}
        else if(how==1&&offset>=12&&offset<20){name="Boton "+std::to_string(offset-12);type=0x04u|((offset-12)<<8);guidData1=0xa36d02f0u;}
        else if(how==2&&(objectId&3)&&offset<3){name=axes[offset];type=objectId&0xffff03u;guidData1=0xa36d02e0u+offset;offset*=4;}
        else if(how==2&&(objectId&0x0c)&&offset<8){name="Boton "+std::to_string(offset);type=objectId&0xffff0cu;guidData1=0xa36d02f0u;offset+=12;}
        else{ret(0x80070002,4);return;}
    }
    uint32_t bytes=read32(output);std::memset(ptr(output+4,bytes-4),0,bytes-4);
    const uint8_t tail[8]={0xbf,0xc7,0x44,0x45,0x53,0x54,0,0};uint16_t guidData3=0x11cf;
    std::memcpy(ptr(output+4,4),&guidData1,4);std::memcpy(ptr(output+8,2),&guidData2,2);std::memcpy(ptr(output+10,2),&guidData3,2);std::memcpy(ptr(output+12,8),tail,8);
    write32(output+20,offset);write32(output+24,type);write32(output+28,0);
    std::memcpy(ptr(output+32,name.size()+1),name.c_str(),name.size()+1);
    ret(0,4);
}
void inputDispatch(){
    uint32_t kind=(inputToken-inputMethodBase)/0x1000,method=((inputToken-inputMethodBase)%0x1000)/16;
    uint32_t self=arg(0);auto found=inputObjects.find(self);
    if(found==inputObjects.end()||found->second.kind!=kind)throw GuestStop("Invalid DirectInput object");
    auto& object=found->second;
    if(++inputMethodCalls[inputToken]<=3)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","DirectInput kind=%u method=%u",kind,method);
    if(method==0){
        if(!arg(2)){ret(0x80004003,3);return;}write32(arg(2),0);
        std::array<uint8_t,16> unknown={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
        if(!inputGuidEquals(arg(1),kind==1?input8Guid:device8Guid)&&!inputGuidEquals(arg(1),unknown)){ret(0x80004002,3);return;}
        ++object.references;write32(arg(2),self);ret(0,3);return;
    }
    if(method==1){ret(++object.references,1);return;}
    if(method==2){uint32_t refs=--object.references;if(!refs){if(kind==4&&object.acquired)--inputPadAcquired;
        heaps->free(GuestHeaps::process,object.table);heaps->free(GuestHeaps::process,self);inputObjects.erase(found);}ret(refs,1);return;}
    if(kind==1){
        switch(method){
        case 3:{uint32_t output=arg(2);if(!output){ret(0x80004003,4);return;}write32(output,0);
            uint32_t device=inputGuidEquals(arg(1),keyboardGuid)?2:inputGuidEquals(arg(1),mouseGuid)?3:inputGuidEquals(arg(1),padGuid)?4:0;
            if(device==4){std::lock_guard<std::mutex> lock(inputMutex);if(!inputPadConnected)device=0;}
            if(inputGuidEquals(arg(1),padGuid))NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","DirectInput CreateDevice gamepad: %s",device?"created":"not connected");
            if(arg(3)){ret(0x80040110,4);return;}if(!device){ret(0x80070002,4);return;}
            write32(output,createInputObject(device));ret(0,4);return;}
        case 4:{uint32_t type=arg(1),callback=arg(2),context=arg(3),flags=arg(4);
            if(!callback||flags&~0x70111u){ret(diInvalid,5);return;}
            bool pad;{std::lock_guard<std::mutex> lock(inputMutex);pad=inputPadConnected;}
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","DirectInput EnumDevices type=%u flags=%x gamepad=%d",type,flags,int(pad));
            uint32_t instance=allocate(580);
            try{for(uint32_t device:{2u,3u,4u}){
                if(device==4&&(!pad||(flags&0x100u)))continue;  // no force feedback
                uint32_t deviceType=device==2?0x13:device==3?0x12:0x15;
                uint32_t deviceClass=device==2?3:device==3?2:4;
                if(type!=0&&(type!=1||device==4)&&type!=deviceClass&&(type&0xff)!=deviceType)continue;
                inputDeviceInstance(instance,device,580);
                uint32_t proceed=callGuest(callback,{instance,context});
                if(device==4)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","DirectInput EnumDevices offered the gamepad; callback returned %u",proceed);
                if(!proceed)break;
            }}catch(...){heaps->free(GuestHeaps::process,instance);throw;}
            heaps->free(GuestHeaps::process,instance);ret(0,5);return;}
        case 5:{if(inputGuidEquals(arg(1),padGuid)){std::lock_guard<std::mutex> lock(inputMutex);ret(inputPadConnected?0:1,2);return;}
            ret(inputGuidEquals(arg(1),keyboardGuid)||inputGuidEquals(arg(1),mouseGuid)?0:0x80070002,2);return;}
        case 7:ret(arg(2)==0x800?0:diInvalid,3);return;
        case 9:{uint32_t format=arg(2),callback=arg(3),context=arg(4),flags=arg(5);
            if(!validActionFormat(format)||!callback||(flags&~0x7110u)){ret(diInvalid,6);return;}
            if(flags&(0x100u|0x2000u|0x4000u)){ret(0,6);return;} // No force-feedback, secondary or non-gaming HID devices.
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Enumerate action devices: %u actions callback=%08x",read32(format+12),callback);
            uint32_t instance=allocate(580);
            try{for(uint32_t kind:{2u,3u}){
                auto mapped=kind==2?buildKeyboardActions(format):0;uint32_t device=createInputObject(kind);
                // The semantic enumerator hands over a usable system device;
                // this guest does not call SetCooperativeLevel on that keyboard.
                inputObjects.at(device).cooperative=10;inputDeviceInstance(instance,kind,580);
                uint32_t proceed;try{proceed=callGuest(callback,{instance,device,0x20u|(mapped?1u:0u),kind==2?1u:0u,context});}catch(...){releaseInputObject(device);throw;}
                releaseInputObject(device);if(!proceed)break;
            }}catch(...){heaps->free(GuestHeaps::process,instance);throw;}
            heaps->free(GuestHeaps::process,instance);ret(0,6);return;}
        default:throw GuestStop("DirectInput interface method pending: "+std::to_string(method));
        }
    }
    if(kind==4&&joystickMethod(object,self,method))return;
    if(method==14){keyboardMouseObjectInfo(kind);return;}
    switch(method){
    case 3:{uint32_t output=arg(1);if(!output||read32(output)!=44){ret(diInvalid,2);return;}
        std::memset(ptr(output+4,40),0,40);write32(output+4,1);write32(output+8,kind==2?0x413:0x212);
        write32(output+12,kind==2?0:3);write32(output+16,kind==2?256:8);ret(0,2);return;}
    case 5:case 6:{uint32_t property=arg(1),header=arg(2);
        if(!header||read32(header)!=20||read32(header+4)!=16||read32(header+12)!=0){ret(diInvalid,3);return;}
        if(property!=1){ret(0x80004001,3);return;}
        if(method==6){if(object.acquired){ret(0x800700aa,3);return;}if(read32(header+16)>4096){ret(diInvalid,3);return;}object.bufferSize=read32(header+16);}
        else write32(header+16,object.bufferSize);
        ret(0,3);return;}
    case 7:if(!object.size||!object.cooperative){ret(diInvalid,1);return;}if(object.acquired){ret(1,1);return;}
        {std::lock_guard<std::mutex> lock(inputMutex);object.sequence=inputSequence;object.pending.clear();object.overflow=false;
         if(kind==4){object.lastValues.clear();for(auto& item:object.format)object.lastValues.push_back(padItemValue(object,item));++inputPadAcquired;
             NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Gamepad acquired by the game: %zu objects",object.format.size());}}
        object.acquired=true;ret(0,1);return;
    case 8:{bool previous=object.acquired;object.acquired=false;if(previous&&kind==4)--inputPadAcquired;ret(previous?0:1,1);return;}
    case 9:{if(!object.acquired){ret(diNotAcquired,3);return;}
        if(arg(1)!=object.size||!arg(2)){ret(diInvalid,3);return;}
        std::lock_guard<std::mutex> lock(inputMutex);std::memset(ptr(arg(2),object.size),0,object.size);
        for(auto item:object.format){uint32_t index=(item.type>>8)&0xffff,category=item.type&0xff;
            if(kind==2){if(item.bytes==4)write32(arg(2)+item.offset,inputKeys[index]);else *static_cast<uint8_t*>(ptr(arg(2)+item.offset,1))=inputKeys[index];}
            else if(category&3){int32_t value=index==0?inputMouseX:index==1?inputMouseY:inputMouseZ;write32(arg(2)+item.offset,static_cast<uint32_t>(value));}
            else *static_cast<uint8_t*>(ptr(arg(2)+item.offset,1))=inputMouseButtons[index];
        }
        if(kind==3)inputMouseX=inputMouseY=inputMouseZ=0;
        ret(0,3);return;}
    case 10:{uint32_t bytes=arg(1),out=arg(2),count=arg(3),flags=arg(4);
        if(!object.bufferSize){ret(0x80040207,5);return;}if(!object.acquired){ret(diNotAcquired,5);return;}
        if((bytes!=16&&bytes!=20)||!count||!memory->writable(count,4)||(flags&~1u)){ret(diInvalid,5);return;}
        if(kind!=2)throw GuestStop("Buffered mouse data pending");
        std::lock_guard<std::mutex> lock(inputMutex);
        if(!inputKeyEvents.empty()&&inputKeyEvents.front().sequence>object.sequence+1)object.overflow=true;
        for(auto event:inputKeyEvents){if(event.sequence<=object.sequence)continue;
            for(auto item:object.format){if(((item.type>>8)&0xffff)!=event.scan)continue;
                if(object.pending.size()>=object.bufferSize){object.pending.pop_front();object.overflow=true;}
                object.pending.push_back({item.offset,event.data,event.time,uint32_t(event.sequence),item.user});
            }
        }object.sequence=inputSequence;
        uint32_t amount=uint32_t(std::min<uint64_t>(read32(count),object.pending.size()));
        if(out&&!memory->writable(out,uint64_t(amount)*bytes)){ret(diInvalid,5);return;}
        if(out)for(uint32_t i=0;i<amount;++i)std::memcpy(ptr(out+i*bytes,bytes),&object.pending[i],bytes);
        uint32_t status=object.overflow?1:0;write32(count,amount);
        if(!(flags&1)){for(uint32_t i=0;i<amount;++i)object.pending.pop_front();object.overflow=false;}
        if(amount)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Keyboard buffered input delivered %u events peek=%u from=%08x",amount,flags&1,g_cur_func);ret(status,5);return;}
    case 11:{uint32_t format=arg(1);if(object.acquired){ret(0x800700aa,2);return;}
        if(!format||read32(format)!=24||read32(format+4)!=16){ret(diInvalid,2);return;}
        uint32_t size=read32(format+12),count=read32(format+16),array=read32(format+20);
        if(!size||size>4096||count>256||(count&&!array)){ret(diInvalid,2);return;}
        std::vector<InputFormatObject> items;
        uint32_t nextAxis=0,nextButton=0;
        for(uint32_t i=0;i<count;++i){uint32_t offset=read32(array+i*16+4),type=read32(array+i*16+8),index=(type>>8)&0xffff,category=type&0xff;
            if(index==0xffff){index=(category&3)?nextAxis++:nextButton++;type=(type&~0xffff00u)|(index<<8);}
            uint32_t bytes=(category&3)?4:1;
            if(offset>size||bytes>size-offset||(kind==2&&(index>255||!(category&12)))||(kind==3&&((category&3)?index>2:index>7))){ret(diInvalid,2);return;}
            items.push_back({offset,type,bytes});}
        object.size=size;object.format=std::move(items);ret(0,2);return;}
    case 12:if(arg(1))throw GuestStop("DirectInput event notification pending");ret(0,2);return;
    case 13:{uint32_t flags=arg(2);if(!windows.count(arg(1))||flags&~31u||(!(flags&3))||((flags&3)==3)||(!(flags&12))||((flags&12)==12)){ret(diInvalid,3);return;}
        if((flags&9)==9){ret(0x80004001,3);return;}object.cooperative=flags;ret(0,3);return;}
    case 15:{uint32_t output=arg(1);if(!output||(read32(output)!=580&&read32(output)!=560)){ret(diInvalid,2);return;}inputDeviceInstance(output,kind,read32(output));ret(0,2);return;}
    case 25:ret(object.acquired?0:diNotAcquired,1);return;
    case 29:{if(!validActionFormat(arg(1))||(arg(3)!=0&&arg(3)!=1&&arg(3)!=2&&arg(3)!=4)){ret(diInvalid,4);return;}
        if(kind!=2){ret(0x80004001,4);return;}auto mapped=buildKeyboardActions(arg(1));
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Keyboard BuildActionMap mapped %u actions",mapped);ret(0,4);return;}
    case 30:{uint32_t format=arg(1);if(object.acquired){ret(0x800700aa,4);return;}
        if(!validActionFormat(format)||(arg(3)&~3u)){ret(diInvalid,4);return;}if(kind!=2){ret(0x80004001,4);return;}
        // DIDSAM_FORCESAVE (2) still installs the mapping. The game uses it
        // when switching from frontend keys to the driving action format.
        // Persistence stays in the guest profile; no host user database exists.
        std::vector<InputFormatObject> items;uint32_t actions=read32(format+16),count=read32(format+12);
        for(uint32_t i=0;i<count;++i){auto action=actions+i*40;if(!inputGuidEquals(action+16,keyboardGuid)||!read32(action+36))continue;
            auto type=read32(action+32);if(((type>>8)&0xffff)>255||!(type&4)){ret(diInvalid,4);return;}items.push_back({i*4,type,4,read32(action)});
            if(count>3)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Keyboard action user=%u scan=%02x semantic=%08x",read32(action),(type>>8)&0xffff,read32(action+4));
        }
        if(read32(format+40)>4096){ret(diInvalid,4);return;}
        object.format=std::move(items);object.size=read32(format+8);object.bufferSize=read32(format+40);object.pending.clear();object.overflow=false;
        inputDrivingMode=std::any_of(object.format.begin(),object.format.end(),[](const InputFormatObject& item){return item.user==1&&((item.type>>8)&0xffff)==0xc8;});
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Keyboard SetActionMap installed %zu actions, %u bytes, buffer=%u",object.format.size(),object.size,object.bufferSize);ret(0,4);return;}
    default:throw GuestStop("DirectInput device method pending: "+std::to_string(method));
    }
}
void testGuestInput(){
    uint32_t keyboard=createInputObject(2),format=allocate(24),items=allocate(32),output=allocate(9);
    write32(format,24);write32(format+4,16);write32(format+8,2);write32(format+12,8);write32(format+16,2);write32(format+20,items);
    write32(items+4,1);write32(items+8,(0xc8<<8)|4);write32(items+20,7);write32(items+24,(0x1c<<8)|4);
    auto invoke=[&](uint32_t slot,std::vector<uint32_t> args){args.insert(args.begin(),keyboard);return callGuest(inputMethodBase+0x2000+slot*16,args);};
    if(invoke(11,{format})||invoke(9,{8,output})!=diNotAcquired)throw GuestStop("DirectInput format/acquisition check failed");
    inputObjects.at(keyboard).cooperative=6;
    if(invoke(7,{}))throw GuestStop("DirectInput acquisition failed");
    setGuestKey(0xc8,true);setGuestKey(0x1c,true);std::memset(ptr(output,9),0xcc,9);
    if(invoke(9,{8,output})||*static_cast<uint8_t*>(ptr(output+1,1))!=0x80||*static_cast<uint8_t*>(ptr(output+7,1))!=0x80||*static_cast<uint8_t*>(ptr(output+8,1))!=0xcc)throw GuestStop("DirectInput guest offset/guard check failed");
    setGuestKey(0xc8,false);if(invoke(9,{8,output})||*static_cast<uint8_t*>(ptr(output+1,1))!=0)throw GuestStop("DirectInput key release failed");
    clearGuestInput();invoke(8,{});if(invoke(9,{8,output})!=diNotAcquired)throw GuestStop("DirectInput unacquisition check failed");
    invoke(2,{});for(uint32_t address:{format,items,output})heaps->free(GuestHeaps::process,address);
    keyboard=createInputObject(2);format=allocate(328);items=allocate(120);output=allocate(80);uint32_t eventCount=allocate(4);
    write32(format,328);write32(format+4,40);write32(format+8,12);write32(format+12,3);write32(format+16,items);
    write32(items+4,0x81000401);write32(items+44,0x8100041c);write32(items+84,0x01000402);
    write32(format+40,2);write32(items,11);write32(items+40,22);
    if(invoke(29,{format,0,0})||!inputGuidEquals(items+16,keyboardGuid)||read32(items+32)!=0x104||read32(items+116))throw GuestStop("Keyboard semantic mapping failed");
    if(invoke(30,{format,0,0}))throw GuestStop("Keyboard action format failed");inputObjects.at(keyboard).cooperative=6;
    if(invoke(7,{}))throw GuestStop("Keyboard action acquisition failed");
    setGuestKey(1,true);setGuestKey(0x1c,true);write32(output+12,0xdeadbeef);
    if(invoke(9,{12,output})||read32(output)!=0x80||read32(output+4)!=0x80||read32(output+8)||read32(output+12)!=0xdeadbeef)throw GuestStop("Keyboard action state/guard failed");
    write32(eventCount,4);write32(output+40,0xdeadbeef);
    if(invoke(10,{20,output,eventCount,1})||read32(eventCount)!=2||read32(output)!=0||read32(output+4)!=0x80||read32(output+16)!=11||read32(output+20)!=4||read32(output+36)!=22||read32(output+40)!=0xdeadbeef)throw GuestStop("Keyboard buffered peek/data/guard failed");
    write32(eventCount,UINT32_MAX);if(invoke(10,{20,0,eventCount,0})||read32(eventCount)!=2)throw GuestStop("Keyboard buffered consumption failed");
    setGuestKey(1,false);if(invoke(9,{12,output})||read32(output))throw GuestStop("Keyboard action release failed");
    write32(eventCount,4);if(invoke(10,{20,output,eventCount,0})||read32(eventCount)!=1||read32(output+4))throw GuestStop("Keyboard buffered release failed");
    for(unsigned i=0;i<2;++i){setGuestKey(1,true);setGuestKey(1,false);}write32(eventCount,4);
    if(invoke(10,{20,output,eventCount,0})!=1||read32(eventCount)!=2||read32(output+4)!=0x80||read32(output+24))throw GuestStop("Keyboard buffered overflow/order failed");
    write32(eventCount,4);if(invoke(10,{20,output,eventCount,0})||read32(eventCount))throw GuestStop("Keyboard buffered drain failed");
    if(invoke(30,{format,0,2})!=0x800700aa)throw GuestStop("Keyboard acquired remapping guard failed");
    invoke(8,{});write32(items,3);write32(items+4,0x810004c8);
    std::memset(ptr(items+16,24),0,24);
    if(invoke(29,{format,0,2})||invoke(30,{format,0,2})||invoke(7,{}))throw GuestStop("Keyboard FORCESAVE action switch failed");
    setGuestKey(0xc8,true);write32(eventCount,4);
    if(invoke(10,{20,output,eventCount,0})||read32(eventCount)!=1||read32(output+4)!=0x80||read32(output+16)!=3)throw GuestStop("Keyboard driving remap press failed");
    setGuestKey(0xc8,false);write32(eventCount,4);
    if(invoke(10,{20,output,eventCount,0})||read32(eventCount)!=1||read32(output+4))throw GuestStop("Keyboard driving remap release failed");
    clearGuestInput();invoke(8,{});invoke(2,{});for(uint32_t address:{format,items,output,eventCount})heaps->free(GuestHeaps::process,address);
    {std::lock_guard<std::mutex> lock(messageMutex);windowMessages.clear();}
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","DirectInput ABI check passed: offsets, guard, press/release, acquisition and stack cleanup");
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Keyboard action mapping passed: Escape/Enter DWORD state, unmapped action, guard and release");
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Keyboard buffered input passed: press/release, offsets, user data, peek, consume, overflow and guard");
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Keyboard action switch passed: acquired guard, FORCESAVE remap, driving press/release");
}
