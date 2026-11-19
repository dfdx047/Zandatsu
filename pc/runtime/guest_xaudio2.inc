// XAudio2 2.7 (DirectX SDK June 2010), created by CoCreateInstance(CLSID_XAudio2). First version: a real-time clock
// with no output. Source voices consume their submitted buffers at sampleRate * frequencyRatio and the callbacks
// (OnVoiceProcessingPassStart/End, OnBufferStart/End, OnStreamEnd, OnLoopEnd) run on the guest timer worker every
// 10 ms, so Wwise and CRI keep streaming. Mixing to AAudio/SDL comes next (PCPorts audio/README.md).
// Interfaces (vtable slots): IXAudio2 0 QI 1 AddRef 2 Release 3 GetDeviceCount 4 GetDeviceDetails 5 Initialize
// 6 RegisterForCallbacks 7 UnregisterForCallbacks 8 CreateSourceVoice 9 CreateSubmixVoice 10 CreateMasteringVoice
// 11 StartEngine 12 StopEngine 13 CommitChanges 14 GetPerformanceData 15 SetDebugConfiguration.
// IXAudio2Voice (no IUnknown) 0 GetVoiceDetails .. 18 DestroyVoice; IXAudio2SourceVoice adds 19 Start 20 Stop
// 21 SubmitSourceBuffer 22 FlushSourceBuffers 23 Discontinuity 24 ExitLoop 25 GetState 26 SetFrequencyRatio
// 27 GetFrequencyRatio.
struct XaBuffer {uint32_t flags,bytes,audio,playBegin,playLength,loopBegin,loopLength,loopCount,context;uint64_t frames,played=0;bool started=false;};
struct XaVoice {uint32_t object,callback=0,channels=2,rate=48000,blockAlign=4,format=1;bool source=false,running=false,ended=false;float ratio=1,volume=1;
    std::deque<XaBuffer> queue;uint64_t samplesPlayed=0;double carry=0;};
std::map<uint32_t,XaVoice> xaVoices;uint32_t nextXaVoice=1;
uint32_t xaEngine=0,xaEngineCallback=0,xaTimer=0;bool xaRunning=false;
std::vector<uint32_t> xaEngineVtable,xaVoiceVtable,xaSourceVtable;
auto xaLast=std::chrono::steady_clock::now();
XaVoice* xaVoice(uint32_t object){auto f=xaVoices.find(objectId(object));return f==xaVoices.end()?nullptr:&f->second;}
void xaCallback(uint32_t cb,uint32_t slot,std::vector<uint32_t> args){if(cb)callMethod(cb,slot,std::move(args));}
// IXAudio2VoiceCallback slots: 0 OnVoiceProcessingPassStart(bytes) 1 OnVoiceProcessingPassEnd 2 OnStreamEnd 3 OnBufferStart(ctx)
// 4 OnBufferEnd(ctx) 5 OnLoopEnd(ctx) 6 OnVoiceError(ctx, hr). IXAudio2EngineCallback: 0 OnProcessingPassStart 1 End 2 OnCriticalError.
void xaTick(){
    auto now=std::chrono::steady_clock::now();double dt=std::chrono::duration<double>(now-xaLast).count();xaLast=now;
    if(!xaRunning){ret(0,5);return;}
    if(xaEngineCallback)callMethod(xaEngineCallback,0,{});
    std::vector<uint32_t> ids;for(auto& e:xaVoices)if(e.second.source&&e.second.running)ids.push_back(e.first);
    for(uint32_t id:ids){
        auto f=xaVoices.find(id);if(f==xaVoices.end())continue;XaVoice& v=f->second;
        double want=dt*v.rate*v.ratio+v.carry;uint64_t frames=uint64_t(want);v.carry=want-double(frames);
        if(v.callback)xaCallback(v.callback,0,{uint32_t(frames*v.blockAlign)});
        while(frames&&!v.queue.empty()){
            XaBuffer& b=v.queue.front();
            if(!b.started){b.started=true;xaCallback(v.callback,3,{b.context});}
            uint64_t take=std::min<uint64_t>(frames,b.frames-b.played);b.played+=take;frames-=take;v.samplesPlayed+=take;
            if(b.played>=b.frames){
                if(b.loopCount&&b.loopCount!=255u){--b.loopCount;b.played=b.loopBegin;xaCallback(v.callback,5,{b.context});continue;}
                if(b.loopCount==255u){b.played=b.loopBegin;xaCallback(v.callback,5,{b.context});continue;}
                XaBuffer done=b;v.queue.pop_front();
                xaCallback(v.callback,4,{done.context});
                if(done.flags&0x40){xaCallback(v.callback,2,{});}   // XAUDIO2_END_OF_STREAM
                f=xaVoices.find(id);if(f==xaVoices.end())break;   // the callback may destroy the voice
            }
        }
        f=xaVoices.find(id);if(f!=xaVoices.end()&&f->second.callback)xaCallback(f->second.callback,1,{});
    }
    if(xaEngineCallback)callMethod(xaEngineCallback,1,{});
    ret(0,5);
}
void xaQI(){if(arg(2))write32(arg(2),arg(0));ret(0,3);}
void xaAddRef(){ret(2,1);}
void xaRelease(){ret(1,1);}
void xaDeviceCount(){write32(arg(1),1);ret(0,2);}
void xaDeviceDetails(){
    // XAUDIO2_DEVICE_DETAILS: DeviceID[256] wchar, DisplayName[256] wchar, Role, WAVEFORMATEXTENSIBLE (40 bytes)
    uint32_t o=arg(2);std::memset(ptr(o,1024+4+40),0,1024+4+40);write32(o+1024,0xF);   // GlobalDefaultDevice
    uint32_t wf=o+1028;uint16_t tag=0xFFFE,ch=2,align=8,bits=32,cb=22;uint32_t rate=48000,bps=rate*8;
    std::memcpy(ptr(wf,2),&tag,2);std::memcpy(ptr(wf+2,2),&ch,2);write32(wf+4,rate);write32(wf+8,bps);std::memcpy(ptr(wf+12,2),&align,2);
    std::memcpy(ptr(wf+14,2),&bits,2);std::memcpy(ptr(wf+16,2),&cb,2);std::memcpy(ptr(wf+18,2),&bits,2);write32(wf+20,3);   // SPEAKER_STEREO
    static const uint8_t floatGuid[16]={3,0,0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};std::memcpy(ptr(wf+24,16),floatGuid,16);ret(0,3);
}
void xaInitialize(){ret(0,3);}
void xaRegister(){xaEngineCallback=arg(1);ret(0,2);}
void xaUnregister(){if(xaEngineCallback==arg(1))xaEngineCallback=0;ret(0,2);}
uint32_t newVoice(bool source,uint32_t channels,uint32_t rate,uint32_t blockAlign,uint32_t callback){
    uint32_t id=nextXaVoice++;uint32_t object=guestObject(source?xaSourceVtable:xaVoiceVtable,id);
    XaVoice v;v.object=object;v.source=source;v.channels=channels?channels:2;v.rate=rate?rate:48000;v.blockAlign=blockAlign?blockAlign:4;v.callback=callback;
    xaVoices.emplace(id,v);return object;
}
void xaCreateSource(){
    // (this, ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain)
    uint32_t out=arg(1),wf=arg(2);uint16_t tag,ch,align;std::memcpy(&tag,ptr(wf,2),2);std::memcpy(&ch,ptr(wf+2,2),2);std::memcpy(&align,ptr(wf+12,2),2);
    uint32_t rate=read32(wf+4);
    if(tag==2||tag==0x166){   // ADPCM / XMA: frames come from the block layout; align is per block
        if(tag==2){uint16_t spb;std::memcpy(&spb,ptr(wf+20,2),2);align=uint16_t(std::max<uint32_t>(1,align/std::max<uint32_t>(1,spb)));}
    }
    uint32_t v=newVoice(true,ch,rate,align,arg(5));xaVoice(v)->format=tag;write32(out,v);ret(0,8);
}
void xaCreateSubmix(){write32(arg(1),newVoice(false,arg(2),arg(3),0,0));ret(0,8);}
void xaCreateMastering(){write32(arg(1),newVoice(false,arg(2)?arg(2):2,arg(3)?arg(3):48000,0,0));ret(0,7);}
void xaStart(){
    xaRunning=true;xaLast=std::chrono::steady_clock::now();
    if(!xaTimer){static uint32_t tick=hostMethod(xaTick);xaTimer=startHostTimer(tick,10);
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","XAudio2 2.7 engine started (silent clock, timer %u)",xaTimer);}
    ret(0,1);
}
void xaStop(){xaRunning=false;ret(0,1);}
void xaCommit(){ret(0,2);}
void xaPerformance(){std::memset(ptr(arg(1),64),0,64);ret(0,2);}
void xaDebug(){ret(0,3);}
// voice methods
void vDetails(){auto* v=xaVoice(arg(0));uint32_t o=arg(1);write32(o,0);write32(o+4,v?v->channels:2);write32(o+8,v?v->rate:48000);ret(0,2);}
void vOk1(){ret(0,1);}void vOk2(){ret(0,2);}void vOk3(){ret(0,3);}void vOk4(){ret(0,4);}void vOk5(){ret(0,5);}void vOk6(){ret(0,6);}
void vSetVolume(){if(auto* v=xaVoice(arg(0)))v->volume=argf(1);ret(0,3);}
void vGetVolume(){auto* v=xaVoice(arg(0));writef(arg(1),v?v->volume:1);ret(0,2);}
void vGetEffectState(){write32(arg(2),1);ret(0,3);}
void vGetOutputMatrix(){uint32_t n=arg(2)*arg(3);for(uint32_t i=0;i<n;++i)writef(arg(4)+i*4,0);ret(0,5);}
void vGetChannelVolumes(){for(uint32_t i=0;i<arg(1);++i)writef(arg(2)+i*4,1);ret(0,3);}
void vGetFilter(){std::memset(ptr(arg(1),12),0,12);ret(0,2);}
void vGetOutputFilter(){std::memset(ptr(arg(2),12),0,12);ret(0,3);}
void vDestroy(){auto f=xaVoices.find(objectId(arg(0)));if(f!=xaVoices.end())xaVoices.erase(f);ret(0,1);}
void sStart(){if(auto* v=xaVoice(arg(0))){v->running=true;}ret(0,3);}
void sStop(){if(auto* v=xaVoice(arg(0)))v->running=false;ret(0,3);}
void sSubmit(){
    auto* v=xaVoice(arg(0));uint32_t b=arg(1);if(!v||!b){ret(0x80070057u,3);return;}
    XaBuffer x{};x.flags=read32(b);x.bytes=read32(b+4);x.audio=read32(b+8);x.playBegin=read32(b+12);x.playLength=read32(b+16);
    x.loopBegin=read32(b+20);x.loopLength=read32(b+24);x.loopCount=read32(b+28);x.context=read32(b+32);
    uint64_t total=v->blockAlign?x.bytes/v->blockAlign:0;x.frames=x.playLength?x.playLength:(total>x.playBegin?total-x.playBegin:0);
    if(!x.frames)x.frames=1;v->queue.push_back(x);ret(0,3);
}
void sFlush(){
    auto* v=xaVoice(arg(0));if(!v){ret(0,1);return;}
    // flushed buffers still get OnBufferEnd, as on Windows (on the next pass)
    std::deque<XaBuffer> gone;if(!v->queue.empty()){auto first=v->queue.front();v->queue.pop_front();gone.swap(v->queue);v->queue.push_back(first);}
    uint32_t cb=v->callback;for(auto& b:gone)xaCallback(cb,4,{b.context});ret(0,1);
}
void sExitLoop(){if(auto* v=xaVoice(arg(0)))for(auto& b:v->queue)b.loopCount=0;ret(0,2);}
void sGetState(){
    auto* v=xaVoice(arg(0));uint32_t o=arg(1);
    write32(o,v&&!v->queue.empty()?v->queue.front().context:0);write32(o+4,v?uint32_t(v->queue.size()):0);
    uint64_t s=v?v->samplesPlayed:0;write32(o+8,uint32_t(s));write32(o+12,uint32_t(s>>32));ret(0,2);   // 2.7: GetState(pState) only
}
void sSetRatio(){if(auto* v=xaVoice(arg(0)))v->ratio=argf(1);ret(0,3);}
void sGetRatio(){auto* v=xaVoice(arg(0));writef(arg(1),v?v->ratio:1);ret(0,2);}
uint32_t createXAudio2(uint32_t out){
    if(xaEngineVtable.empty()){
        xaEngineVtable={hostMethod(xaQI),hostMethod(xaAddRef),hostMethod(xaRelease),hostMethod(xaDeviceCount),hostMethod(xaDeviceDetails),hostMethod(xaInitialize),
            hostMethod(xaRegister),hostMethod(xaUnregister),hostMethod(xaCreateSource),hostMethod(xaCreateSubmix),hostMethod(xaCreateMastering),
            hostMethod(xaStart),hostMethod(xaStop),hostMethod(xaCommit),hostMethod(xaPerformance),hostMethod(xaDebug)};
        xaVoiceVtable={hostMethod(vDetails),hostMethod(vOk2),hostMethod(vOk2),hostMethod(vOk3),hostMethod(vOk3),hostMethod(vGetEffectState),
            hostMethod(vOk5),hostMethod(vOk4),hostMethod(vOk3),hostMethod(vGetFilter),hostMethod(vOk4),hostMethod(vGetOutputFilter),
            hostMethod(vSetVolume),hostMethod(vGetVolume),hostMethod(vOk4),hostMethod(vGetChannelVolumes),hostMethod(vOk6),hostMethod(vGetOutputMatrix),
            hostMethod(vDestroy)};
        xaSourceVtable=xaVoiceVtable;
        for(auto fn:{sStart,sStop,sSubmit,sFlush,vOk1,sExitLoop,sGetState,sSetRatio,sGetRatio})xaSourceVtable.push_back(hostMethod(fn));
    }
    if(!xaEngine)xaEngine=guestObject(xaEngineVtable,0);
    if(out)write32(out,xaEngine);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","XAudio2 2.7 created (silent)");return 0;
}
