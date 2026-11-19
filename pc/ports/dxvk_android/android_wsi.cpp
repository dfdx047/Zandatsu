#include <cstdint>
#include "../../runtime/android_resolution.h"
#ifdef __ANDROID__
#include <android/native_window.h>
#else
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#undef None
#undef Always
#undef Status
// Linux bench: display==nullptr means a VK_EXT_headless_surface (NFS_D3D9_HEADLESS=1), used where X11 presentation
// is broken; frames are then seen only through the bridge's backbuffer capture.
struct ANativeWindow {Display* display;Window window;uint32_t width,height;};
#include <cstdlib>
static bool headlessWsi(){return std::getenv("NFS_D3D9_HEADLESS")!=nullptr;}
#endif
#include <mutex>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include "src/wsi/wsi_presenter.h"
#include "src/wsi/wsi_window.h"
#include "src/wsi/wsi_monitor.h"
#include "src/dxvk/dxvk_platform_exts.h"

namespace {
std::mutex windowMutex;
ANativeWindow* currentWindow=nullptr;
#ifdef __ANDROID__
uintptr_t windowGeneration=0x1000;
#endif
HWND currentHandle=nullptr;
uint32_t screenWidth=1280,screenHeight=720;
uint32_t renderWidth=0,renderHeight=0;
// Buffer size set on the current window by resizeWindow (D3D9 Reset), 0 = the
// window's own size. The swap chain must use it: a screen-sized swap chain on
// smaller buffers is cropped to their top-left corner by Android.
uint32_t bufferWidth=0,bufferHeight=0;
#ifdef __ANDROID__
// Each Vulkan surface holds a reference to the window it was made from until DXVK
// destroys it. Android frees the window when the app goes to the background; the
// presenter destroys the old surface only on the next Present, and doing that on
// a freed window crashed inside the driver.
std::vector<std::pair<VkSurfaceKHR,ANativeWindow*>> surfaceWindows;
#endif
HMONITOR monitor(){return reinterpret_cast<HMONITOR>(uintptr_t{1});}
// The Vulkan driver DXVK talks to: the linked system loader, or an imported
// adrenotools driver chosen in the launcher (set before the device exists).
std::atomic<PFN_vkGetInstanceProcAddr> instanceProcAddr{nullptr};
}
extern "C" void dxvkAndroidSetInstanceProcAddr(PFN_vkGetInstanceProcAddr entry){instanceProcAddr=entry;}
extern "C" PFN_vkGetInstanceProcAddr dxvkAndroidGetInstanceProcAddr(){
    auto entry=instanceProcAddr.load();return entry?entry:vkGetInstanceProcAddr;
}
extern "C" void dxvkAndroidSetRenderSize(uint32_t width,uint32_t height){std::lock_guard<std::mutex> guard(windowMutex);renderWidth=width;renderHeight=height;
#ifdef __ANDROID__
    // Same as dxvkAndroidSetWindow when the window came first.
    if(currentWindow&&!bufferWidth&&width&&height&&width<screenWidth&&height<screenHeight&&
       ANativeWindow_setBuffersGeometry(currentWindow,width,height,0)==0){bufferWidth=width;bufferHeight=height;}
#endif
}
#ifdef __ANDROID__
extern "C" void dxvkAndroidSurfaceDestroyed(VkSurfaceKHR surface){
    std::lock_guard<std::mutex> guard(windowMutex);
    for(auto entry=surfaceWindows.begin();entry!=surfaceWindows.end();++entry)
        if(entry->first==surface){ANativeWindow_release(entry->second);surfaceWindows.erase(entry);return;}
}
#endif
extern "C" HWND dxvkAndroidGetWindowHandle(){std::lock_guard<std::mutex> guard(windowMutex);return currentHandle;}
extern "C" void dxvkAndroidSetWindow(ANativeWindow* window){
    std::lock_guard<std::mutex> guard(windowMutex);
    if(window==currentWindow)return;
#ifdef __ANDROID__
    if(window){ANativeWindow_acquire(window);screenWidth=ANativeWindow_getWidth(window);screenHeight=ANativeWindow_getHeight(window);}
    if(currentWindow)ANativeWindow_release(currentWindow);
#endif
    currentWindow=window;bufferWidth=bufferHeight=0;
#ifdef __ANDROID__
    // Below native resolution, present at the render size and let the Android
    // compositor scale the surface: the swap chain blit no longer writes a
    // full-screen image every frame (Adreno 610: 2.5 MB per frame at 2340x1080).
    if(window&&renderWidth&&renderHeight&&renderWidth<screenWidth&&renderHeight<screenHeight&&
       ANativeWindow_setBuffersGeometry(window,renderWidth,renderHeight,0)==0){bufferWidth=renderWidth;bufferHeight=renderHeight;}
    currentHandle=window?reinterpret_cast<HWND>(++windowGeneration):nullptr;
#else
    currentHandle=window;
#endif
}
#ifndef __ANDROID__
extern "C" ANativeWindow* dxvkCreateTestWindow(){
    if(headlessWsi())return new ANativeWindow{nullptr,0,1280,720};
    XInitThreads();auto display=XOpenDisplay(nullptr);if(!display)return nullptr;
    Window window=XCreateSimpleWindow(display,DefaultRootWindow(display),0,0,1280,720,0,0,0);
    XMapWindow(display,window);XSync(display,False);return new ANativeWindow{display,window,1280,720};
}
extern "C" void dxvkDestroyTestWindow(ANativeWindow* window){if(!window)return;if(!window->display){delete window;return;}XDestroyWindow(window->display,window->window);XCloseDisplay(window->display);delete window;}
#endif
namespace dxvk {
DxvkPlatformExts DxvkPlatformExts::s_instance;
std::string_view DxvkPlatformExts::getName(){return "Android WSI";}
DxvkNameSet DxvkPlatformExts::getInstanceExtensions(){DxvkNameSet names;names.add(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef __ANDROID__
names.add(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#else
if(headlessWsi())names.add(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);else names.add(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
return names;}
DxvkNameSet DxvkPlatformExts::getDeviceExtensions(uint32_t){return {};}
void DxvkPlatformExts::initInstanceExtensions(){}
void DxvkPlatformExts::initDeviceExtensions(const DxvkInstance*){}
}
namespace dxvk::wsi {
VkResult createSurface(HWND window,const Rc<vk::InstanceFn>& instance,VkSurfaceKHR* surface){
    std::lock_guard<std::mutex> guard(windowMutex);
    if(!currentWindow||window!=currentHandle)return VK_ERROR_SURFACE_LOST_KHR;
#ifdef __ANDROID__
    auto create=reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(dxvkAndroidGetInstanceProcAddr()(instance->instance(),"vkCreateAndroidSurfaceKHR"));
    if(!create)return VK_ERROR_EXTENSION_NOT_PRESENT;
    VkAndroidSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};info.window=currentWindow;
    VkResult result=create(instance->instance(),&info,nullptr,surface);
    if(result==VK_SUCCESS){ANativeWindow_acquire(currentWindow);surfaceWindows.emplace_back(*surface,currentWindow);}
    return result;
#else
    if(!currentWindow->display){
        auto headless=reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(vkGetInstanceProcAddr(instance->instance(),"vkCreateHeadlessSurfaceEXT"));
        if(!headless)return VK_ERROR_EXTENSION_NOT_PRESENT;
        VkHeadlessSurfaceCreateInfoEXT info{VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT};
        return headless(instance->instance(),&info,nullptr,surface);
    }
    auto create=reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(vkGetInstanceProcAddr(instance->instance(),"vkCreateXlibSurfaceKHR"));
    if(!create)return VK_ERROR_EXTENSION_NOT_PRESENT;
    VkXlibSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};info.dpy=currentWindow->display;info.window=currentWindow->window;
    return create(instance->instance(),&info,nullptr,surface);
#endif
}
HMONITOR getDefaultMonitor(){return monitor();}
HMONITOR enumMonitors(uint32_t index){return index?nullptr:monitor();}
bool getDisplayName(HMONITOR handle,WCHAR (&name)[32]){if(handle!=monitor())return false;const wchar_t display[]=L"\\\\.\\DISPLAY1";std::fill(std::begin(name),std::end(name),0);std::copy(std::begin(display),std::end(display),name);return true;}
bool getDesktopCoordinates(HMONITOR handle,RECT* rect){if(handle!=monitor()||!rect)return false;std::lock_guard<std::mutex> guard(windowMutex);*rect={0,0,static_cast<LONG>(screenWidth),static_cast<LONG>(screenHeight)};return true;}
bool getCurrentDisplayMode(HMONITOR handle,WsiMode* mode){if(handle!=monitor()||!mode)return false;std::lock_guard<std::mutex> guard(windowMutex);*mode={screenWidth,screenHeight,{60,1},32,false};return true;}
bool getDesktopDisplayMode(HMONITOR handle,WsiMode* mode){return getCurrentDisplayMode(handle,mode);}
bool getDisplayMode(HMONITOR handle,uint32_t index,WsiMode* mode){
    if(handle!=monitor()||!mode)return false;
    std::lock_guard<std::mutex> guard(windowMutex);
    if(!renderWidth||!renderHeight){
        static constexpr AndroidRenderMode pcModes[]={{640,480},{800,600},{1024,768},{1280,960},{1280,1024},{1600,1200}};
        if(index>6)return false;
        auto selected=index==6?AndroidRenderMode{screenWidth,screenHeight}:pcModes[index];
        *mode={selected.width,selected.height,{60,1},32,false};return true;
    }
    if(index>=6)return false;
    auto sizes=androidRenderModes(screenWidth,screenHeight,renderWidth?renderWidth:screenWidth,renderHeight?renderHeight:screenHeight);
    *mode={sizes[index].width,sizes[index].height,{60,1},32,false};return true;
}
void getWindowSize(HWND window,uint32_t* width,uint32_t* height){std::lock_guard<std::mutex> guard(windowMutex);bool valid=currentWindow&&window==currentHandle;if(width)*width=valid?(bufferWidth?bufferWidth:screenWidth):0;if(height)*height=valid?(bufferHeight?bufferHeight:screenHeight):0;}
void resizeWindow(HWND window,DxvkWindowState*,uint32_t width,uint32_t height){std::lock_guard<std::mutex> guard(windowMutex);
#ifdef __ANDROID__
if(currentWindow&&window==currentHandle&&ANativeWindow_setBuffersGeometry(currentWindow,width,height,0)==0){bufferWidth=width;bufferHeight=height;}
#else
(void)window;(void)width;(void)height;
#endif
}
bool isWindow(HWND window){std::lock_guard<std::mutex> guard(windowMutex);return currentWindow&&window==currentHandle;}
HMONITOR getWindowMonitor(HWND window){return isWindow(window)?monitor():nullptr;}
bool setWindowMode(HMONITOR handle,HWND window,const WsiMode*,bool){return handle==monitor()&&isWindow(window);}
bool enterFullscreenMode(HMONITOR handle,HWND window,DxvkWindowState*,bool){return handle==monitor()&&isWindow(window);}
bool leaveFullscreenMode(HWND window,DxvkWindowState*){return isWindow(window);}
bool restoreDisplayMode(HMONITOR handle){return handle==monitor();}
}
