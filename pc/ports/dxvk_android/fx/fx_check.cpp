// Host smoke test of the present effects (present_fx.cpp) on a Linux GPU, headless (VK_EXT_headless_surface):
// a D3D9 device with an 854x480 back buffer on a 1280x720 window, a box moving across a gradient for 120 frames,
// presented with whatever DXVK_FSR / DXVK_FI say. Passes if every Present succeeds; the present-fx statistics
// (stderr) show what ran and its GPU time. Built with DXVK_HOST_HEADLESS=ON (see CMakeLists.txt):
//   DXVK_FSR=1 DXVK_FI=1 NFS_D3D9_HEADLESS=1 ./dxvk_fx_check
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>

struct ANativeWindow;
extern "C" IDirect3D9* Direct3DCreate9(UINT);
extern "C" ANativeWindow* dxvkCreateTestWindow();
extern "C" void dxvkDestroyTestWindow(ANativeWindow* window);
extern "C" void dxvkAndroidSetWindow(ANativeWindow* window);
extern "C" HWND dxvkAndroidGetWindowHandle();

int main(int argc, char** argv) {
  setenv("NFS_D3D9_HEADLESS", "1", 0);
  const int frames = argc > 1 ? std::atoi(argv[1]) : 120;
  ANativeWindow* window = dxvkCreateTestWindow();
  if (!window) return 1;
  dxvkAndroidSetWindow(window);
  HWND hwnd = dxvkAndroidGetWindowHandle();

  IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (!d3d) return 2;
  D3DPRESENT_PARAMETERS pp = { };
  pp.BackBufferWidth = 854;
  pp.BackBufferHeight = 480;
  pp.BackBufferFormat = D3DFMT_X8R8G8B8;
  pp.BackBufferCount = 1;
  pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  pp.hDeviceWindow = hwnd;
  pp.Windowed = TRUE;
  pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // as the MGR bridge (FI forces FIFO itself)
  IDirect3DDevice9* dev = nullptr;
  if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev)) || !dev) {
    d3d->Release();
    return 3;
  }

  for (int f = 0; f < frames; f++) {
    // Background bands (static detail) and a box moving 6 px per frame (motion for the frame generation).
    for (int y = 0; y < 480; y += 40) {
      D3DRECT band = { 0, y, 854, y + 40 };
      dev->Clear(1, &band, D3DCLEAR_TARGET, D3DCOLOR_XRGB(20 + y / 3, 60, 200 - y / 3), 1.0f, 0);
    }
    int x = (f * 6) % 700;
    D3DRECT box = { x, 180, x + 120, 300 };
    dev->Clear(1, &box, D3DCLEAR_TARGET, D3DCOLOR_XRGB(250, 240, 40), 1.0f, 0);
    if (FAILED(dev->Present(nullptr, nullptr, nullptr, nullptr))) {
      std::fprintf(stderr, "Present %d failed\n", f);
      return 4;
    }
  }
  dev->Release();
  d3d->Release();
  dxvkDestroyTestWindow(window);
  std::printf("present effects check: %d frames presented\n", frames);
  return 0;
}
