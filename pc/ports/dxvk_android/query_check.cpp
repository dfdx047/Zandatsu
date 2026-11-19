#include <d3d9.h>
#include <cstdio>
#include <stdexcept>
extern "C" IDirect3D9* Direct3DCreate9(UINT);
int main(){
    auto d3d=Direct3DCreate9(D3D_SDK_VERSION);if(!d3d)return 1;
    UINT adapters=d3d->GetAdapterCount();if(!adapters){d3d->Release();return 2;}
    for(UINT i=0;i<adapters;++i){
        D3DADAPTER_IDENTIFIER9 id{};D3DCAPS9 caps{};
        if(FAILED(d3d->GetAdapterIdentifier(i,0,&id))||FAILED(d3d->GetDeviceCaps(i,D3DDEVTYPE_HAL,&caps))){d3d->Release();return 3;}
        std::printf("Adapter %u: %s; vertex shader %x pixel shader %x; texture %ux%u\n",i,id.Description,caps.VertexShaderVersion,caps.PixelShaderVersion,caps.MaxTextureWidth,caps.MaxTextureHeight);
        D3DCAPS9 invalid{};if(SUCCEEDED(d3d->GetDeviceCaps(adapters,D3DDEVTYPE_HAL,&invalid))){d3d->Release();return 4;}
    }
    d3d->Release();std::puts("Native D3D9 adapter queries passed; no surface or draw test");return 0;
}
