#pragma once
#include <cstdint>
struct ANativeWindow;
void configureD3D9Bridge(void* guestBase,uint32_t (*allocator)(uint32_t),void (*deallocator)(uint32_t));
void shutdownD3D9Bridge();
void setD3D9HostWindow(ANativeWindow* window);
void setD3D9RenderSize(unsigned width,unsigned height);
// Launcher compatibility options; call before the game creates its device.
void setD3D9Compatibility(bool cpuTextures,unsigned backBuffers);
bool d3d9RequestsSurface();
void acknowledgeD3D9Surface(bool available);
uint32_t createGuestD3D9(uint32_t sdk);
uint32_t dispatchGuestD3D9(uint32_t token,const uint32_t* args,uint32_t* argumentCount);
// On-screen pad overlay (Android touch, pc/android/mgr_android_main.cpp): circles drawn over the frame before Present.
// Each circle: x, y (0..1 of the screen), radius (fraction of the screen height), RGBA (0..1), filled (0/1) = 9 floats.
void setTouchOverlay(const float* circles,int count);
