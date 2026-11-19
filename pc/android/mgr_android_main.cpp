// Android host loop of the MGR:R PC port (lab). Window and pause handling follow codepdbh/nfsu2-recompiled
// android/app/src/main/cpp/main.cpp (MIT); no splash renderer, no touch yet.
// Lab knobs: <files>/mgr_exe.txt (path of the player's exe), <files>/mgr_env.txt (KEY=VALUE lines set before boot).
#include <android_native_app_glue.h>
#include <android/log.h>
#include <fstream>
#include <string>
#include <cstdlib>
#include "guest_runtime.h"
#include "guest_boot_worker.h"
#include "d3d9_bridge.h"

namespace {
struct Host { bool resumed = false, started = false, sized = false; };
void onCommand(android_app* app, int32_t command) {
    auto& h = *static_cast<Host*>(app->userData);
    if (command == APP_CMD_RESUME) h.resumed = true;
    if (command == APP_CMD_PAUSE) { setGuestPaused(true); h.resumed = false; acknowledgeD3D9Surface(false); setD3D9HostWindow(nullptr); }
    if (command == APP_CMD_TERM_WINDOW || command == APP_CMD_WINDOW_RESIZED || command == APP_CMD_INIT_WINDOW) {
        setGuestPaused(true); acknowledgeD3D9Surface(false); setD3D9HostWindow(nullptr);
    }
}
std::string firstLine(const std::string& path) { std::ifstream f(path); std::string s; std::getline(f, s); return s; }
void loadEnv(const std::string& path) {
    std::ifstream f(path); std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        setenv(line.substr(0, eq).c_str(), line.substr(eq + 1).c_str(), 1);
        __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "env %s", line.c_str());
    }
}
}

extern "C" __attribute__((visibility("default"))) void mgr_android_main(android_app* app) {
    std::string files = app->activity->internalDataPath ? app->activity->internalDataPath : "";
    setenv("DXVK_LOG_PATH", "none", 0);
    setenv("DXVK_STATE_CACHE_PATH", files.c_str(), 0);
    loadEnv(files + "/mgr_env.txt");
    std::string exe = firstLine(files + "/mgr_exe.txt");
    if (exe.empty()) exe = "/storage/6A55-7C3B/MGRPC/METAL GEAR RISING REVENGEANCE.exe";
    Host host; app->userData = &host; app->onAppCmd = onCommand;
    GuestBootWorker worker; std::string status;
    while (!app->destroyRequested) {
        int events; android_poll_source* source = nullptr;
        bool active = host.resumed && app->window;
        int id = ALooper_pollOnce(active ? 16 : -1, nullptr, &events, reinterpret_cast<void**>(&source));
        if (id >= 0 && source) source->process(app, source);
        if (app->destroyRequested) break;
        if (!host.resumed || !app->window) continue;
        try {
            setD3D9HostWindow(app->window);
            if (d3d9RequestsSurface()) acknowledgeD3D9Surface(true);
            setGuestPaused(false);
            if (!host.sized) { host.sized = true; setGuestDisplaySize(unsigned(ANativeWindow_getWidth(app->window)), unsigned(ANativeWindow_getHeight(app->window))); }
            if (!host.started) { host.started = true; __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "boot %s", exe.c_str()); worker.start(exe); }
            auto now = worker.status();
            if (now != status) { status = now; __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "status: %s", status.c_str()); }
        } catch (const std::exception& e) {
            __android_log_print(ANDROID_LOG_ERROR, "MGRLAB", "%s", e.what());
        }
    }
    acknowledgeD3D9Surface(false); setD3D9HostWindow(nullptr);
}
