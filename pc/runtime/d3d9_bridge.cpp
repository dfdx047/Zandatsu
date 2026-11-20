#include "d3d9_bridge.h"
#include "runtime_log.h"
#include "dxt_shadow.h"
#include "vertex_formats.h"
#include <d3d9.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <cmath>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <vector>
#include <cstring>
#include <stdexcept>
#ifdef __ANDROID__
#include <android/native_window.h>
#endif
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

extern "C" IDirect3D9* Direct3DCreate9(UINT sdk);
extern "C" void dxvkAndroidSetWindow(ANativeWindow* window);
extern "C" HWND dxvkAndroidGetWindowHandle();
extern "C" void dxvkAndroidSetRenderSize(uint32_t width,uint32_t height);
namespace {
std::mutex overlayMutex;std::vector<float> overlayCircles;
// Draws the touch pad over the game's frame with fixed-function D3D9 (XYZRHW + DIFFUSE triangles), restoring all
// device state (state block + render target) afterwards.
void drawTouchOverlay(IDirect3DDevice9* d){
    std::vector<float> c;{std::lock_guard<std::mutex> l(overlayMutex);c=overlayCircles;}
    if(c.empty())return;
    IDirect3DSurface9 *bb=nullptr,*rt=nullptr;if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb)))return;
    D3DSURFACE_DESC desc{};bb->GetDesc(&desc);const float W=float(desc.Width),H=float(desc.Height);
    IDirect3DStateBlock9* sb=nullptr;d->CreateStateBlock(D3DSBT_ALL,&sb);d->GetRenderTarget(0,&rt);d->SetRenderTarget(0,bb);
    D3DVIEWPORT9 vp{0,0,desc.Width,desc.Height,0.f,1.f};d->SetViewport(&vp);
    d->BeginScene();
    d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);d->SetTexture(0,nullptr);
    const std::pair<D3DRENDERSTATETYPE,DWORD> rs[]={{D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,TRUE},{D3DRS_SRCBLEND,D3DBLEND_SRCALPHA},
        {D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA},{D3DRS_BLENDOP,D3DBLENDOP_ADD},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_LIGHTING,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},
        {D3DRS_STENCILENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_FOGENABLE,FALSE},{D3DRS_SRGBWRITEENABLE,FALSE},{D3DRS_COLORWRITEENABLE,0xF},
        {D3DRS_SEPARATEALPHABLENDENABLE,FALSE},{D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_MULTISAMPLEANTIALIAS,FALSE}};
    for(auto& [k,v]:rs)d->SetRenderState(k,v);
    d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
    struct V{float x,y,z,w;DWORD c;};std::vector<V> v;const int seg=40;
    for(size_t i=0;i+9<=c.size();i+=9){
        float cx=c[i]*W,cy=c[i+1]*H,r=c[i+2]*H;auto col=[&](float a){return D3DCOLOR_ARGB(int(a*255),int(c[i+3]*255),int(c[i+4]*255),int(c[i+5]*255));};
        bool filled=c[i+8]>0.5f;float inner=filled?0.f:r*0.82f;DWORD ring=col(c[i+6]),fill=col(c[i+6]*0.35f);
        for(int k=0;k<seg;++k){
            float a0=6.2831853f*k/seg,a1=6.2831853f*(k+1)/seg;
            V o0{cx+r*std::cos(a0),cy+r*std::sin(a0),0,1,ring},o1{cx+r*std::cos(a1),cy+r*std::sin(a1),0,1,ring};
            V i0{cx+inner*std::cos(a0),cy+inner*std::sin(a0),0,1,ring},i1{cx+inner*std::cos(a1),cy+inner*std::sin(a1),0,1,ring};
            v.insert(v.end(),{o0,o1,i0,i0,o1,i1});
            if(!filled){V m{cx,cy,0,1,fill},f0{i0.x,i0.y,0,1,fill},f1{i1.x,i1.y,0,1,fill};v.insert(v.end(),{m,f0,f1});}
        }
    }
    if(!v.empty())d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(v.size()/3),v.data(),sizeof(V));
    d->EndScene();
    d->SetRenderTarget(0,rt);if(rt)rt->Release();bb->Release();
    if(sb){sb->Apply();sb->Release();}
}
}
void setTouchOverlay(const float* circles,int count){std::lock_guard<std::mutex> l(overlayMutex);overlayCircles.assign(circles,circles+count*9);}
extern "C" void mgrRuntimeCounters(uint64_t* handoffs,uint64_t* wakes,uint64_t* spins);
void setD3D9RenderSize(unsigned width,unsigned height){dxvkAndroidSetRenderSize(width,height);}
namespace {
// Display modes offered to the game (its RESOLUTION option lists them). The DXVK Android WSI reports only a few
// (MGR showed 800x600, 1024x768, 1920x1080), so offer 16:9 sizes from 480p to 1080p plus the launcher's free size
// (MGR_RENDER_SIZE); the swapchain buffers take the chosen size and the compositor scales to the screen.
std::vector<std::pair<unsigned,unsigned>> displayModes(){
    std::vector<std::pair<unsigned,unsigned>> modes{{640,360},{800,450},{854,480},{960,540},{1024,576},{1152,648},{1280,720},
        {1366,768},{1600,900},{1920,1080},{800,600},{1024,768}};
    // Lab MGR_MODES="WxH,WxH,...": the exact mode list the game sees (to find which modes its options accept).
    if(const char* list=std::getenv("MGR_MODES")){modes.clear();unsigned a,b;int used=0;const char* q=list;
        while(std::sscanf(q,"%ux%u%n",&a,&b,&used)==2){modes.emplace_back(a,b);q+=used;if(*q!=',')break;++q;}}
    unsigned w=0,h=0;if(const char* s=std::getenv("MGR_RENDER_SIZE"))if(std::sscanf(s,"%ux%u",&w,&h)==2&&w>=320&&h>=200&&
        std::find(modes.begin(),modes.end(),std::make_pair(w,h))==modes.end())modes.emplace_back(w,h);
    std::sort(modes.begin(),modes.end(),[](auto a,auto b){return a.first*a.second<b.first*b.second||(a.first*a.second==b.first*b.second&&a.first<b.first);});
    return modes;
}
bool modeFormat(D3DFORMAT f){return f==D3DFMT_X8R8G8B8||f==D3DFMT_A8R8G8B8||f==D3DFMT_R5G6B5||f==D3DFMT_X1R5G5B5;}
}
#ifndef __ANDROID__
extern "C" ANativeWindow* dxvkCreateTestWindow();
extern "C" void dxvkDestroyTestWindow(ANativeWindow* window);
#endif
namespace {
constexpr uint32_t methodBase=0x7e000000;
enum class Kind:uint32_t {D3D9=1,Device=2,Texture=3,Cube=4,Surface=5,VertexBuffer=6,IndexBuffer=7,
    VertexShader=8,PixelShader=9,Declaration=10,StateBlock=11,Query=12,SwapChain=13,VolumeTexture=14,Volume=15};
struct Object {IUnknown* native;Kind kind;uint32_t references=1;CompressedView compressed;uint32_t parent=0;uint32_t children=0;uint64_t psHash=0;};
std::unordered_map<uint32_t,Object> objects;
// Nearly every call of a frame targets the same object (the device). Element
// pointers of an unordered_map survive rehashing; only erasing an object
// invalidates them, so retireGuestObject/shutdown drop this one-entry cache.
uint32_t cachedObjectAddress=0;Object* cachedObject=nullptr;
Object* findObject(uint32_t address){
    if(address==cachedObjectAddress&&cachedObject)return cachedObject;
    auto found=objects.find(address);if(found==objects.end())return nullptr;
    cachedObjectAddress=address;cachedObject=&found->second;return cachedObject;
}
std::map<IUnknown*,uint32_t> addresses;
void* guestBase;
uint32_t (*allocateGuest)(uint32_t);
void (*freeGuest)(uint32_t);
std::atomic<ANativeWindow*> hostWindow{nullptr};
std::atomic<bool> surfaceRequested{false},surfaceAvailable{false};
void* pointer(uint32_t address){return address?static_cast<uint8_t*>(guestBase)+address:nullptr;}
void write(uint32_t address,uint32_t value){if(!address)throw std::runtime_error("Null D3D9 output");std::memcpy(pointer(address),&value,4);}
uint32_t wrap(IUnknown* native,Kind kind){
    if(!native)return 0;auto previous=addresses.find(native);if(previous!=addresses.end()){++objects.at(previous->second).references;return previous->second;}
    static constexpr uint32_t sizes[]={0,17,119,22,22,17,14,14,5,5,5,6,8,10,22,11};
    uint32_t count=sizes[static_cast<uint32_t>(kind)];
    uint32_t table=allocateGuest(count*4),object=allocateGuest(4);
    for(uint32_t i=0;i<count;++i)write(table+i*4,methodBase+static_cast<uint32_t>(kind)*4096+i*16);
    write(object,table);objects.emplace(object,Object{native,kind});addresses.emplace(native,object);return object;
}
// The guest handle's kind decides the interface: D3D9 interfaces form single
// inheritance chains from IUnknown, so once the kind matches, static_cast is
// exact. dynamic_cast on every SetTexture/SetStreamSource cost ~3% of a race.
template<class T>bool kindMatches(Kind kind){
    if constexpr(std::is_same_v<T,IDirect3DSurface9>)return kind==Kind::Surface;
    else if constexpr(std::is_same_v<T,IDirect3DBaseTexture9>)return kind==Kind::Texture||kind==Kind::Cube||kind==Kind::VolumeTexture;
    else if constexpr(std::is_same_v<T,IDirect3DTexture9>)return kind==Kind::Texture;
    else if constexpr(std::is_same_v<T,IDirect3DCubeTexture9>)return kind==Kind::Cube;
    else if constexpr(std::is_same_v<T,IDirect3DVertexBuffer9>)return kind==Kind::VertexBuffer;
    else if constexpr(std::is_same_v<T,IDirect3DIndexBuffer9>)return kind==Kind::IndexBuffer;
    else if constexpr(std::is_same_v<T,IDirect3DVertexShader9>)return kind==Kind::VertexShader;
    else if constexpr(std::is_same_v<T,IDirect3DPixelShader9>)return kind==Kind::PixelShader;
    else if constexpr(std::is_same_v<T,IDirect3DVertexDeclaration9>)return kind==Kind::Declaration;
    else return false;
}
template<class T>T* nativeObject(uint32_t address){
    if(!address)return nullptr;auto found=objects.find(address);if(found==objects.end())throw std::runtime_error("Invalid D3D9 resource handle");
    if(kindMatches<T>(found->second.kind))return static_cast<T*>(found->second.native);
    auto native=dynamic_cast<T*>(found->second.native);if(!native)throw std::runtime_error("D3D9 resource interface mismatch");return native;
}
uint32_t wrapTexture(IDirect3DBaseTexture9* texture){if(!texture)return 0;switch(texture->GetType()){case D3DRTYPE_TEXTURE:return wrap(texture,Kind::Texture);case D3DRTYPE_CUBETEXTURE:return wrap(texture,Kind::Cube);case D3DRTYPE_VOLUMETEXTURE:return wrap(texture,Kind::VolumeTexture);default:throw std::runtime_error("Unknown D3D9 base texture type");}}
void retireGuestObject(uint32_t address){
    cachedObjectAddress=0;cachedObject=nullptr;
    auto found=objects.find(address);if(found==objects.end())return;
    uint32_t table;std::memcpy(&table,pointer(address),4);
    freeGuest(table);freeGuest(address);addresses.erase(found->second.native);objects.erase(found);
}
void attachTextureSurface(uint32_t texture,uint32_t surface){
    if(!surface)return;
    auto& child=objects.at(surface);
    if(!child.parent){child.parent=texture;++objects.at(texture).children;}
    else if(child.parent!=texture)throw std::runtime_error("D3D9 subresource changed parent");
}
// Underground 2 copies its initial depth surface from 0x870854 into the first
// render surface descriptor at 0x86e774, then releases both on device Reset.
// The copy does not call AddRef, so account for that owned descriptor here.
void retainInitialDepthDescriptor(uint32_t outputAddress,uint32_t surfaceAddress){
    if(outputAddress!=0x870854||!surfaceAddress)return;
    auto& surface=objects.at(surfaceAddress);surface.native->AddRef();++surface.references;
}
float floatValue(uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
uint32_t floatBits(float value){uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
CompressedView compressionView(uint32_t address){auto found=objects.find(address);return found==objects.end()?CompressedView{}:found->second.compressed;}
// DXVK lets the CPU run at most BackBufferCount+1 frames ahead of the GPU.
// Fewer back buffers (the game's request is logged) serialise a CPU-heavy frame
// with its GPU work (Present waited 7-10 ms with the GPU 67% busy). Two
// back buffers allow DXVK's default latency of 3; the game only ever reads
// back buffer 0, so the extra buffers are invisible to it.
// Launcher "Sincronía CPU/GPU": 2 (default) or 1 for the lowest input latency.
std::atomic<uint32_t> configuredBackBuffers{2};
// Launcher "Texturas comprimidas: convertir en CPU": DXT decoded to RGBA8 even when the
// driver reports BC support, for drivers whose BC sampling is broken.
std::atomic<bool> forceCpuTextures{false};
uint32_t hostBackBufferCount(uint32_t requested){
    uint32_t count=std::max(requested,configuredBackBuffers.load());
    static bool logged=false;
    if(!logged){logged=true;NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 game back buffers %u -> host %u",requested,count);}
    return count;
}
D3DPRESENT_PARAMETERS presentation(uint32_t address){
    auto p=static_cast<const uint32_t*>(pointer(address));if(!p)throw std::runtime_error("Null D3D9 presentation parameters");
    D3DPRESENT_PARAMETERS result{};result.BackBufferWidth=p[0];result.BackBufferHeight=p[1];result.BackBufferFormat=static_cast<D3DFORMAT>(p[2]);result.BackBufferCount=hostBackBufferCount(p[3]);
    result.MultiSampleType=static_cast<D3DMULTISAMPLE_TYPE>(p[4]);result.MultiSampleQuality=p[5];result.SwapEffect=static_cast<D3DSWAPEFFECT>(p[6]);result.hDeviceWindow=dxvkAndroidGetWindowHandle();
    result.Windowed=p[8];result.EnableAutoDepthStencil=p[9];result.AutoDepthStencilFormat=static_cast<D3DFORMAT>(p[10]);result.Flags=p[11];result.FullScreen_RefreshRateInHz=p[12];result.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;return result;
}
#include "d3d9_locks.inc"
namespace {
// Frame counter and optional backbuffer capture: NFS_D3D9_CAPTURE=<dir>[,<every N frames>] writes <dir>/frame_NNNNNN.ppm
// (BGRA/XRGB backbuffers only). Proof for the headless Linux bench and GPU diagnostics; costs a GPU readback per capture.
std::atomic<uint64_t> presentedFrames{0};
void captureBackbuffer(IDirect3DDevice9* device,uint64_t frame){
    static const char* spec=std::getenv("NFS_D3D9_CAPTURE");if(!spec||!*spec)return;
    static std::string dir;static uint64_t every=0;
    if(!every){std::string value=spec;auto comma=value.find(',');dir=value.substr(0,comma);every=comma==std::string::npos?120:std::max<uint64_t>(1,std::strtoull(value.c_str()+comma+1,nullptr,10));}
    if(frame%every)return;
    IDirect3DSurface9* back=nullptr;if(FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||!back)return;
    D3DSURFACE_DESC desc{};back->GetDesc(&desc);IDirect3DSurface9* copy=nullptr;
    if(desc.Format==D3DFMT_A8R8G8B8||desc.Format==D3DFMT_X8R8G8B8){
        if(SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr))&&
           SUCCEEDED(device->GetRenderTargetData(back,copy))){
            D3DLOCKED_RECT lock{};
            if(SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY))){
                char name[64];std::snprintf(name,sizeof name,"/frame_%06llu.ppm",static_cast<unsigned long long>(frame));
                if(FILE* out=std::fopen((dir+name).c_str(),"wb")){
                    std::fprintf(out,"P6\n%u %u\n255\n",desc.Width,desc.Height);std::vector<uint8_t> row(desc.Width*3);
                    for(uint32_t y=0;y<desc.Height;++y){auto src=static_cast<const uint8_t*>(lock.pBits)+size_t(y)*lock.Pitch;
                        for(uint32_t x=0;x<desc.Width;++x){row[x*3]=src[x*4+2];row[x*3+1]=src[x*4+1];row[x*3+2]=src[x*4];}
                        std::fwrite(row.data(),1,row.size(),out);}
                    std::fclose(out);NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","Captured frame %llu to %s%s",static_cast<unsigned long long>(frame),dir.c_str(),name);
                }
                copy->UnlockRect();
            }
        }
        if(copy)copy->Release();
    }
    back->Release();
}
// MGR_SKIP_PS="HASH,HASH" (hex, FNV-1a 64 of the shader tokens): draws with these pixel shaders are skipped (the PC
// counterpart of swordman_skip_ps, to measure or cut effects). MGR_PS_STATS=<frames>: every <frames> presents, logs
// the draws per pixel shader and render target (size, format) seen, to name the shaders of each pass.
uint64_t currentPixelShader=0;
struct TargetDesc{uint32_t w=0,h=0,format=0;};TargetDesc currentTarget;
uint64_t shaderHash(const DWORD* code){
    uint64_t h=1469598103934665603ull;if(!code)return 0;
    for(size_t i=0;i<65536;++i){h=(h^code[i])*1099511628211ull;if(code[i]==0x0000FFFFu)break;}
    return h;
}
std::unordered_set<uint64_t>& skipSet(){
    static std::unordered_set<uint64_t> set=[]{std::unordered_set<uint64_t> r;const char* s=std::getenv("MGR_SKIP_PS");
        while(s&&*s){char* end=nullptr;uint64_t v=std::strtoull(s,&end,16);if(end==s)break;r.insert(v);s=*end?end+1:end;}
        if(!r.empty())NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","skip-ps: %zu pixel shaders skipped",r.size());return r;}();
    return set;
}
bool skippedPixelShader(uint64_t hash){auto& set=skipSet();return hash&&!set.empty()&&set.count(hash);}
uint32_t psStatsFrames(){static const uint32_t n=[]{const char* s=std::getenv("MGR_PS_STATS");return s?uint32_t(std::atoi(s)):0u;}();return n;}
std::map<std::tuple<uint64_t,uint32_t,uint32_t,uint32_t>,uint64_t> psDraws;
void countPixelShaderDraw(){++psDraws[{currentPixelShader,currentTarget.w,currentTarget.h,currentTarget.format}];}
void trackRenderTarget(uint32_t address){
    currentTarget={};auto found=address?objects.find(address):objects.end();if(found==objects.end())return;
    D3DSURFACE_DESC desc{};if(SUCCEEDED(static_cast<IDirect3DSurface9*>(found->second.native)->GetDesc(&desc)))currentTarget={desc.Width,desc.Height,uint32_t(desc.Format)};
}
void logPixelShaderStats(uint64_t frame){
    uint32_t n=psStatsFrames();if(!n||frame%n)return;
    std::vector<std::pair<uint64_t,std::tuple<uint64_t,uint32_t,uint32_t,uint32_t>>> rows;for(auto& [k,v]:psDraws)rows.emplace_back(v,k);
    std::sort(rows.rbegin(),rows.rend());
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","ps-stats frames %u: %zu (shader, target) pairs",n,rows.size());
    for(size_t i=0;i<rows.size()&&i<120;++i){auto& [count,k]=rows[i];
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","ps-stats %016llX rt %ux%u fmt %u  %.1f draws/frame",static_cast<unsigned long long>(std::get<0>(k)),std::get<1>(k),std::get<2>(k),std::get<3>(k),double(count)/n);}
    psDraws.clear();
}
// Lab MGR_MEMSNAP=<dir>,<first frame>,<every>,<count>: dumps the exe's .data/.bss (0x01880000, 0xB6FA30 bytes) as
// <dir>/snap_<frame>_<steady ns>.bin, to find the game's clocks (per-frame or real-time) by diffing snapshots.
void memorySnapshot(uint64_t frame){
    static const char* spec=std::getenv("MGR_MEMSNAP");if(!spec||!*spec)return;
    static std::string dir;static uint64_t first=0,every=0,count=0;
    if(!every){std::string v=spec;char d[256]={};unsigned long long a=0,b=0,c=0;
        if(std::sscanf(v.c_str(),"%255[^,],%llu,%llu,%llu",d,&a,&b,&c)!=4||!b){spec=nullptr;return;}dir=d;first=a;every=b;count=c;}
    if(frame<first||(frame-first)%every||(frame-first)/every>=count)return;
    auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    char name[96];std::snprintf(name,sizeof name,"/snap_%06llu_%lld.bin",static_cast<unsigned long long>(frame),static_cast<long long>(ns));
    if(FILE* out=std::fopen((dir+name).c_str(),"wb")){std::fwrite(pointer(0x01880000u),1,0xB6FA30u,out);std::fclose(out);
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","Memory snapshot %s",name);}
}
// Lab position probe (pilot step "probe N"): a Cheat-Engine style search for the player's position, to measure the
// physics bug (characters above the ground) as a number. Candidates are 16-byte aligned vec4 with w = 1 in the guest
// heap arenas (an object's position, or the translation row of a bone matrix). 0 = collect (standing), 1 = keep the
// unchanged (standing), 2 = keep the changed (after walking), 3 = keep the unchanged (standing), 4 = keep the changed,
// 9 = log the first survivors every frame ("pos-watch"), 10 = stop. Lab only: nothing runs without the pilot step.
struct PosCand{uint32_t va;float x,y,z;};
std::vector<PosCand> posCands;std::atomic<bool> posWatching{false};std::mutex posLock;
bool posRead(uint32_t va,float* v){std::memcpy(v,pointer(va),16);
    return v[3]==1.0f&&std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2])&&std::fabs(v[0])<1e5f&&std::fabs(v[1])<1e5f&&
        std::fabs(v[2])<1e5f&&(v[0]!=0.f||v[1]!=0.f||v[2]!=0.f);}
void posProbeStage(int stage){
    std::lock_guard<std::mutex> g(posLock);size_t before=posCands.size();
    if(stage==0){posCands.clear();
        const uint32_t ranges[2][2]={{0x10000000u,0x1f000000u},{0x30000000u,0x70000000u}};
        for(auto& r:ranges)for(uint32_t va=r[0];va<r[1];va+=16){float v[4];if(posRead(va,v))posCands.push_back({va,v[0],v[1],v[2]});}
    }else if(stage>=1&&stage<=4){bool keepChanged=stage==2||stage==4;std::vector<PosCand> out;out.reserve(posCands.size());
        for(auto c:posCands){float v[4];if(!posRead(c.va,v))continue;
            float dx=v[0]-c.x,dy=v[1]-c.y,dz=v[2]-c.z;bool same=dx==0.f&&dy==0.f&&dz==0.f;
            bool moved=std::fmax(std::fabs(dx),std::fabs(dz))>0.2f&&std::fabs(dx)<100.f&&std::fabs(dy)<100.f&&std::fabs(dz)<100.f;
            if(keepChanged?moved:same)out.push_back({c.va,v[0],v[1],v[2]});}
        posCands.swap(out);
    }else if(stage==9)posWatching=true;else if(stage==10)posWatching=false;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","pos-probe stage %d: %zu -> %zu candidates",stage,before,posCands.size());
    if(stage==3||stage==4)for(size_t i=0;i<posCands.size()&&i<48;i++)
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","pos-cand %08X %.3f %.3f %.3f",posCands[i].va,posCands[i].x,posCands[i].y,posCands[i].z);
}
void posWatchFrame(uint64_t frame){
    if(!posWatching.load(std::memory_order_relaxed))return;std::lock_guard<std::mutex> g(posLock);
    char line[1024];int n=std::snprintf(line,sizeof line,"pos-watch %llu",static_cast<unsigned long long>(frame));
    for(size_t i=0;i<posCands.size()&&i<12&&n<900;i++){float v[4];std::memcpy(v,pointer(posCands[i].va),16);
        n+=std::snprintf(line+n,sizeof line-n," %08X:%.3f,%.3f,%.3f",posCands[i].va,v[0],v[1],v[2]);}
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","%s",line);
}
void countFrame(){
    using clock=std::chrono::steady_clock;static clock::time_point last=clock::now();static uint64_t lastFrame=0;
    uint64_t frame=++presentedFrames;memorySnapshot(frame);posWatchFrame(frame);logPixelShaderStats(frame);auto now=clock::now();double seconds=std::chrono::duration<double>(now-last).count();
    if(seconds>=5.0){uint64_t h=0,w=0,sp=0;mgrRuntimeCounters(&h,&w,&sp);static uint64_t lh=0,lw=0,lsp=0;
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","Present: frame %llu, %.1f fps over %.1f s; machine futex handoffs %.0f/s, spin handoffs %.0f/s, signal wakes %.0f/s",static_cast<unsigned long long>(frame),(frame-lastFrame)/seconds,seconds,(h-lh)/seconds,(sp-lsp)/seconds,(w-lw)/seconds);lh=h;lw=w;lsp=sp;last=now;lastFrame=frame;}
}
}
bool compressedFormat(D3DFORMAT format){return format>=D3DFMT_DXT1&&format<=D3DFMT_DXT5;}
D3DFORMAT nativeTextureFormat(IDirect3DDevice9* device,D3DFORMAT format,DWORD usage,D3DRESOURCETYPE type){
    if(!compressedFormat(format)||(usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL)))return format;
    if(forceCpuTextures.load())return D3DFMT_A8B8G8R8;
    IDirect3D9* d3d=nullptr;D3DDEVICE_CREATION_PARAMETERS parameters{};D3DDISPLAYMODE mode{};
    if(FAILED(device->GetDirect3D(&d3d)))return format;
    device->GetCreationParameters(&parameters);d3d->GetAdapterDisplayMode(parameters.AdapterOrdinal,&mode);
    HRESULT supported=d3d->CheckDeviceFormat(parameters.AdapterOrdinal,parameters.DeviceType,mode.Format,usage,type,format);d3d->Release();
    return SUCCEEDED(supported)?format:D3DFMT_A8B8G8R8;
}
void describeOriginal(uint32_t address,D3DSURFACE_DESC* desc){auto view=compressionView(address);if(view.texture&&desc)desc->Format=static_cast<D3DFORMAT>(view.texture->format);}
}
void setD3D9Compatibility(bool cpuTextures,unsigned backBuffers){
    forceCpuTextures=cpuTextures;configuredBackBuffers=backBuffers<1?1u:backBuffers>3?3u:backBuffers;
}
void configureD3D9Bridge(void* base,uint32_t (*allocator)(uint32_t),void (*deallocator)(uint32_t)){
    guestBase=base;allocateGuest=allocator;freeGuest=deallocator;
#ifdef __ANDROID__
    // android_main provides a writable private cache before creating DXVK.
    // Other bring-up callers can still run without a writable working directory.
    if(!std::getenv("DXVK_STATE_CACHE_PATH"))setenv("DXVK_STATE_CACHE","0",1);
    setenv("DXVK_LOG_PATH","none",1);
#endif
#ifndef __ANDROID__
    if(std::getenv("NFS_D3D9_TEST_DISPLAY")){auto window=dxvkCreateTestWindow();if(!window)throw std::runtime_error("Xlib test display unavailable");setD3D9HostWindow(window);acknowledgeD3D9Surface(true);}
#endif
}
void shutdownD3D9Bridge(){
    for(auto& lock:resourceLocks)freeGuest(lock.second.guest);resourceLocks.clear();
    for(uint32_t kind=15;kind;--kind)for(auto it=objects.begin();it!=objects.end();){
        if(static_cast<uint32_t>(it->second.kind)!=kind){++it;continue;}
        auto object=it->second;for(uint32_t n=0;n<object.references;++n)object.native->Release();
        cachedObjectAddress=0;cachedObject=nullptr;
        uint32_t table;std::memcpy(&table,pointer(it->first),4);freeGuest(table);freeGuest(it->first);addresses.erase(object.native);it=objects.erase(it);
    }
    surfaceRequested=false;surfaceAvailable=false;
#ifndef __ANDROID__
    if(std::getenv("NFS_D3D9_TEST_DISPLAY")){auto window=hostWindow.load();setD3D9HostWindow(nullptr);dxvkDestroyTestWindow(window);}
#endif
}
void setD3D9HostWindow(ANativeWindow* window){hostWindow=window;dxvkAndroidSetWindow(window);}
bool d3d9RequestsSurface(){return surfaceRequested;}
void acknowledgeD3D9Surface(bool available){surfaceAvailable=available;}
uint32_t createGuestD3D9(uint32_t sdk){
    auto native=Direct3DCreate9(sdk);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","DXVK native Direct3DCreate9 sdk=%u",sdk);
    return wrap(native,Kind::D3D9);
}
// Logs each vertex declaration type the first time the game uses it, flagging the
// ones this GPU cannot fetch (see vertex_formats.h): the evidence for stretched geometry.
void noteVertexDeclaration(const D3DVERTEXELEMENT9* elements){
    static bool seen[32]{};
    for(unsigned i=0;elements&&i<64&&elements[i].Stream!=0xff;++i){
        unsigned type=elements[i].Type;
        if(type>=32||seen[type])continue;
        seen[type]=true;
        const char* name="?";for(auto& entry:kD3DVertexFormats)if(entry.type==type)name=entry.name;
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 game vertex type %s (usage %u)",name,unsigned(elements[i].Usage));
    }
}
// Set when DXVK could not rebuild the swap chain in Reset. DXVK's TestCooperativeLevel
// always reports D3D_OK, so the game would keep drawing on the half-reset device and
// crash (null dereference, Mali reports); until a Reset succeeds the device reads as
// lost and needing a reset, which the game handles as on Windows.
bool deviceResetPending=false;
uint32_t dispatchGuestD3D9(uint32_t token,const uint32_t* args,uint32_t* argumentCount){
    uint32_t kind=(token-methodBase)/4096,method=((token-methodBase)%4096)/16;
    Object* entry=findObject(args[0]);
    if(!entry||static_cast<uint32_t>(entry->kind)!=kind){
        NFS_RUNTIME_LOG(ANDROID_LOG_ERROR,"NFSU2","Invalid D3D9 COM call object=%08x token=%08x kind=%u method=%u actualKind=%u",args[0],token,kind,method,
            entry?static_cast<uint32_t>(entry->kind):0);
        throw std::runtime_error("Invalid guest D3D9 COM object");
    }
    auto object=entry->native;
    // Android may replace ANativeWindow after backgrounding, even at the same
    // pointer address. Override the destination with this surface generation so
    // DXVK recreates its presenter while keeping the game's device/resources.
    if((kind==2&&method==17)||(kind==13&&method==3)){
        *argumentCount=kind==2?5:6;auto window=dxvkAndroidGetWindowHandle();
        if(!window||deviceResetPending)return D3DERR_DEVICELOST;
        auto source=static_cast<const RECT*>(pointer(args[1])),destination=static_cast<const RECT*>(pointer(args[2]));
        auto dirty=static_cast<const RGNDATA*>(pointer(args[4]));
        if(kind==2)drawTouchOverlay(static_cast<IDirect3DDevice9*>(object));
        if(kind==2){countFrame();captureBackbuffer(static_cast<IDirect3DDevice9*>(object),presentedFrames.load());}
        return kind==2?static_cast<IDirect3DDevice9*>(object)->Present(source,destination,window,dirty):
            static_cast<IDirect3DSwapChain9*>(object)->Present(source,destination,window,dirty,args[5]);
    }
    // Log the first calls of each method; tokens span 0x7e000000..0x7e00ffff.
    static uint8_t methodCalls[4096];auto& calls=methodCalls[((token-methodBase)/16)&4095];
    if(calls<3&&++calls)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 COM kind=%u method=%u",kind,method);
    // Counts belong to the guest COM wrapper. DXVK texture subresources
    // delegate AddRef/Release to their parent texture, so native counts can
    // include ownership through a different guest object. Returning them made
    // the game's release-until-zero loop call an already freed surface wrapper.
    if(method==1){*argumentCount=1;object->AddRef();return ++entry->references;}
    if(method==2){*argumentCount=1;
        // The game's detail rebuild releases the cube parent after every face
        // lookup, although only its first Release owns a parent reference.
        // Keep the callable wrapper until the acquired faces are released.
        if(!entry->references){
            if(entry->children)return 0;
            throw std::runtime_error("D3D9 COM Release without owned reference");
        }
        auto nativeReferences=object->Release();uint32_t references=--entry->references;
        if(kind==5)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Surface Release object=%08x guestRefs=%u nativeRefs=%u",args[0],references,uint32_t(nativeReferences));
        if(!references){
            if(entry->children)return uint32_t(nativeReferences);
            auto parent=entry->parent;retireGuestObject(args[0]);
            if(parent){auto owner=objects.find(parent);if(owner!=objects.end()&&!--owner->second.children&&!owner->second.references)retireGuestObject(parent);}
        }
        return references;}
    if(method==0){
        *argumentCount=3;IUnknown* result=nullptr;HRESULT status=object->QueryInterface(*static_cast<const GUID*>(pointer(args[1])),reinterpret_cast<void**>(&result));
        if(SUCCEEDED(status))write(args[2],wrap(result,entry->kind));else write(args[2],0);return status;
    }
    if(kind==1){
        auto d3d=static_cast<IDirect3D9*>(object);
        switch(method){
        case 3:*argumentCount=2;return D3DERR_NOTAVAILABLE;
        case 4:*argumentCount=1;return d3d->GetAdapterCount();
        case 5:*argumentCount=4;return d3d->GetAdapterIdentifier(args[1],args[2],static_cast<D3DADAPTER_IDENTIFIER9*>(pointer(args[3])));
        case 6:*argumentCount=3;return modeFormat(static_cast<D3DFORMAT>(args[2]))?uint32_t(displayModes().size()):0;
        case 7:{*argumentCount=5;auto modes=displayModes();auto* mode=static_cast<D3DDISPLAYMODE*>(pointer(args[4]));
            if(!modeFormat(static_cast<D3DFORMAT>(args[2]))||args[3]>=modes.size()||!mode)return D3DERR_INVALIDCALL;
            mode->Width=modes[args[3]].first;mode->Height=modes[args[3]].second;mode->RefreshRate=60;mode->Format=static_cast<D3DFORMAT>(args[2]);
            {static unsigned logged=0;if(logged++<64)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","EnumAdapterModes %u -> %ux%u",args[3],mode->Width,mode->Height);}
            return D3D_OK;}
        case 8:*argumentCount=3;return d3d->GetAdapterDisplayMode(args[1],static_cast<D3DDISPLAYMODE*>(pointer(args[2])));
        case 9:*argumentCount=6;return d3d->CheckDeviceType(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),static_cast<D3DFORMAT>(args[4]),args[5]);
        case 10:{*argumentCount=7;auto format=static_cast<D3DFORMAT>(args[6]);auto type=static_cast<D3DRESOURCETYPE>(args[5]);
            HRESULT status=d3d->CheckDeviceFormat(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),args[4],type,format);
            if(FAILED(status)&&compressedFormat(format)&&!(args[4]&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))&&(type==D3DRTYPE_TEXTURE||type==D3DRTYPE_CUBETEXTURE||type==D3DRTYPE_SURFACE))
                status=d3d->CheckDeviceFormat(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),args[4],type,D3DFMT_A8B8G8R8);
            return status;}
        case 11:*argumentCount=7;return d3d->CheckDeviceMultiSampleType(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),args[4],static_cast<D3DMULTISAMPLE_TYPE>(args[5]),static_cast<DWORD*>(pointer(args[6])));
        case 12:*argumentCount=6;return d3d->CheckDepthStencilMatch(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),static_cast<D3DFORMAT>(args[4]),static_cast<D3DFORMAT>(args[5]));
        case 13:*argumentCount=5;return d3d->CheckDeviceFormatConversion(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),static_cast<D3DFORMAT>(args[4]));
        case 14:*argumentCount=4;return d3d->GetDeviceCaps(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DCAPS9*>(pointer(args[3])));
        case 15:*argumentCount=2;return d3d->GetAdapterMonitor(args[1])?0xf0000300:0;
        case 16:{
            *argumentCount=7;auto p=static_cast<const uint32_t*>(pointer(args[5]));if(!p||!args[6])return D3DERR_INVALIDCALL;
            if(!hostWindow.load())throw std::runtime_error("D3D9 CreateDevice requires Android surface");
            surfaceRequested=true;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while(!surfaceAvailable){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Android surface handover timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
            auto window=dxvkAndroidGetWindowHandle();if(!window)return D3DERR_DEVICELOST;
            D3DPRESENT_PARAMETERS parameters{};parameters.BackBufferWidth=p[0];parameters.BackBufferHeight=p[1];parameters.BackBufferFormat=static_cast<D3DFORMAT>(p[2]);parameters.BackBufferCount=hostBackBufferCount(p[3]);
            parameters.MultiSampleType=static_cast<D3DMULTISAMPLE_TYPE>(p[4]);parameters.MultiSampleQuality=p[5];parameters.SwapEffect=static_cast<D3DSWAPEFFECT>(p[6]);parameters.hDeviceWindow=window;
            parameters.Windowed=p[8];parameters.EnableAutoDepthStencil=p[9];parameters.AutoDepthStencilFormat=static_cast<D3DFORMAT>(p[10]);parameters.Flags=p[11];parameters.FullScreen_RefreshRateInHz=p[12];parameters.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
            IDirect3DDevice9* device=nullptr;HRESULT status=d3d->CreateDevice(args[1],static_cast<D3DDEVTYPE>(args[2]),window,args[4],&parameters,&device);
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 backbuffer %ux%u CreateDevice result=%08x",parameters.BackBufferWidth,parameters.BackBufferHeight,uint32_t(status));
            // Without a device the game carries on and faults later in its own
            // code (PowerVR GE8322 reports): after its retry also fails, stop
            // here with the reason instead.
            static unsigned failures=0;failures=FAILED(status)?failures+1:0;
            if(FAILED(status)&&failures<2)return status;
            if(FAILED(status))throw std::runtime_error("La GPU no pudo crear el dispositivo grafico (Vulkan, CreateDevice "+
                std::to_string(uint32_t(status))+"). Usa \"Probar driver\" en el launcher para ver que le falta.");
            write(args[6],wrap(device,Kind::Device));return status;
        }
        }
    }
    if(kind==2){auto device=static_cast<IDirect3DDevice9*>(object);
        // Pixel shader identity (MGR_SKIP_PS / MGR_PS_STATS): bytecode hash kept on the guest shader object.
        if(method==106){*argumentCount=3;IDirect3DPixelShader9* shader=nullptr;auto code=static_cast<const DWORD*>(pointer(args[1]));
            HRESULT status=device->CreatePixelShader(code,args[2]?&shader:nullptr);
            if(args[2]){uint32_t address=SUCCEEDED(status)?wrap(shader,Kind::PixelShader):0;if(address)objects.at(address).psHash=shaderHash(code);write(args[2],address);}
            return status;}
        if(method==107){auto found=args[1]?objects.find(args[1]):objects.end();currentPixelShader=found!=objects.end()?found->second.psHash:0;}
        if(method==37&&args[1]==0)trackRenderTarget(args[2]);
        if(method>=81&&method<=84){
            if(psStatsFrames())countPixelShaderDraw();
            if(skippedPixelShader(currentPixelShader)){static const uint8_t counts[4]={4,7,5,9};*argumentCount=counts[method-81];return D3D_OK;}}
        switch(method){
        case 9:{*argumentCount=2;D3DDEVICE_CREATION_PARAMETERS p{};HRESULT status=device->GetCreationParameters(&p);if(SUCCEEDED(status)){write(args[1],p.AdapterOrdinal);write(args[1]+4,p.DeviceType);write(args[1]+8,0xf0000301);write(args[1]+12,p.BehaviorFlags);}return status;}
        case 13:{*argumentCount=3;auto p=presentation(args[1]);IDirect3DSwapChain9* result=nullptr;HRESULT status=device->CreateAdditionalSwapChain(&p,&result);write(args[2],SUCCEEDED(status)?wrap(result,Kind::SwapChain):0);return status;}
        case 16:{*argumentCount=2;auto p=presentation(args[1]);HRESULT status=D3DERR_DEVICELOST;
            // DXVK throws (DxvkError) when it cannot rebuild the swap chain; Windows
            // would report a lost device, which the game retries (Mali G615 report).
            // The old Android window can stay busy for a moment: retry once before giving up.
            for(int attempt=0;attempt<2;++attempt){
                try{status=device->Reset(&p);break;}catch(...){
                    NFS_RUNTIME_LOG(ANDROID_LOG_ERROR,"NFSU2","D3D9 Reset raised a DXVK error (attempt %d)",attempt+1);status=D3DERR_DEVICELOST;
                    if(!attempt)std::this_thread::sleep_for(std::chrono::milliseconds(100));}
            }
            deviceResetPending=status==D3DERR_DEVICELOST;
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 Reset backbuffer %ux%u result=%08x",p.BackBufferWidth,p.BackBufferHeight,uint32_t(status));return status;}
        case 23:{*argumentCount=9;if(args[8])throw std::runtime_error("Shared texture handle pending");if(!args[7])return D3DERR_INVALIDCALL;
            auto original=static_cast<D3DFORMAT>(args[5]),format=nativeTextureFormat(device,original,args[4],D3DRTYPE_TEXTURE);IDirect3DTexture9* texture=nullptr;
            HRESULT status=device->CreateTexture(args[1],args[2],args[3],args[4],format,static_cast<D3DPOOL>(args[6]),&texture,nullptr);
            uint32_t address=SUCCEEDED(status)?wrap(texture,Kind::Texture):0;
            if(address&&format!=original){objects.at(address).compressed.texture=std::make_shared<CompressedTexture>(original,args[1],args[2],texture->GetLevelCount(),1);NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","CPU DXT fallback texture %ux%u format=%u",args[1],args[2],original);}
            write(args[7],address);return status;}
        case 25:{*argumentCount=8;if(args[7])throw std::runtime_error("Shared cube handle pending");if(!args[6])return D3DERR_INVALIDCALL;
            auto original=static_cast<D3DFORMAT>(args[4]),format=nativeTextureFormat(device,original,args[3],D3DRTYPE_CUBETEXTURE);IDirect3DCubeTexture9* texture=nullptr;
            HRESULT status=device->CreateCubeTexture(args[1],args[2],args[3],format,static_cast<D3DPOOL>(args[5]),&texture,nullptr);
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 CreateCubeTexture edge=%u levels=%u usage=%08x format=%u nativeFormat=%u pool=%u status=%08x output=%08x",args[1],args[2],args[3],unsigned(original),unsigned(format),args[5],uint32_t(status),args[6]);
            uint32_t address=SUCCEEDED(status)?wrap(texture,Kind::Cube):0;
            if(address&&format!=original)objects.at(address).compressed.texture=std::make_shared<CompressedTexture>(original,args[1],args[1],texture->GetLevelCount(),6);
            write(args[6],address);return status;}
#include "d3d9_device_methods.inc"
    }}
    if(kind==3){auto texture=static_cast<IDirect3DTexture9*>(object);
        if(method==17){*argumentCount=3;auto desc=static_cast<D3DSURFACE_DESC*>(pointer(args[2]));HRESULT status=texture->GetLevelDesc(args[1],desc);if(SUCCEEDED(status))describeOriginal(args[0],desc);return status;}
        if(method==18){*argumentCount=3;IDirect3DSurface9* surface=nullptr;HRESULT status=texture->GetSurfaceLevel(args[1],&surface);uint32_t address=SUCCEEDED(status)?wrap(surface,Kind::Surface):0;if(address){attachTextureSurface(args[0],address);auto view=compressionView(args[0]);view.level=args[1];objects.at(address).compressed=view;}write(args[2],address);return status;}
        if(method==19){*argumentCount=5;return lockTexture(args[0],args[1],args[2],static_cast<const RECT*>(pointer(args[3])),args[4]);}
        if(method==20){*argumentCount=2;copyLockBack(args[0],args[1]);HRESULT status=texture->UnlockRect(args[1]);releaseLock(args[0],args[1]);return status;}}
    if(kind==4){auto texture=static_cast<IDirect3DCubeTexture9*>(object);
        if(method==17){*argumentCount=3;auto desc=static_cast<D3DSURFACE_DESC*>(pointer(args[2]));HRESULT status=texture->GetLevelDesc(args[1],desc);if(SUCCEEDED(status))describeOriginal(args[0],desc);return status;}
        if(method==18){*argumentCount=4;IDirect3DSurface9* surface=nullptr;HRESULT status=texture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(args[1]),args[2],&surface);uint32_t address=SUCCEEDED(status)?wrap(surface,Kind::Surface):0;if(address){attachTextureSurface(args[0],address);auto view=compressionView(args[0]);view.face=args[1];view.level=args[2];objects.at(address).compressed=view;}write(args[3],address);return status;}
        if(method==19){*argumentCount=6;return lockCube(args[0],args[1],args[2],args[3],static_cast<const RECT*>(pointer(args[4])),args[5]);}
        if(method==20){*argumentCount=3;uint32_t subresource=args[1]*32+args[2];copyLockBack(args[0],subresource);HRESULT status=texture->UnlockRect(static_cast<D3DCUBEMAP_FACES>(args[1]),args[2]);releaseLock(args[0],subresource);return status;}}
    if(kind==5){auto surface=static_cast<IDirect3DSurface9*>(object);
        if(method==12){*argumentCount=2;auto desc=static_cast<D3DSURFACE_DESC*>(pointer(args[1]));HRESULT status=surface->GetDesc(desc);if(SUCCEEDED(status))describeOriginal(args[0],desc);return status;}
        if(method==13){*argumentCount=4;return lockSurface(args[0],args[1],static_cast<const RECT*>(pointer(args[2])),args[3]);}
        if(method==14){*argumentCount=1;copyLockBack(args[0],0);HRESULT status=surface->UnlockRect();releaseLock(args[0],0);return status;}}
    if(kind==6||kind==7){
        if(method==11){*argumentCount=5;return kind==6?lockBuffer<IDirect3DVertexBuffer9,D3DVERTEXBUFFER_DESC>(args[0],args[1],args[2],args[3],args[4]):lockBuffer<IDirect3DIndexBuffer9,D3DINDEXBUFFER_DESC>(args[0],args[1],args[2],args[3],args[4]);}
        if(method==12){*argumentCount=1;copyLockBack(args[0],0);HRESULT status=kind==6?static_cast<IDirect3DVertexBuffer9*>(object)->Unlock():static_cast<IDirect3DIndexBuffer9*>(object)->Unlock();releaseLock(args[0],0);return status;}}
#include "d3d9_resource_methods.inc"
    throw std::runtime_error("D3D9 guest method pending: kind="+std::to_string(kind)+" slot="+std::to_string(method));
}
// Pilot entry (mgr_android_main.cpp, step "probe N").
void mgrPosProbe(int stage){posProbeStage(stage);}
