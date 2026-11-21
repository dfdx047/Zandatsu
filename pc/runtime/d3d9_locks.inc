struct ResourceLock {void* native;uint32_t guest,size,pitch,rowBytes,rows,flags;CompressedView compressed{};RECT rectangle{};uint32_t nativePitch{};bool shadowed{};};
std::map<uint64_t,ResourceLock> resourceLocks;
void dropBufferShadow(uint32_t address);   // below, with lockBuffer
uint64_t lockKey(uint32_t object,uint32_t subresource){return (uint64_t(object)<<32)|subresource;}
uint32_t rowBytes(D3DFORMAT format,uint32_t width){
    switch(format){
    case D3DFMT_DXT1:return ((width+3)/4)*8;
    case D3DFMT_DXT2:case D3DFMT_DXT3:case D3DFMT_DXT4:case D3DFMT_DXT5:return ((width+3)/4)*16;
    case D3DFMT_R8G8B8:return width*3;
    case D3DFMT_A8:case D3DFMT_L8:case D3DFMT_P8:return width;
    case D3DFMT_R5G6B5:case D3DFMT_X1R5G5B5:case D3DFMT_A1R5G5B5:case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4:case D3DFMT_A8L8:case D3DFMT_L16:case D3DFMT_R16F:case D3DFMT_V8U8:return width*2;
    case D3DFMT_A8R8G8B8:case D3DFMT_X8R8G8B8:case D3DFMT_A8B8G8R8:case D3DFMT_X8B8G8R8:
    case D3DFMT_A2R10G10B10:case D3DFMT_A2B10G10R10:case D3DFMT_G16R16:case D3DFMT_R32F:
    case D3DFMT_G16R16F:case D3DFMT_Q8W8V8U8:case D3DFMT_V16U16:return width*4;
    case D3DFMT_A16B16G16R16:case D3DFMT_A16B16G16R16F:case D3DFMT_G32R32F:return width*8;
    case D3DFMT_A32B32G32R32F:return width*16;
    default:throw std::runtime_error("D3D9 lock pixel format pending: "+std::to_string(format));
    }
}
bool blockCompressed(D3DFORMAT format){return format>=D3DFMT_DXT1&&format<=D3DFMT_DXT5;}
HRESULT lockPixels(uint32_t address,uint32_t subresource,const D3DSURFACE_DESC& desc,
                   const D3DLOCKED_RECT& native,const RECT* rect,uint32_t flags,uint32_t output){
    uint32_t width=rect?rect->right-rect->left:desc.Width,height=rect?rect->bottom-rect->top:desc.Height;
    auto view=compressionView(address);
    if(view.texture){
        if(subresource){view.level=subresource%32;view.face=subresource/32;}
        auto& texture=*view.texture;uint32_t fullWidth=texture.mipWidth(view.level),fullHeight=texture.mipHeight(view.level);
        RECT region=rect?*rect:RECT{0,0,static_cast<LONG>(fullWidth),static_cast<LONG>(fullHeight)};
        if(region.left<0||region.top<0||region.right>static_cast<LONG>(fullWidth)||region.bottom>static_cast<LONG>(fullHeight)||region.right<=region.left||region.bottom<=region.top)
            throw std::runtime_error("Invalid compressed lock rectangle");
        if((region.left%4)||(region.top%4)||(region.right%4&&region.right!=static_cast<LONG>(fullWidth))||(region.bottom%4&&region.bottom!=static_cast<LONG>(fullHeight)))
            throw std::runtime_error("Compressed lock rectangle must follow block boundaries");
        if(native.Pitch<=0||static_cast<uint32_t>(native.Pitch)<static_cast<uint32_t>(region.right-region.left)*4)throw std::runtime_error("Invalid decoded texture pitch");
        uint32_t pitch=texture.pitch(view.level),rows=(region.bottom-region.top+3)/4,bytes=((region.right-region.left+3)/4)*texture.blockBytes();
        uint32_t temporary=allocateGuest(pitch*rows),offset=(region.top/4)*pitch+(region.left/4)*texture.blockBytes();auto& raw=texture.bytes(view.face,view.level);
        if(!(flags&D3DLOCK_DISCARD))for(uint32_t row=0;row<rows;++row)std::memcpy(static_cast<uint8_t*>(pointer(temporary))+row*pitch,raw.data()+offset+row*pitch,bytes);
        resourceLocks.emplace(lockKey(address,subresource),ResourceLock{native.pBits,temporary,pitch*rows,pitch,bytes,rows,flags,view,region,static_cast<uint32_t>(native.Pitch)});
        write(output,pitch);write(output+4,temporary);return D3D_OK;
    }
    uint32_t rows=blockCompressed(desc.Format)?(height+3)/4:height,bytes=rowBytes(desc.Format,width);
    if(native.Pitch<=0||static_cast<uint32_t>(native.Pitch)<bytes||!rows)throw std::runtime_error("Invalid D3D9 lock pitch");
    uint64_t size=uint64_t(native.Pitch)*rows;if(size>0x7fffffff)throw std::runtime_error("D3D9 lock allocation too large");
    uint32_t temporary=allocateGuest(size);
    if(!(flags&D3DLOCK_DISCARD))for(uint32_t row=0;row<rows;++row)std::memcpy(static_cast<uint8_t*>(pointer(temporary))+row*native.Pitch,static_cast<const uint8_t*>(native.pBits)+row*native.Pitch,bytes);
    resourceLocks.emplace(lockKey(address,subresource),ResourceLock{native.pBits,temporary,static_cast<uint32_t>(size),static_cast<uint32_t>(native.Pitch),bytes,rows,flags});
    write(output,native.Pitch);write(output+4,temporary);return D3D_OK;
}
void copyLockBack(uint32_t address,uint32_t subresource){
    auto lock=resourceLocks.find(lockKey(address,subresource));if(lock==resourceLocks.end())throw std::runtime_error("D3D9 unlock without guest lock");
    auto& data=lock->second;if(!(data.flags&D3DLOCK_READONLY))for(uint32_t row=0;row<data.rows;++row)
        if(!data.compressed.texture)std::memcpy(static_cast<uint8_t*>(data.native)+row*data.pitch,static_cast<const uint8_t*>(pointer(data.guest))+row*data.pitch,data.rowBytes);
    if(data.compressed.texture&&!(data.flags&D3DLOCK_READONLY)){
        auto& texture=*data.compressed.texture;auto& raw=texture.bytes(data.compressed.face,data.compressed.level);
        uint32_t offset=(data.rectangle.top/4)*data.pitch+(data.rectangle.left/4)*texture.blockBytes();
        for(uint32_t row=0;row<data.rows;++row)std::memcpy(raw.data()+offset+row*data.pitch,static_cast<const uint8_t*>(pointer(data.guest))+row*data.pitch,data.rowBytes);
        uint32_t width=texture.mipWidth(data.compressed.level),height=texture.mipHeight(data.compressed.level);
        auto pixels=decodeDXT(texture.mode(),width,height,raw.data(),raw.size());
        for(int32_t row=0;row<data.rectangle.bottom-data.rectangle.top;++row)
            std::memcpy(static_cast<uint8_t*>(data.native)+row*data.nativePitch,pixels.data()+((size_t(data.rectangle.top)+row)*width+data.rectangle.left)*4,(data.rectangle.right-data.rectangle.left)*4);
    }
}
// A resource released while still locked (MGR keeps VB/IB locked for good: sub_00F99AC0 Lock, sub_00F99B10 Unlock)
// loses its lock with it, as in real D3D9. Left in the table, the next object the heap puts at the same guest address
// failed every Lock with INVALIDCALL: on a CONTINUE reload EffectPrimitive::Startup failed, StartupScene (0x009EE540)
// returned without creating [0x01B788C0+0x1E70] and the scene crashed through NULL (0x00F40F20 and the 1E70 guards).
void dropStaleLocks(uint32_t address){
    static unsigned reports=0;
    for(auto it=resourceLocks.lower_bound(uint64_t(address)<<32);it!=resourceLocks.end()&&uint32_t(it->first>>32)==address;){
        if(reports++<8)NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"MGR","D3D9 stale lock dropped: object %08x subresource %u",address,uint32_t(it->first));
        if(!it->second.shadowed)freeGuest(it->second.guest);it=resourceLocks.erase(it);
    }
    dropBufferShadow(address);
}
void releaseLock(uint32_t address,uint32_t subresource){auto lock=resourceLocks.find(lockKey(address,subresource));if(lock!=resourceLocks.end()){if(!lock->second.shadowed)freeGuest(lock->second.guest);resourceLocks.erase(lock);}}
HRESULT lockTexture(uint32_t address,uint32_t level,uint32_t output,const RECT* rect,uint32_t flags){
    if(!output||resourceLocks.count(lockKey(address,level)))return D3DERR_INVALIDCALL;
    auto texture=nativeObject<IDirect3DTexture9>(address);D3DSURFACE_DESC desc{};HRESULT status=texture->GetLevelDesc(level,&desc);if(FAILED(status))return status;
    D3DLOCKED_RECT lock{};status=texture->LockRect(level,&lock,rect,flags);if(FAILED(status))return status;
    return lockPixels(address,level,desc,lock,rect,flags,output);
}
HRESULT lockCube(uint32_t address,uint32_t face,uint32_t level,uint32_t output,const RECT* rect,uint32_t flags){
    uint32_t subresource=face*32+level;if(!output||resourceLocks.count(lockKey(address,subresource)))return D3DERR_INVALIDCALL;
    auto texture=nativeObject<IDirect3DCubeTexture9>(address);D3DSURFACE_DESC desc{};HRESULT status=texture->GetLevelDesc(level,&desc);if(FAILED(status))return status;
    D3DLOCKED_RECT lock{};status=texture->LockRect(static_cast<D3DCUBEMAP_FACES>(face),level,&lock,rect,flags);if(FAILED(status))return status;
    return lockPixels(address,subresource,desc,lock,rect,flags,output);
}
HRESULT lockSurface(uint32_t address,uint32_t output,const RECT* rect,uint32_t flags){
    if(!output||resourceLocks.count(lockKey(address,0)))return D3DERR_INVALIDCALL;
    auto surface=nativeObject<IDirect3DSurface9>(address);D3DSURFACE_DESC desc{};HRESULT status=surface->GetDesc(&desc);if(FAILED(status))return status;
    D3DLOCKED_RECT lock{};status=surface->LockRect(&lock,rect,flags);if(FAILED(status))return status;return lockPixels(address,0,desc,lock,rect,flags,output);
}
// Every VB/IB keeps one guest shadow of the whole buffer for its lifetime (21/11, Mi A3 / SD665): each Lock used to
// allocate a guest copy and read the locked range back from DXVK's mapping, which is uncached GPU memory. MGR locks its
// four big static buffers (11 MB) every frame: the read-back cost 33.6 ms/frame on a Cortex-A73 (now 0.02 ms). All the game's writes go through the shadow, so it
// already holds what the buffer holds: Lock returns shadow+offset with no read-back, Unlock still copies the range out.
// MGR_BUFFER_SHADOW=0 turns it off (A/B).
// The native buffer is no longer locked at Lock time: the game writes the shadow, and Unlock sends DXVK only the bytes
// that differ from what it already has (uploaded). MGR locks its 11 MB of static buffers whole every frame; DXVK 1.9
// then memcpy'd the whole dirty range out of uncached mapped memory at the next draw (FlushBuffer): DrawPrimitive and
// DrawIndexedPrimitive cost 30 + 11 ms/frame on the Mi A3.
struct BufferShadow{uint32_t guest=0,size=0;std::vector<uint8_t> uploaded;};
uint64_t shadowCompareNs=0,shadowSentBytes=0,shadowUnchanged=0;
std::map<uint32_t,std::pair<uint32_t,uint32_t>> shadowChangedSpan;   // buffer -> union of changed ranges (xray)
std::unordered_map<uint32_t,BufferShadow> bufferShadows;
bool bufferShadowOn(){static const bool on=[]{const char* v=std::getenv("MGR_BUFFER_SHADOW");return !v||std::atoi(v)!=0;}();return on;}
void dropBufferShadow(uint32_t address){auto it=bufferShadows.find(address);if(it!=bufferShadows.end()){freeGuest(it->second.guest);bufferShadows.erase(it);}}
// Lock cost split (21/11, Mi A3): native DXVK Lock (may wait for the GPU) vs our copies, by dynamic/static and flags.
struct LockCost{uint64_t n=0,nativeNs=0,copyNs=0,bytes=0;};
uint64_t unlockCopyNs=0,unlockBytes=0;
LockCost lockCosts[2][4];   // [dynamic][0 none,1 DISCARD,2 NOOVERWRITE,3 other]
void reportLockCosts(){
    static const bool on=std::getenv("MGR_XRAY")&&std::atoi(std::getenv("MGR_XRAY"));if(!on)return;
    static unsigned frames=0;if(++frames%120)return;
    static const char* kind[4]={"none","discard","nooverwrite","other"};
    std::string line;char one[160];
    for(int d=0;d<2;++d)for(int k=0;k<4;++k){auto& c=lockCosts[d][k];if(!c.n)continue;
        std::snprintf(one,sizeof one," [%s %s: %.0f/frame lock %.2f ms copy %.2f ms %.2f MB]",d?"dyn":"static",kind[k],c.n/120.0,c.nativeNs/1e6/120,c.copyNs/1e6/120,c.bytes/1048576.0/120);line+=one;c=LockCost{};}
    std::snprintf(one,sizeof one," [unlock %.2f ms %.2f MB, compare %.2f ms, sent %.3f MB, unchanged %.1f]",unlockCopyNs/1e6/120,unlockBytes/1048576.0/120,
        shadowCompareNs/1e6/120,shadowSentBytes/1048576.0/120,shadowUnchanged/120.0);line+=one;unlockCopyNs=unlockBytes=shadowCompareNs=shadowSentBytes=shadowUnchanged=0;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","D3D9 buffer locks per frame:%s",line.c_str());
    std::string spans;for(auto& [buffer,span]:shadowChangedSpan){auto it=bufferShadows.find(buffer);
        std::snprintf(one,sizeof one," %08x(size %u): %u-%u",buffer,it!=bufferShadows.end()?it->second.size:0,span.first,span.second);spans+=one;}
    shadowChangedSpan.clear();NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"MGR","D3D9 shadow changed spans (120 frames):%s",spans.c_str());
}
template<class Buffer,class Desc>HRESULT lockBuffer(uint32_t address,uint32_t offset,uint32_t size,uint32_t output,uint32_t flags){
    if(!output||resourceLocks.count(lockKey(address,0)))return D3DERR_INVALIDCALL;
    auto buffer=nativeObject<Buffer>(address);Desc desc{};HRESULT status=buffer->GetDesc(&desc);if(FAILED(status))return status;
    if(offset>desc.Size)return D3DERR_INVALIDCALL;
    auto t0=std::chrono::steady_clock::now();
    void* native=nullptr;
    if(!bufferShadowOn()){status=buffer->Lock(offset,size,&native,flags);if(FAILED(status))return status;}
    auto t1=std::chrono::steady_clock::now();
    if(!size||offset+size>desc.Size)size=desc.Size-offset;
    auto& cost=lockCosts[(desc.Usage&D3DUSAGE_DYNAMIC)?1:0][(flags&D3DLOCK_DISCARD)?1:(flags&D3DLOCK_NOOVERWRITE)?2:flags?3:0];
    ++cost.n;cost.nativeNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();cost.bytes+=size;
    struct CopyTimer{LockCost& c;std::chrono::steady_clock::time_point t;~CopyTimer(){c.copyNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();}} copyTimer{cost,t1};
    if(bufferShadowOn()){   // all buffers (static ones are the 11 MB/frame case)
        auto& shadow=bufferShadows[address];
        if(!shadow.guest){   // first lock: one read of what the buffer holds
            shadow.guest=allocateGuest(desc.Size);shadow.size=desc.Size;shadow.uploaded.assign(desc.Size,0);
            void* all=nullptr;
            if(!(flags&D3DLOCK_DISCARD)&&SUCCEEDED(buffer->Lock(0,0,&all,D3DLOCK_READONLY))){std::memcpy(shadow.uploaded.data(),all,desc.Size);buffer->Unlock();}
            std::memcpy(pointer(shadow.guest),shadow.uploaded.data(),desc.Size);
        }
        ResourceLock entry{nullptr,shadow.guest+offset,size,size,size,1,flags};entry.shadowed=true;
        resourceLocks.emplace(lockKey(address,0),entry);write(output,shadow.guest+offset);return D3D_OK;
    }
    uint32_t temporary=allocateGuest(size);
    if(!(flags&D3DLOCK_DISCARD))std::memcpy(pointer(temporary),native,size);
    resourceLocks.emplace(lockKey(address,0),ResourceLock{native,temporary,size,size,size,1,flags});write(output,temporary);return D3D_OK;
}
// Unlock of a shadowed buffer: first and last differing 256-byte block against what DXVK holds; only that range is
// locked and copied (nothing at all when the game changed nothing).
template<class Buffer>HRESULT unlockShadowedBuffer(uint32_t address,const ResourceLock& data){
    if(data.flags&D3DLOCK_READONLY)return D3D_OK;
    auto it=bufferShadows.find(address);if(it==bufferShadows.end())return D3DERR_INVALIDCALL;
    auto& shadow=it->second;auto t0=std::chrono::steady_clock::now();
    uint32_t base=data.guest-shadow.guest,n=data.size;
    const uint8_t* now=static_cast<const uint8_t*>(pointer(data.guest));uint8_t* old=shadow.uploaded.data()+base;
    constexpr uint32_t block=256;uint32_t lo=0,hi=n;
    if(data.flags&D3DLOCK_DISCARD){lo=0;hi=n;}
    else{
        while(lo<n){uint32_t c=std::min(block,n-lo);if(std::memcmp(now+lo,old+lo,c))break;lo+=c;}
        if(lo>=n){++shadowUnchanged;shadowCompareNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t0).count();return D3D_OK;}
        while(hi>lo){uint32_t c=(hi-lo)%block?(hi-lo)%block:block;if(std::memcmp(now+hi-c,old+hi-c,c))break;hi-=c;}
    }
    shadowCompareNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t0).count();
    auto buffer=nativeObject<Buffer>(address);void* native=nullptr;
    HRESULT status=buffer->Lock(base+lo,hi-lo,&native,data.flags&~D3DLOCK_READONLY);if(FAILED(status))return status;
    std::memcpy(native,now+lo,hi-lo);std::memcpy(old+lo,now+lo,hi-lo);shadowSentBytes+=hi-lo;
    {auto& span=shadowChangedSpan[address];uint32_t a=base+lo,b=base+hi;
        if(span.first==span.second){span={a,b};}else{span.first=std::min(span.first,a);span.second=std::max(span.second,b);}}
    return buffer->Unlock();
}
