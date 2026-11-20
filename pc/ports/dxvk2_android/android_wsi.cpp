// Android ANativeWindow WSI driver for DXVK 2.7.1 ("Android", the default without DXVK_WSI_DRIVER).
// Same extern "C" interface as ../dxvk_android/android_wsi.cpp (DXVK 1.9.2b), so the runtime
// (pc/runtime/d3d9_bridge.cpp, pc/android/mgr_loader.cpp) can use either library unchanged.
#include <android/native_window.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>
#include "../../runtime/android_resolution.h"
#include "src/wsi/wsi_platform.h"
#include "src/wsi/wsi_monitor.h"
#include "src/wsi/wsi_window.h"

namespace {
std::mutex windowMutex;
ANativeWindow* currentWindow = nullptr;
uintptr_t windowGeneration = 0x1000;
HWND currentHandle = nullptr;
uint32_t screenWidth = 1280, screenHeight = 720;
uint32_t renderWidth = 0, renderHeight = 0;
// Buffer size set on the current window by resizeWindow (D3D9 Reset), 0 = the
// window's own size. The swap chain must use it: a screen-sized swap chain on
// smaller buffers is cropped to their top-left corner by Android.
uint32_t bufferWidth = 0, bufferHeight = 0;
// Each Vulkan surface holds a reference to the window it was made from until DXVK
// destroys it (Presenter::destroySurface calls dxvkAndroidSurfaceDestroyed).
std::vector<std::pair<VkSurfaceKHR, ANativeWindow*>> surfaceWindows;
HMONITOR monitor() { return reinterpret_cast<HMONITOR>(uintptr_t{1}); }
// The Vulkan driver DXVK talks to: the linked system loader, or an imported
// adrenotools driver chosen in the launcher (set before the device exists).
std::atomic<PFN_vkGetInstanceProcAddr> instanceProcAddr{nullptr};
}

extern "C" __attribute__((visibility("default"))) void dxvkAndroidSetInstanceProcAddr(PFN_vkGetInstanceProcAddr entry) {
    instanceProcAddr = entry;
}
extern "C" __attribute__((visibility("default"))) PFN_vkGetInstanceProcAddr dxvkAndroidGetInstanceProcAddr() {
    auto entry = instanceProcAddr.load();
    return entry ? entry : vkGetInstanceProcAddr;
}
extern "C" __attribute__((visibility("default"))) void dxvkAndroidSetRenderSize(uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> guard(windowMutex);
    renderWidth = width; renderHeight = height;
    // Same as dxvkAndroidSetWindow when the window came first.
    if (currentWindow && !bufferWidth && width && height && width < screenWidth && height < screenHeight &&
        ANativeWindow_setBuffersGeometry(currentWindow, width, height, 0) == 0) { bufferWidth = width; bufferHeight = height; }
}
extern "C" __attribute__((visibility("default"))) void dxvkAndroidSurfaceDestroyed(VkSurfaceKHR surface) {
    std::lock_guard<std::mutex> guard(windowMutex);
    for (auto entry = surfaceWindows.begin(); entry != surfaceWindows.end(); ++entry)
        if (entry->first == surface) { ANativeWindow_release(entry->second); surfaceWindows.erase(entry); return; }
}
extern "C" __attribute__((visibility("default"))) HWND dxvkAndroidGetWindowHandle() {
    std::lock_guard<std::mutex> guard(windowMutex);
    return currentHandle;
}
extern "C" __attribute__((visibility("default"))) void dxvkAndroidSetWindow(ANativeWindow* window) {
    std::lock_guard<std::mutex> guard(windowMutex);
    if (window == currentWindow) return;
    if (window) { ANativeWindow_acquire(window); screenWidth = ANativeWindow_getWidth(window); screenHeight = ANativeWindow_getHeight(window); }
    if (currentWindow) ANativeWindow_release(currentWindow);
    currentWindow = window; bufferWidth = bufferHeight = 0;
    // Below native resolution, present at the render size and let the Android
    // compositor scale the surface (no full-screen blit every frame).
    if (window && renderWidth && renderHeight && renderWidth < screenWidth && renderHeight < screenHeight &&
        ANativeWindow_setBuffersGeometry(window, renderWidth, renderHeight, 0) == 0) { bufferWidth = renderWidth; bufferHeight = renderHeight; }
    currentHandle = window ? reinterpret_cast<HWND>(++windowGeneration) : nullptr;
}

namespace dxvk::wsi {

class AndroidWsiDriver : public WsiDriver {
public:
    std::vector<const char*> getInstanceExtensions() override {
        return { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME };
    }

    HMONITOR getDefaultMonitor() override { return monitor(); }
    HMONITOR enumMonitors(uint32_t index) override { return index ? nullptr : monitor(); }
    HMONITOR enumMonitors(const LUID*[], uint32_t, uint32_t index) override { return enumMonitors(index); }

    bool getDisplayName(HMONITOR handle, WCHAR (&name)[32]) override {
        if (handle != monitor()) return false;
        const wchar_t display[] = L"\\\\.\\DISPLAY1";
        std::fill(std::begin(name), std::end(name), 0);
        std::copy(std::begin(display), std::end(display), name);
        return true;
    }
    bool getDesktopCoordinates(HMONITOR handle, RECT* rect) override {
        if (handle != monitor() || !rect) return false;
        std::lock_guard<std::mutex> guard(windowMutex);
        *rect = {0, 0, static_cast<LONG>(screenWidth), static_cast<LONG>(screenHeight)};
        return true;
    }
    bool getDisplayMode(HMONITOR handle, uint32_t index, WsiMode* mode) override {
        if (handle != monitor() || !mode) return false;
        std::lock_guard<std::mutex> guard(windowMutex);
        if (!renderWidth || !renderHeight) {
            static constexpr AndroidRenderMode pcModes[] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 960}, {1280, 1024}, {1600, 1200}};
            if (index > 6) return false;
            auto selected = index == 6 ? AndroidRenderMode{screenWidth, screenHeight} : pcModes[index];
            *mode = {selected.width, selected.height, {60, 1}, 32, false};
            return true;
        }
        if (index >= 6) return false;
        auto sizes = androidRenderModes(screenWidth, screenHeight, renderWidth, renderHeight);
        *mode = {sizes[index].width, sizes[index].height, {60, 1}, 32, false};
        return true;
    }
    bool getCurrentDisplayMode(HMONITOR handle, WsiMode* mode) override {
        if (handle != monitor() || !mode) return false;
        std::lock_guard<std::mutex> guard(windowMutex);
        *mode = {screenWidth, screenHeight, {60, 1}, 32, false};
        return true;
    }
    bool getDesktopDisplayMode(HMONITOR handle, WsiMode* mode) override { return getCurrentDisplayMode(handle, mode); }
    WsiEdidData getMonitorEdid(HMONITOR) override { return {}; }

    void getWindowSize(HWND window, uint32_t* width, uint32_t* height) override {
        std::lock_guard<std::mutex> guard(windowMutex);
        bool valid = currentWindow && window == currentHandle;
        if (width) *width = valid ? (bufferWidth ? bufferWidth : screenWidth) : 0;
        if (height) *height = valid ? (bufferHeight ? bufferHeight : screenHeight) : 0;
    }
    void resizeWindow(HWND window, DxvkWindowState*, uint32_t width, uint32_t height) override {
        std::lock_guard<std::mutex> guard(windowMutex);
        if (currentWindow && window == currentHandle && ANativeWindow_setBuffersGeometry(currentWindow, width, height, 0) == 0) {
            bufferWidth = width; bufferHeight = height;
        }
    }
    bool setWindowMode(HMONITOR handle, HWND window, DxvkWindowState*, const WsiMode&) override {
        return handle == monitor() && isWindow(window);
    }
    bool enterFullscreenMode(HMONITOR handle, HWND window, DxvkWindowState*, bool) override {
        return handle == monitor() && isWindow(window);
    }
    bool leaveFullscreenMode(HWND window, DxvkWindowState*, bool) override { return isWindow(window); }
    bool restoreDisplayMode() override { return true; }
    HMONITOR getWindowMonitor(HWND window) override { return isWindow(window) ? monitor() : nullptr; }
    bool isWindow(HWND window) override {
        std::lock_guard<std::mutex> guard(windowMutex);
        return currentWindow && window == currentHandle;
    }
    bool isMinimized(HWND) override { return false; }
    bool isOccluded(HWND) override { return false; }
    void updateFullscreenWindow(HMONITOR, HWND, bool) override { }

    VkResult createSurface(HWND window, PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkInstance instance,
                           VkSurfaceKHR* surface) override {
        std::lock_guard<std::mutex> guard(windowMutex);
        if (!currentWindow || window != currentHandle) return VK_ERROR_SURFACE_LOST_KHR;
        if (!getInstanceProcAddr) getInstanceProcAddr = dxvkAndroidGetInstanceProcAddr();
        auto create = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(getInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR"));
        if (!create) return VK_ERROR_EXTENSION_NOT_PRESENT;
        VkAndroidSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
        info.window = currentWindow;
        VkResult result = create(instance, &info, nullptr, surface);
        if (result == VK_SUCCESS) { ANativeWindow_acquire(currentWindow); surfaceWindows.emplace_back(*surface, currentWindow); }
        return result;
    }
};

static bool createAndroidWsiDriver(WsiDriver** driver) {
    *driver = new AndroidWsiDriver();
    return true;
}

WsiBootstrap AndroidWSI = { "Android", createAndroidWsiDriver };

}
