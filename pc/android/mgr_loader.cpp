// NativeActivity entry of the MGR:R PC lab APK. The APK carries only this loader: the game library (runtime + lifted
// code, built from the player's executable) is opened from outside the APK, so no game code is ever inside it
// (ProjectSwordMan distribution rule). Each library is read into an anonymous memfd and opened with
// android_dlopen_ext(ANDROID_DLEXT_USE_LIBRARY_FD): nothing is written to internal storage (the release launcher will
// decrypt libgame straight into the memfd, so the clear code never touches the disk).
// Search order: <files>/<name>, then the directory named in <files>/mgr_libdir.txt (lab: the SD card).
// Vulkan driver: the system one, unless <files>/mgr_driver.txt names a driver library inside <files>/drivers/ (Turnip);
// then it is opened with libadrenotools and handed to DXVK (dxvkAndroidSetInstanceProcAddr). Never switched silently.
#include <android_native_app_glue.h>
#include <android/dlext.h>
#include <android/log.h>
#include <adrenotools/driver.h>
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <vector>

namespace {
std::string firstLine(const std::string& path) { std::ifstream f(path); std::string s; std::getline(f, s); return s; }
void* openFromMemory(const std::string& path, const char* name, int flags) {
    int in = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (in < 0) return nullptr;
    int mem = int(syscall(__NR_memfd_create, name, MFD_CLOEXEC)); // libc memfd_create needs API 30
    std::vector<char> chunk(8 << 20); ssize_t got; size_t total = 0;
    while (mem >= 0 && (got = read(in, chunk.data(), chunk.size())) > 0) {
        for (ssize_t done = 0; done < got;) { ssize_t w = write(mem, chunk.data() + done, size_t(got - done)); if (w <= 0) { close(mem); mem = -1; break; } done += w; }
        total += size_t(got);
    }
    close(in);
    if (mem < 0) return nullptr;
    android_dlextinfo info{}; info.flags = ANDROID_DLEXT_USE_LIBRARY_FD; info.library_fd = mem;
    void* handle = android_dlopen_ext(name, flags, &info);
    __android_log_print(handle ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "MGRLAB", "%s: %zu bytes from %s via memfd: %s", name, total,
        path.c_str(), handle ? "loaded" : dlerror());
    close(mem);
    return handle;
}
void* openFd(int fd, const char* name, int flags) {
    android_dlextinfo info{}; info.flags = ANDROID_DLEXT_USE_LIBRARY_FD; info.library_fd = fd;
    void* handle = android_dlopen_ext(name, flags, &info);
    __android_log_print(handle ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "MGRLAB", "%s from launcher memfd %d: %s", name, fd, handle ? "loaded" : dlerror());
    return handle;
}
std::string env(const char* name) { const char* v = getenv(name); return v ? v : ""; }
void* openLibrary(const std::string& files, const char* name, int flags) {
    // Launcher (app/.../pc/PcGameActivity.kt): the game library was decrypted into a memfd; DXVK ships in the APK.
    if (!env("MGR_LIB_FD").empty()) {
        if (std::string(name) == "libmgrpc.so") return openFd(atoi(env("MGR_LIB_FD").c_str()), name, flags);
        return dlopen(name, flags);
    }
    std::string local = files + "/" + name;
    if (access(local.c_str(), R_OK) == 0) return dlopen(local.c_str(), flags);
    std::string dir = firstLine(files + "/mgr_libdir.txt");
    return dir.empty() ? nullptr : openFromMemory(dir + "/" + name, name, flags);
}
}

void android_main(android_app* app) {
    std::string files = app->activity->internalDataPath ? app->activity->internalDataPath : "";
    void* dxvk = openLibrary(files, "libdxvk_android.so", RTLD_NOW | RTLD_GLOBAL);
    if (!env("MGR_START_ERROR").empty()) __android_log_print(ANDROID_LOG_ERROR, "MGRLAB", "launcher: %s", env("MGR_START_ERROR").c_str());
    std::string driver = env("MGR_DRIVER_NAME").empty() ? firstLine(files + "/mgr_driver.txt") : env("MGR_DRIVER_NAME");
    if (dxvk && !driver.empty()) {
        // Hook libraries ship in the APK's lib dir (extractNativeLibs=true): <data app dir>/lib/arm64.
        Dl_info self{}; dladdr(reinterpret_cast<void*>(&android_main), &self);
        std::string libDir = self.dli_fname ? std::string(self.dli_fname) : "";
        libDir = libDir.substr(0, libDir.find_last_of('/') + 1);
        if (!env("MGR_HOOK_DIR").empty()) libDir = env("MGR_HOOK_DIR");
        std::string driverDir = env("MGR_DRIVER_DIR").empty() ? files + "/drivers/" : env("MGR_DRIVER_DIR"), temp = files + "/drvtmp/";
        mkdir(temp.c_str(), 0700);
        void* vk = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, temp.c_str(), libDir.c_str(),
            driverDir.c_str(), driver.c_str(), nullptr, nullptr);
        auto entry = vk ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(vk, "vkGetInstanceProcAddr")) : nullptr;
        auto set = reinterpret_cast<void (*)(PFN_vkGetInstanceProcAddr)>(dlsym(dxvk, "dxvkAndroidSetInstanceProcAddr"));
        __android_log_print(entry && set ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "MGRLAB", "Vulkan driver %s%s (hooks %s): %s",
            driverDir.c_str(), driver.c_str(), libDir.c_str(), entry && set ? "custom driver active" : dlerror());
        if (entry && set) set(entry);
    } else if (dxvk) __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "Vulkan driver: system");
    void* game = dxvk ? openLibrary(files, "libmgrpc.so", RTLD_NOW) : nullptr;
    auto entry = game ? reinterpret_cast<void (*)(android_app*)>(dlsym(game, "mgr_android_main")) : nullptr;
    if (!entry) {
        __android_log_print(ANDROID_LOG_ERROR, "MGRLAB", "game library not loaded (files %s): %s", files.c_str(), dlerror());
        ANativeActivity_finish(app->activity);
        while (!app->destroyRequested) {
            int events; android_poll_source* source = nullptr;
            if (ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0 && source) source->process(app, source);
        }
        return;
    }
    entry(app);
}
