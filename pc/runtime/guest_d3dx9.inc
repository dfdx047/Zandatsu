// The 23 d3dx9_43.dll functions the MGR:R PC exe imports, written natively (no Microsoft DLL needed):
// 17 of vector/matrix/quaternion math, DDS textures from memory through the guest's own IDirect3DDevice9
// (the D3D9 bridge), and ID3DXConstantTable parsed from the shader's CTAB comment block.
// Matrix layout: row-major float[16] as D3DXMATRIX, row vectors (v * M).
struct M4 {float m[16];};
M4 readM(uint32_t a){M4 r;std::memcpy(r.m,ptr(a,64),64);return r;}
void writeM(uint32_t a,const M4& r){std::memcpy(ptr(a,64),r.m,64);}
M4 mul(const M4& a,const M4& b){M4 r{};for(int i=0;i<4;++i)for(int j=0;j<4;++j){float s=0;for(int k=0;k<4;++k)s+=a.m[i*4+k]*b.m[k*4+j];r.m[i*4+j]=s;}return r;}
M4 identity(){M4 r{};r.m[0]=r.m[5]=r.m[10]=r.m[15]=1;return r;}
void dxMatrixMultiply(){writeM(arg(0),mul(readM(arg(1)),readM(arg(2))));ret(arg(0),3);}
void dxMatrixTranspose(){M4 a=readM(arg(1)),r;for(int i=0;i<4;++i)for(int j=0;j<4;++j)r.m[i*4+j]=a.m[j*4+i];writeM(arg(0),r);ret(arg(0),2);}
void dxMatrixTranslation(){M4 r=identity();r.m[12]=argf(1);r.m[13]=argf(2);r.m[14]=argf(3);writeM(arg(0),r);ret(arg(0),4);}
void dxMatrixScaling(){M4 r=identity();r.m[0]=argf(1);r.m[5]=argf(2);r.m[10]=argf(3);writeM(arg(0),r);ret(arg(0),4);}
void dxMatrixRotationX(){float c=std::cos(argf(1)),s=std::sin(argf(1));M4 r=identity();r.m[5]=c;r.m[6]=s;r.m[9]=-s;r.m[10]=c;writeM(arg(0),r);ret(arg(0),2);}
void dxMatrixRotationY(){float c=std::cos(argf(1)),s=std::sin(argf(1));M4 r=identity();r.m[0]=c;r.m[2]=-s;r.m[8]=s;r.m[10]=c;writeM(arg(0),r);ret(arg(0),2);}
void dxMatrixRotationZ(){float c=std::cos(argf(1)),s=std::sin(argf(1));M4 r=identity();r.m[0]=c;r.m[1]=s;r.m[4]=-s;r.m[5]=c;writeM(arg(0),r);ret(arg(0),2);}
M4 fromQuat(float x,float y,float z,float w){
    M4 r=identity();
    r.m[0]=1-2*(y*y+z*z);r.m[1]=2*(x*y+z*w);r.m[2]=2*(x*z-y*w);
    r.m[4]=2*(x*y-z*w);r.m[5]=1-2*(x*x+z*z);r.m[6]=2*(y*z+x*w);
    r.m[8]=2*(x*z+y*w);r.m[9]=2*(y*z-x*w);r.m[10]=1-2*(x*x+y*y);return r;
}
void axisAngleQuat(uint32_t v,float angle,float q[4]){
    float x=readf(v),y=readf(v+4),z=readf(v+8),n=std::sqrt(x*x+y*y+z*z);if(n>0){x/=n;y/=n;z/=n;}
    float s=std::sin(angle/2);q[0]=x*s;q[1]=y*s;q[2]=z*s;q[3]=std::cos(angle/2);
}
void dxMatrixRotationAxis(){float q[4];axisAngleQuat(arg(1),argf(2),q);writeM(arg(0),fromQuat(q[0],q[1],q[2],q[3]));ret(arg(0),3);}
void dxMatrixRotationQuaternion(){uint32_t q=arg(1);writeM(arg(0),fromQuat(readf(q),readf(q+4),readf(q+8),readf(q+12)));ret(arg(0),2);}
void dxMatrixInverse(){
    M4 a=readM(arg(2));float inv[16],*m=a.m;
    inv[0]=m[5]*m[10]*m[15]-m[5]*m[11]*m[14]-m[9]*m[6]*m[15]+m[9]*m[7]*m[14]+m[13]*m[6]*m[11]-m[13]*m[7]*m[10];
    inv[4]=-m[4]*m[10]*m[15]+m[4]*m[11]*m[14]+m[8]*m[6]*m[15]-m[8]*m[7]*m[14]-m[12]*m[6]*m[11]+m[12]*m[7]*m[10];
    inv[8]=m[4]*m[9]*m[15]-m[4]*m[11]*m[13]-m[8]*m[5]*m[15]+m[8]*m[7]*m[13]+m[12]*m[5]*m[11]-m[12]*m[7]*m[9];
    inv[12]=-m[4]*m[9]*m[14]+m[4]*m[10]*m[13]+m[8]*m[5]*m[14]-m[8]*m[6]*m[13]-m[12]*m[5]*m[10]+m[12]*m[6]*m[9];
    inv[1]=-m[1]*m[10]*m[15]+m[1]*m[11]*m[14]+m[9]*m[2]*m[15]-m[9]*m[3]*m[14]-m[13]*m[2]*m[11]+m[13]*m[3]*m[10];
    inv[5]=m[0]*m[10]*m[15]-m[0]*m[11]*m[14]-m[8]*m[2]*m[15]+m[8]*m[3]*m[14]+m[12]*m[2]*m[11]-m[12]*m[3]*m[10];
    inv[9]=-m[0]*m[9]*m[15]+m[0]*m[11]*m[13]+m[8]*m[1]*m[15]-m[8]*m[3]*m[13]-m[12]*m[1]*m[11]+m[12]*m[3]*m[9];
    inv[13]=m[0]*m[9]*m[14]-m[0]*m[10]*m[13]-m[8]*m[1]*m[14]+m[8]*m[2]*m[13]+m[12]*m[1]*m[10]-m[12]*m[2]*m[9];
    inv[2]=m[1]*m[6]*m[15]-m[1]*m[7]*m[14]-m[5]*m[2]*m[15]+m[5]*m[3]*m[14]+m[13]*m[2]*m[7]-m[13]*m[3]*m[6];
    inv[6]=-m[0]*m[6]*m[15]+m[0]*m[7]*m[14]+m[4]*m[2]*m[15]-m[4]*m[3]*m[14]-m[12]*m[2]*m[7]+m[12]*m[3]*m[6];
    inv[10]=m[0]*m[5]*m[15]-m[0]*m[7]*m[13]-m[4]*m[1]*m[15]+m[4]*m[3]*m[13]+m[12]*m[1]*m[7]-m[12]*m[3]*m[5];
    inv[14]=-m[0]*m[5]*m[14]+m[0]*m[6]*m[13]+m[4]*m[1]*m[14]-m[4]*m[2]*m[13]-m[12]*m[1]*m[6]+m[12]*m[2]*m[5];
    inv[3]=-m[1]*m[6]*m[11]+m[1]*m[7]*m[10]+m[5]*m[2]*m[11]-m[5]*m[3]*m[10]-m[9]*m[2]*m[7]+m[9]*m[3]*m[6];
    inv[7]=m[0]*m[6]*m[11]-m[0]*m[7]*m[10]-m[4]*m[2]*m[11]+m[4]*m[3]*m[10]+m[8]*m[2]*m[7]-m[8]*m[3]*m[6];
    inv[11]=-m[0]*m[5]*m[11]+m[0]*m[7]*m[9]+m[4]*m[1]*m[11]-m[4]*m[3]*m[9]-m[8]*m[1]*m[7]+m[8]*m[3]*m[5];
    inv[15]=m[0]*m[5]*m[10]-m[0]*m[6]*m[9]-m[4]*m[1]*m[10]+m[4]*m[2]*m[9]+m[8]*m[1]*m[6]-m[8]*m[2]*m[5];
    float det=m[0]*inv[0]+m[1]*inv[4]+m[2]*inv[8]+m[3]*inv[12];
    if(arg(1))writef(arg(1),det);
    if(det==0){ret(0,3);return;}
    M4 r;for(int i=0;i<16;++i)r.m[i]=inv[i]/det;writeM(arg(0),r);ret(arg(0),3);
}
void dxVec3TransformCoord(){
    uint32_t v=arg(1);M4 m=readM(arg(2));float x=readf(v),y=readf(v+4),z=readf(v+8),o[4];
    for(int j=0;j<4;++j)o[j]=x*m.m[j]+y*m.m[4+j]+z*m.m[8+j]+m.m[12+j];
    float w=o[3]?o[3]:1;writef(arg(0),o[0]/w);writef(arg(0)+4,o[1]/w);writef(arg(0)+8,o[2]/w);ret(arg(0),3);
}
void dxVec3TransformNormal(){
    uint32_t v=arg(1);M4 m=readM(arg(2));float x=readf(v),y=readf(v+4),z=readf(v+8),o[3];
    for(int j=0;j<3;++j)o[j]=x*m.m[j]+y*m.m[4+j]+z*m.m[8+j];
    writef(arg(0),o[0]);writef(arg(0)+4,o[1]);writef(arg(0)+8,o[2]);ret(arg(0),3);
}
void dxVec4Transform(){
    uint32_t v=arg(1);M4 m=readM(arg(2));float x=readf(v),y=readf(v+4),z=readf(v+8),w=readf(v+12),o[4];
    for(int j=0;j<4;++j)o[j]=x*m.m[j]+y*m.m[4+j]+z*m.m[8+j]+w*m.m[12+j];
    for(int j=0;j<4;++j)writef(arg(0)+j*4,o[j]);ret(arg(0),3);
}
void dxVec3Normalize(){
    uint32_t v=arg(1);float x=readf(v),y=readf(v+4),z=readf(v+8),n=std::sqrt(x*x+y*y+z*z);
    if(n>0){x/=n;y/=n;z/=n;}else x=y=z=0;writef(arg(0),x);writef(arg(0)+4,y);writef(arg(0)+8,z);ret(arg(0),2);
}
void readQ(uint32_t a,float q[4]){for(int i=0;i<4;++i)q[i]=readf(a+i*4);}
void writeQ(uint32_t a,const float q[4]){for(int i=0;i<4;++i)writef(a+i*4,q[i]);}
void dxQuaternionRotationAxis(){float q[4];axisAngleQuat(arg(1),argf(2),q);writeQ(arg(0),q);ret(arg(0),3);}
void dxQuaternionMultiply(){
    // D3DXQuaternionMultiply(out, q1, q2) = q2 * q1 in Hamilton order (D3DX applies q1 then q2)
    float a[4],b[4],r[4];readQ(arg(1),a);readQ(arg(2),b);
    r[3]=b[3]*a[3]-b[0]*a[0]-b[1]*a[1]-b[2]*a[2];
    r[0]=b[3]*a[0]+b[0]*a[3]+b[1]*a[2]-b[2]*a[1];
    r[1]=b[3]*a[1]-b[0]*a[2]+b[1]*a[3]+b[2]*a[0];
    r[2]=b[3]*a[2]+b[0]*a[1]-b[1]*a[0]+b[2]*a[3];
    writeQ(arg(0),r);ret(arg(0),3);
}
void dxQuaternionInverse(){
    float q[4];readQ(arg(1),q);float n=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
    float r[4]={-q[0]/n,-q[1]/n,-q[2]/n,q[3]/n};if(n==0)r[0]=r[1]=r[2]=r[3]=0;writeQ(arg(0),r);ret(arg(0),2);
}
void dxQuaternionSlerp(){
    float a[4],b[4],r[4],t=argf(3);readQ(arg(1),a);readQ(arg(2),b);
    float d=a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3],sign=1;if(d<0){d=-d;sign=-1;}
    float s0=1-t,s1=t;
    if(1-d>1e-6f){float th=std::acos(d),st=std::sin(th);s0=std::sin((1-t)*th)/st;s1=std::sin(t*th)/st;}
    for(int i=0;i<4;++i)r[i]=s0*a[i]+sign*s1*b[i];writeQ(arg(0),r);ret(arg(0),4);
}
void dxQuaternionRotationMatrix(){
    M4 m=readM(arg(1));float q[4],tr=m.m[0]+m.m[5]+m.m[10];
    if(tr>0){float s=std::sqrt(tr+1)*2;q[3]=s/4;q[0]=(m.m[6]-m.m[9])/s;q[1]=(m.m[8]-m.m[2])/s;q[2]=(m.m[1]-m.m[4])/s;}
    else if(m.m[0]>m.m[5]&&m.m[0]>m.m[10]){float s=std::sqrt(1+m.m[0]-m.m[5]-m.m[10])*2;q[0]=s/4;q[3]=(m.m[6]-m.m[9])/s;q[1]=(m.m[1]+m.m[4])/s;q[2]=(m.m[8]+m.m[2])/s;}
    else if(m.m[5]>m.m[10]){float s=std::sqrt(1+m.m[5]-m.m[0]-m.m[10])*2;q[1]=s/4;q[3]=(m.m[8]-m.m[2])/s;q[0]=(m.m[1]+m.m[4])/s;q[2]=(m.m[6]+m.m[9])/s;}
    else{float s=std::sqrt(1+m.m[10]-m.m[0]-m.m[5])*2;q[2]=s/4;q[3]=(m.m[1]-m.m[4])/s;q[0]=(m.m[8]+m.m[2])/s;q[1]=(m.m[6]+m.m[9])/s;}
    writeQ(arg(0),q);ret(arg(0),2);
}

// ---- textures. IDirect3DDevice9 slots: 23 CreateTexture, 25 CreateCubeTexture. Texture: 19 LockRect, 20 UnlockRect.
// Cube texture: 19 LockRect(face, level, locked, rect, flags), 20 UnlockRect(face, level).
struct DdsInfo {uint32_t width,height,mips,format,dataOffset;bool cube,compressed;uint32_t blockBytes,bytesPerPixel;};
bool parseDds(const uint8_t* d,uint32_t size,DdsInfo& o){
    if(size<128||std::memcmp(d,"DDS ",4))return false;
    auto u=[&](uint32_t off){uint32_t v;std::memcpy(&v,d+off,4);return v;};
    o.height=u(12);o.width=u(16);o.mips=std::max(1u,u(28));o.cube=u(112)&0x200;o.dataOffset=128;
    uint32_t pf=u(80),fourcc=u(84),bits=u(88),r=u(92),a=u(104);o.compressed=false;o.blockBytes=0;o.bytesPerPixel=0;
    if(pf&4){
        if(fourcc==0x30315844)return false;   // "DX10" header: not produced by this game's tools
        o.format=fourcc;
        if(fourcc==0x31545844){o.compressed=true;o.blockBytes=8;}                                         // DXT1
        else if(fourcc==0x32545844||fourcc==0x33545844||fourcc==0x34545844||fourcc==0x35545844){o.compressed=true;o.blockBytes=16;}  // DXT2-5
        else if(fourcc==0x31495441){o.compressed=true;o.blockBytes=8;}   // ATI1
        else if(fourcc==0x32495441){o.compressed=true;o.blockBytes=16;}  // ATI2
        else switch(fourcc){case 111:o.bytesPerPixel=2;break;case 112:o.bytesPerPixel=4;break;case 113:o.bytesPerPixel=8;break;
            case 114:o.bytesPerPixel=4;break;case 115:o.bytesPerPixel=8;break;case 116:o.bytesPerPixel=16;break;default:return false;}
    }else if(bits==32){o.format=a?21u:22u;o.bytesPerPixel=4;}          // A8R8G8B8 / X8R8G8B8
    else if(bits==24){o.format=20;o.bytesPerPixel=3;}                    // R8G8B8
    else if(bits==16){o.format=r==0xf800?23u:a==0xf000?26u:a==0x8000?25u:24u;o.bytesPerPixel=2;}
    else if(bits==8){o.format=a&&!r?28u:50u;o.bytesPerPixel=1;}          // A8 / L8
    else return false;
    return true;
}
uint32_t levelRows(const DdsInfo& i,uint32_t h){return i.compressed?std::max(1u,(h+3)/4):h;}
uint32_t levelPitch(const DdsInfo& i,uint32_t w){return i.compressed?std::max(1u,(w+3)/4)*i.blockBytes:w*i.bytesPerPixel;}
// Copy every mip level (of one face) from the file into a locked texture. Returns the bytes consumed.
uint32_t uploadLevels(uint32_t texture,const DdsInfo& info,uint32_t levels,const uint8_t* src,uint32_t avail,bool cube,uint32_t face){
    uint32_t used=0,locked=allocate(8);
    for(uint32_t l=0;l<info.mips;++l){
        uint32_t w=std::max(1u,info.width>>l),h=std::max(1u,info.height>>l),pitch=levelPitch(info,w),rows=levelRows(info,h),bytes=pitch*rows;
        if(used+bytes>avail)break;
        if(l<levels){
            uint32_t hr=cube?callMethod(texture,19,{face,l,locked,0,0}):callMethod(texture,19,{l,locked,0,0});
            if(!hr){int32_t dstPitch=int32_t(read32(locked));uint32_t bits=read32(locked+4);
                for(uint32_t row=0;row<rows;++row)std::memcpy(ptr(bits+row*uint32_t(dstPitch),pitch),src+used+row*pitch,pitch);
                if(cube)callMethod(texture,20,{face,l});else callMethod(texture,20,{l});}
        }
        used+=bytes;
    }
    heaps->free(GuestHeaps::process,locked);return used;
}
void writeImageInfo(uint32_t out,const DdsInfo& i){if(!out)return;write32(out,i.width);write32(out+4,i.height);write32(out+8,1);write32(out+12,i.mips);write32(out+16,i.format);write32(out+20,i.cube?5:3);write32(out+24,4);}
void dxCreateTextureFromFileInMemoryEx(){
    // (device, data, size, width, height, mips, usage, format, pool, filter, mipFilter, colorKey, srcInfo, palette, out)
    uint32_t device=arg(0),data=arg(1),size=arg(2),mips=arg(5),usage=arg(6),format=arg(7),pool=arg(8),out=arg(14);DdsInfo info;
    auto* bytes=static_cast<const uint8_t*>(ptr(data,size));
    if(!parseDds(bytes,size,info)||info.cube){NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"MGR","D3DXCreateTextureFromFileInMemoryEx: not a 2D DDS (size %u, magic %08x)",size,read32(data));ret(0x88760B59u,15);return;}   // D3DXERR_INVALIDDATA
    uint32_t levels=(mips==0||mips==0xffffffffu||mips==0xfffffffdu)?info.mips:std::min(mips,info.mips);
    if(format&&format!=0xfffffffdu&&format!=info.format)NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"MGR","D3DX texture asked format %u, file has %u: using the file's",format,info.format);
    uint32_t hr=callMethod(device,23,{info.width,info.height,levels,usage,info.format,pool,out,0});
    if(hr){ret(hr,15);return;}
    uploadLevels(read32(out),info,levels,bytes+info.dataOffset,size-info.dataOffset,false,0);
    writeImageInfo(arg(12),info);ret(0,15);
}
void dxCreateCubeTextureFromFileInMemoryEx(){
    // (device, data, size, edge, mips, usage, format, pool, filter, mipFilter, colorKey, srcInfo, palette, out)
    uint32_t device=arg(0),data=arg(1),size=arg(2),mips=arg(4),usage=arg(5),pool=arg(7),out=arg(13);DdsInfo info;
    auto* bytes=static_cast<const uint8_t*>(ptr(data,size));
    if(!parseDds(bytes,size,info)||!info.cube){NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"MGR","D3DXCreateCubeTextureFromFileInMemoryEx: not a cube DDS");ret(0x88760B59u,14);return;}
    uint32_t levels=(mips==0||mips==0xffffffffu||mips==0xfffffffdu)?info.mips:std::min(mips,info.mips);
    uint32_t hr=callMethod(device,25,{info.width,levels,usage,info.format,pool,out,0});if(hr){ret(hr,14);return;}
    uint32_t off=info.dataOffset;for(uint32_t face=0;face<6&&off<size;++face)off+=uploadLevels(read32(out),info,levels,bytes+off,size-off,true,face);
    writeImageInfo(arg(11),info);ret(0,14);
}
void dxCreateTexture(){
    // (device, width, height, mips, usage, format, pool, out): straight to IDirect3DDevice9::CreateTexture
    uint32_t mips=arg(3);if(mips==0xffffffffu)mips=0;
    ret(callMethod(arg(0),23,{arg(1),arg(2),mips,arg(4),arg(5),arg(6),arg(7),0}),8);
}

// ---- ID3DXConstantTable from the CTAB comment of SM2/SM3 bytecode.
struct CtabConstant {std::string name;uint32_t regSet,regIndex,regCount,cls,type,rows,columns,elements,bytes,defaultValue;};
struct ConstantTable {uint32_t blob,blobSize,version,creator;std::vector<CtabConstant> constants;uint32_t descStrings=0;};
std::map<uint32_t,ConstantTable> constantTables;uint32_t nextConstantTableId=1;
std::vector<uint32_t> constantTableVtable;
ConstantTable* tableOf(uint32_t self){auto f=constantTables.find(objectId(self));return f==constantTables.end()?nullptr:&f->second;}
// A D3DXHANDLE for constant i is (table id << 16) | (i + 1); a name string handle is resolved by name.
const CtabConstant* constantOf(ConstantTable& t,uint32_t handle){
    if(!handle)return nullptr;
    if((handle&0xffff0000u)==0x7a000000u){uint32_t i=handle&0xffff;return i>=1&&i<=t.constants.size()?&t.constants[i-1]:nullptr;}
    // otherwise the handle is a guest string: the name itself
    if(!memory->readable(handle,1))return nullptr;std::string name=guestString(handle);
    for(auto& c:t.constants)if(c.name==name)return &c;return nullptr;
}
void ctQueryInterface(){if(arg(2))write32(arg(2),0);ret(0x80004002u,3);}
void ctAddRef(){ret(2,1);}
void ctRelease(){ret(1,1);}
void ctBufferPointer(){auto* t=tableOf(arg(0));ret(t?t->blob:0,1);}
void ctBufferSize(){auto* t=tableOf(arg(0));ret(t?t->blobSize:0,1);}
void ctGetDesc(){auto* t=tableOf(arg(0));uint32_t o=arg(1);if(!t||!o){ret(0x8876086Cu,2);return;}write32(o,t->creator);write32(o+4,t->version);write32(o+8,uint32_t(t->constants.size()));ret(0,2);}
void writeConstantDesc(uint32_t out,const CtabConstant& c){
    // D3DXCONSTANT_DESC: Name, RegisterSet, RegisterIndex, RegisterCount, Class, Type, Rows, Columns, Elements, StructMembers, Bytes, DefaultValue
    write32(out,ansiString(c.name));write32(out+4,c.regSet);write32(out+8,c.regIndex);write32(out+12,c.regCount);write32(out+16,c.cls);write32(out+20,c.type);
    write32(out+24,c.rows);write32(out+28,c.columns);write32(out+32,c.elements);write32(out+36,0);write32(out+40,c.bytes);write32(out+44,c.defaultValue);
}
void ctGetConstantDesc(){
    auto* t=tableOf(arg(0));if(!t){ret(0x8876086Cu,4);return;}auto* c=constantOf(*t,arg(1));uint32_t out=arg(2),count=arg(3);
    if(!c){ret(0x8876086Cu,4);return;}if(count){uint32_t n=read32(count);if(out&&n>=1)writeConstantDesc(out,*c);write32(count,1);}else if(out)writeConstantDesc(out,*c);ret(0,4);
}
void ctGetSamplerIndex(){auto* t=tableOf(arg(0));auto* c=t?constantOf(*t,arg(1)):nullptr;ret(c&&c->regSet==3?c->regIndex:0xffffffffu,2);}
void ctGetConstant(){auto* t=tableOf(arg(0));if(!t||arg(1)){ret(0,3);return;}uint32_t i=arg(2);ret(i<t->constants.size()?0x7a000000u|(i+1):0,3);}
void ctGetConstantByName(){
    auto* t=tableOf(arg(0));if(!t||arg(1)){ret(0,3);return;}std::string name=guestString(arg(2));
    for(uint32_t i=0;i<t->constants.size();++i)if(t->constants[i].name==name){ret(0x7a000000u|(i+1),3);return;}ret(0,3);
}
void ctGetConstantElement(){auto* t=tableOf(arg(0));auto* c=t?constantOf(*t,arg(1)):nullptr;ret(c&&arg(2)==0?arg(1):0,3);}
// Set* write the registers through the device: IDirect3DDevice9 slots 94 SetVertexShaderConstantF, 109 SetPixelShaderConstantF.
void setFloats(uint32_t self,uint32_t device,uint32_t handle,const std::vector<float>& values){
    auto* t=tableOf(self);auto* c=t?constantOf(*t,handle):nullptr;if(!c||c->regSet!=2)return;
    bool pixel=(t->version&0xffff0000u)==0xffff0000u;uint32_t vec4=std::min<uint32_t>(c->regCount,uint32_t((values.size()+3)/4));if(!vec4)return;
    uint32_t buf=allocate(vec4*16);std::memset(ptr(buf,vec4*16),0,vec4*16);std::memcpy(ptr(buf,vec4*16),values.data(),std::min<size_t>(values.size()*4,vec4*16));
    callMethod(device,pixel?109:94,{c->regIndex,buf,vec4});heaps->free(GuestHeaps::process,buf);
}
std::vector<float> guestFloats(uint32_t a,uint32_t n){std::vector<float> v(n);for(uint32_t i=0;i<n;++i)v[i]=readf(a+i*4);return v;}
void ctSetDefaults(){ret(0,2);}
void ctSetValue(){setFloats(arg(0),arg(1),arg(2),guestFloats(arg(3),arg(4)/4));ret(0,5);}
void ctSetFloat(){setFloats(arg(0),arg(1),arg(2),{argf(3)});ret(0,4);}
void ctSetFloatArray(){setFloats(arg(0),arg(1),arg(2),guestFloats(arg(3),arg(4)));ret(0,5);}
void ctSetVector(){setFloats(arg(0),arg(1),arg(2),guestFloats(arg(3),4));ret(0,4);}
void ctSetVectorArray(){setFloats(arg(0),arg(1),arg(2),guestFloats(arg(3),arg(4)*4));ret(0,5);}
void ctSetMatrix(){
    // D3DX writes matrices transposed into column-major registers for MATRIX_COLUMNS (class 3), as rows for MATRIX_ROWS (2)
    auto* t=tableOf(arg(0));auto* c=t?constantOf(*t,arg(2)):nullptr;auto m=guestFloats(arg(3),16);
    if(c&&c->cls==3){std::vector<float> tr(16);for(int i=0;i<4;++i)for(int j=0;j<4;++j)tr[i*4+j]=m[j*4+i];m=tr;}
    setFloats(arg(0),arg(1),arg(2),m);ret(0,4);
}
void ctSetMatrixTranspose(){
    auto* t=tableOf(arg(0));auto* c=t?constantOf(*t,arg(2)):nullptr;auto m=guestFloats(arg(3),16);
    if(!c||c->cls!=3){std::vector<float> tr(16);for(int i=0;i<4;++i)for(int j=0;j<4;++j)tr[i*4+j]=m[j*4+i];m=tr;}
    setFloats(arg(0),arg(1),arg(2),m);ret(0,4);
}
void ctUnimplemented(){throw GuestStop("ID3DXConstantTable method not implemented (from "+hex(g_cur_func)+")");}
void dxGetShaderConstantTable(){
    uint32_t function=arg(0),out=arg(1);if(!out){ret(0x8876086Cu,2);return;}
    if(constantTableVtable.empty()){
        // ID3DXBuffer (QI, AddRef, Release, GetBufferPointer, GetBufferSize) then ID3DXConstantTable
        constantTableVtable={hostMethod(ctQueryInterface),hostMethod(ctAddRef),hostMethod(ctRelease),hostMethod(ctBufferPointer),hostMethod(ctBufferSize),
            hostMethod(ctGetDesc),hostMethod(ctGetConstantDesc),hostMethod(ctGetSamplerIndex),hostMethod(ctGetConstant),hostMethod(ctGetConstantByName),
            hostMethod(ctGetConstantElement),hostMethod(ctSetDefaults),hostMethod(ctSetValue),hostMethod(ctUnimplemented),hostMethod(ctUnimplemented),
            hostMethod(ctUnimplemented),hostMethod(ctUnimplemented),hostMethod(ctSetFloat),hostMethod(ctSetFloatArray),hostMethod(ctSetVector),
            hostMethod(ctSetVectorArray),hostMethod(ctSetMatrix),hostMethod(ctUnimplemented),hostMethod(ctUnimplemented),hostMethod(ctSetMatrixTranspose),
            hostMethod(ctUnimplemented),hostMethod(ctUnimplemented)};
    }
    // Walk the token stream for a comment (0xFFFE) whose first dword is 'CTAB'.
    uint32_t version=read32(function),p=function+4;
    for(uint32_t guard=0;guard<65536;++guard){
        uint32_t token=read32(p);if(token==0x0000FFFF)break;
        if((token&0xffff)==0xFFFE){
            uint32_t dwords=token>>16;
            if(read32(p+4)==0x42415443){   // "CTAB"
                uint32_t base=p+8,size=(dwords-1)*4;ConstantTable t{};t.blob=base;t.blobSize=size;
                t.creator=read32(base+4)?base+read32(base+4):0;t.version=read32(base+8);uint32_t count=read32(base+12),info=base+read32(base+16);
                for(uint32_t i=0;i<count;++i){uint32_t e=info+i*20;CtabConstant c{};c.name=guestString(base+read32(e));
                    uint16_t rs,ri,rc;std::memcpy(&rs,ptr(e+4,2),2);std::memcpy(&ri,ptr(e+6,2),2);std::memcpy(&rc,ptr(e+8,2),2);
                    c.regSet=rs;c.regIndex=ri;c.regCount=rc;uint32_t ty=base+read32(e+12);uint16_t f[5];std::memcpy(f,ptr(ty,10),10);
                    c.cls=f[0];c.type=f[1];c.rows=f[2];c.columns=f[3];c.elements=f[4];c.bytes=c.rows*c.columns*std::max<uint32_t>(1,c.elements)*4;
                    c.defaultValue=read32(e+16)?base+read32(e+16):0;t.constants.push_back(c);}
                t.version=version;uint32_t id=nextConstantTableId++;constantTables.emplace(id,t);
                write32(out,guestObject(constantTableVtable,id));ret(0,2);return;
            }
            p+=4+dwords*4;continue;
        }
        p+=4+((token>>24)&0xf)*4;   // instruction length in SM2+ (bits 24-27)
    }
    write32(out,0);ret(0x8876086Cu,2);   // D3DERR_INVALIDCALL: no constant table
}

const std::map<std::string,recomp_func_t> d3dx9Functions={
    {"D3DXMatrixMultiply",dxMatrixMultiply},{"D3DXMatrixTranspose",dxMatrixTranspose},{"D3DXMatrixTranslation",dxMatrixTranslation},
    {"D3DXMatrixScaling",dxMatrixScaling},{"D3DXMatrixRotationX",dxMatrixRotationX},{"D3DXMatrixRotationY",dxMatrixRotationY},
    {"D3DXMatrixRotationZ",dxMatrixRotationZ},{"D3DXMatrixRotationAxis",dxMatrixRotationAxis},{"D3DXMatrixRotationQuaternion",dxMatrixRotationQuaternion},
    {"D3DXMatrixInverse",dxMatrixInverse},{"D3DXVec3TransformCoord",dxVec3TransformCoord},{"D3DXVec3TransformNormal",dxVec3TransformNormal},
    {"D3DXVec4Transform",dxVec4Transform},{"D3DXVec3Normalize",dxVec3Normalize},{"D3DXQuaternionRotationAxis",dxQuaternionRotationAxis},
    {"D3DXQuaternionMultiply",dxQuaternionMultiply},{"D3DXQuaternionInverse",dxQuaternionInverse},{"D3DXQuaternionSlerp",dxQuaternionSlerp},
    {"D3DXQuaternionRotationMatrix",dxQuaternionRotationMatrix},
    {"D3DXCreateTextureFromFileInMemoryEx",dxCreateTextureFromFileInMemoryEx},{"D3DXCreateCubeTextureFromFileInMemoryEx",dxCreateCubeTextureFromFileInMemoryEx},
    {"D3DXCreateTexture",dxCreateTexture},{"D3DXGetShaderConstantTable",dxGetShaderConstantTable},
};
