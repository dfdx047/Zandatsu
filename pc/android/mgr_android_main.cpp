// Android host loop of the MGR:R PC port (lab). Window and pause handling follow codepdbh/nfsu2-recompiled
// android/app/src/main/cpp/main.cpp (MIT); no splash renderer, no touch yet.
// Lab knobs: <files>/mgr_exe.txt (path of the player's exe), <files>/mgr_env.txt (KEY=VALUE lines set before boot).
#include <android_native_app_glue.h>
#include <android/log.h>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <deque>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <string>
#include <cstdlib>
#include "guest_runtime.h"
#include "guest_boot_worker.h"
#include "d3d9_bridge.h"
#include "gamepad_input.h"

namespace {
struct Host { bool resumed = false, started = false, sized = false; };
void setPadKey(unsigned scan, bool down);
void publishPad();
extern GamepadInput gamepad;
void onCommand(android_app* app, int32_t command) {
    auto& h = *static_cast<Host*>(app->userData);
    if (command == APP_CMD_RESUME) h.resumed = true;
    if (command == APP_CMD_PAUSE) { setGuestPaused(true); gamepad.releaseAll(setPadKey); publishPad(); clearGuestInput(); h.resumed = false; acknowledgeD3D9Surface(false); setD3D9HostWindow(nullptr); }
    if (command == APP_CMD_TERM_WINDOW || command == APP_CMD_WINDOW_RESIZED || command == APP_CMD_INIT_WINDOW) {
        setGuestPaused(true); acknowledgeD3D9Surface(false); setD3D9HostWindow(nullptr);
    }
}
// Input: the game reads the pad through XInput, so the Android pad (built-in or external) only feeds the XInput state
// (no keyboard emulation, it would double every press). Key events with gamepad key codes (also `adb shell input
// keyevent KEYCODE_BUTTON_A`) take the same path. Touch drives the mouse pointer (tap = left click).
GamepadInput gamepad;
void setPadKey(unsigned scan, bool down) { setGuestKey(scan, down); }
// Lab pilot: <files>/mgr_pilot.txt (pushed with run-as) holds steps run one after the other, then the file is deleted:
//   stick <x> <y> <ms> | rstick <x> <y> <ms> | btn <A|B|X|Y|LB|RB|LT|RT|BACK|START|LS|RS>[+...] <ms> | wait <ms>
// (x/y in -1..1, y down positive as Android reports it). The pilot is added on top of the real pad.
struct PilotStep { std::string kind; float x = 0, y = 0; uint32_t buttons = 0; float lt = 0, rt = 0; int ms = 0, hx = 0, hy = 0; };
std::deque<PilotStep> pilotSteps;
std::chrono::steady_clock::time_point pilotEnd{};
bool pilotActive = false;
PilotStep pilotNow;
uint32_t pilotButton(const std::string& name, float& lt, float& rt, int& hx, int& hy) {
    static const char* names[10] = {"A", "B", "X", "Y", "LB", "RB", "BACK", "START", "LS", "RS"};
    uint32_t bits = 0; std::stringstream list(name); std::string one;
    while (std::getline(list, one, '+')) {
        if (one == "LT") lt = 1; else if (one == "RT") rt = 1;
        else if (one == "UP") hy = -1; else if (one == "DOWN") hy = 1; else if (one == "LEFT") hx = -1; else if (one == "RIGHT") hx = 1;
        for (int i = 0; i < 10; ++i) if (one == names[i]) bits |= 1u << i;
    }
    return bits;
}
void pilotLoad(const std::string& path) {
    std::ifstream f(path); if (!f) return;
    std::string line; int n = 0;
    while (std::getline(f, line)) {
        std::stringstream in(line); PilotStep st; in >> st.kind;
        if (st.kind == "stick" || st.kind == "rstick") in >> st.x >> st.y >> st.ms;
        else if (st.kind == "btn") { std::string b; in >> b >> st.ms; st.buttons = pilotButton(b, st.lt, st.rt, st.hx, st.hy); }
        else if (st.kind == "wait") in >> st.ms; else continue;
        pilotSteps.push_back(st); ++n;
    }
    f.close(); std::remove(path.c_str());
    __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "pilot: %d steps loaded", n);
}
bool pilotTick() {   // true when the pilot state changed
    auto now = std::chrono::steady_clock::now();
    if (pilotActive && now < pilotEnd) return false;
    bool was = pilotActive; pilotActive = false; pilotNow = PilotStep{};
    if (!pilotSteps.empty()) { pilotNow = pilotSteps.front(); pilotSteps.pop_front(); pilotActive = true; pilotEnd = now + std::chrono::milliseconds(pilotNow.ms); }
    return was || pilotActive;
}
void publishPad() {
    auto state = gamepad.padState();
    if (pilotActive) {
        if (pilotNow.kind == "stick") { state.leftX = pilotNow.x; state.leftY = pilotNow.y; }
        if (pilotNow.kind == "rstick") { state.rightX = pilotNow.x; state.rightY = pilotNow.y; }
        state.buttons |= pilotNow.buttons; if (pilotNow.hx) state.hatX = pilotNow.hx; if (pilotNow.hy) state.hatY = pilotNow.hy; state.leftTrigger = std::fmax(state.leftTrigger, pilotNow.lt); state.rightTrigger = std::fmax(state.rightTrigger, pilotNow.rt);
    }
    const float axes[6] = {state.leftX, state.leftY, state.leftTrigger - state.rightTrigger, state.rightX, state.rightY, 0.f};
    setGuestPadConnected(true);
    setGuestPad(axes, state.buttons, state.hatX, state.hatY);
}
// Native touch for menus and story (MGR's PC menus are keyboard/pad driven: a mouse click does not pick an item,
// build/lab/pc/runs/touch2): tap = A (confirm, skip), swipe = d-pad, two-finger tap = B (back). Each gesture becomes a
// short pad press through the pilot queue, so it is never shorter than one game poll.
struct Touch { float x = 0, y = 0; std::chrono::steady_clock::time_point at{}; int fingers = 0; bool active = false; } touch;
void touchPress(const char* names, int ms = 120) {
    PilotStep st; st.kind = "btn"; st.ms = ms; st.buttons = pilotButton(names, st.lt, st.rt, st.hx, st.hy);
    pilotSteps.push_back(st); PilotStep gap; gap.kind = "wait"; gap.ms = 60; pilotSteps.push_back(gap);
}
void touchGesture(const AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    int fingers = int(AMotionEvent_getPointerCount(event));
    if (action == AMOTION_EVENT_ACTION_DOWN) {
        touch = Touch{AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0), std::chrono::steady_clock::now(), 1, true};
        return;
    }
    if (!touch.active) return;
    if (action == AMOTION_EVENT_ACTION_POINTER_DOWN) touch.fingers = std::max(touch.fingers, fingers);
    if (action == AMOTION_EVENT_ACTION_CANCEL) { touch.active = false; return; }
    if (action != AMOTION_EVENT_ACTION_UP) return;
    touch.active = false;
    float dx = AMotionEvent_getX(event, 0) - touch.x, dy = AMotionEvent_getY(event, 0) - touch.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - touch.at).count();
    const char* press = nullptr;
    if (dist < 60 && ms < 500) press = touch.fingers >= 2 ? "B" : "A";
    else if (dist >= 100) press = std::fabs(dx) > std::fabs(dy) ? (dx > 0 ? "RIGHT" : "LEFT") : (dy > 0 ? "DOWN" : "UP");
    if (press) { touchPress(press); __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "touch: %s (%.0f px, %lld ms, %d fingers)", press, dist, (long long)ms, touch.fingers); }
}
int32_t onInput(android_app* app, AInputEvent* event) {
    int32_t type = AInputEvent_getType(event);
    static int logged = 0;
    if (type == AINPUT_EVENT_TYPE_KEY && logged++ < 40)
        __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "key %d action %d source %x", AKeyEvent_getKeyCode(event), AKeyEvent_getAction(event), AInputEvent_getSource(event));
    if (type == AINPUT_EVENT_TYPE_MOTION && (AInputEvent_getSource(event) & AINPUT_SOURCE_CLASS_JOYSTICK)) {
        bool handled = gamepad.motion(event, false, setPadKey); if (handled) publishPad(); return handled ? 1 : 0;
    }
    if (type == AINPUT_EVENT_TYPE_MOTION && (AInputEvent_getSource(event) & AINPUT_SOURCE_CLASS_POINTER)) {
        touchGesture(event);
        return 1;
    }
    if (type == AINPUT_EVENT_TYPE_KEY && gamepad.key(event, false, setPadKey)) { publishPad(); return 1; }
    return 0;
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
    Host host; app->userData = &host; app->onAppCmd = onCommand; app->onInputEvent = onInput;
    gamepad.setKeyEmulation(false);
    // The game polls XInput only while it saw a pad at start (later it waits for WM_DEVICECHANGE): handhelds have a
    // built-in pad, so report it connected before the boot.
    publishPad();
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
            pilotLoad(files + "/mgr_pilot.txt");
            if (pilotTick()) publishPad();
            auto now = worker.status();
            if (now != status) { status = now; __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "status: %s", status.c_str()); }
        } catch (const std::exception& e) {
            __android_log_print(ANDROID_LOG_ERROR, "MGRLAB", "%s", e.what());
        }
    }
    acknowledgeD3D9Surface(false); setD3D9HostWindow(nullptr);
}
