// Android host loop of the MGR:R PC port (lab). Window and pause handling follow codepdbh/nfsu2-recompiled
// android/app/src/main/cpp/main.cpp (MIT); no splash renderer, no touch yet.
// Lab knobs: <files>/mgr_exe.txt (path of the player's exe), <files>/mgr_env.txt (KEY=VALUE lines set before boot).
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <jni.h>
#include <dlfcn.h>
#include <mutex>
#include <atomic>
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

void mgrPosProbe(int stage);   // d3d9_bridge.cpp, lab position probe
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
        else if (st.kind == "wait") in >> st.ms;
        else if (st.kind == "probe") { in >> st.hx; st.ms = 1; }   // lab position probe stage (d3d9_bridge.cpp)
        else continue;
        pilotSteps.push_back(st); ++n;
    }
    f.close(); std::remove(path.c_str());
    __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "pilot: %d steps loaded", n);
}
bool pilotTick() {   // true when the pilot state changed
    auto now = std::chrono::steady_clock::now();
    if (pilotActive && now < pilotEnd) return false;
    bool was = pilotActive; pilotActive = false; pilotNow = PilotStep{};
    if (!pilotSteps.empty()) { pilotNow = pilotSteps.front(); pilotSteps.pop_front(); pilotActive = true; pilotEnd = now + std::chrono::milliseconds(pilotNow.ms);
        if (pilotNow.kind == "probe") { mgrPosProbe(pilotNow.hx); pilotNow.hx = 0; } }
    return was || pilotActive;
}
android_app* touchApp = nullptr;
struct PadButton { const char* name; float x, y; float r, g, b; };
const PadButton padButtons[] = {
    {"A", 0.875f, 0.80f, 0.30f, 0.85f, 0.35f}, {"B", 0.945f, 0.64f, 0.90f, 0.30f, 0.30f},
    {"X", 0.805f, 0.64f, 0.30f, 0.55f, 0.95f}, {"Y", 0.875f, 0.48f, 0.95f, 0.85f, 0.25f},
    {"LT", 0.06f, 0.22f, 0.85f, 0.85f, 0.85f}, {"LB", 0.06f, 0.42f, 0.70f, 0.70f, 0.70f},
    {"RT", 0.94f, 0.22f, 0.85f, 0.85f, 0.85f}, {"RB", 0.82f, 0.22f, 0.70f, 0.70f, 0.70f},
};
const float padButtonR = 0.065f, stickR = 0.15f, knobR = 0.065f;
struct TouchPad {
    bool on = false; int32_t stickId = -1; float ox = 0.15f, oy = 0.70f, sx = 0, sy = 0;
    uint32_t pressed = 0; std::chrono::steady_clock::time_point lastUse{};
} pad;
float screenW() { return touchApp && touchApp->contentRect.right > 0 ? float(touchApp->contentRect.right) : touchApp && touchApp->window ? float(ANativeWindow_getWidth(touchApp->window)) : 1.f; }
float screenH() { return touchApp && touchApp->contentRect.bottom > 0 ? float(touchApp->contentRect.bottom) : touchApp && touchApp->window ? float(ANativeWindow_getHeight(touchApp->window)) : 1.f; }
// Launcher (PcGameActivity, 20/11): the 360 touch controls send their whole pad state here (nativeTouchPad); merged
// on top of the real pad like the pilot. Java input arrives on the UI thread: hostInput serializes it with the loop.
struct JavaPad { uint32_t buttons = 0; int hat = 0; float axes[6] = {}; } javaPad;
bool javaInput = false;
std::recursive_mutex hostInput;
void publishPad() {
    auto state = gamepad.padState();
    if (javaInput) {
        state.buttons |= javaPad.buttons;
        const float* a = javaPad.axes;
        if (std::fabs(a[0]) > 0.02f || std::fabs(a[1]) > 0.02f) { state.leftX = a[0]; state.leftY = a[1]; }
        if (std::fabs(a[2]) > 0.02f || std::fabs(a[3]) > 0.02f) { state.rightX = a[2]; state.rightY = a[3]; }
        state.leftTrigger = std::fmax(state.leftTrigger, a[4]); state.rightTrigger = std::fmax(state.rightTrigger, a[5]);
        if (javaPad.hat & 1) state.hatY = -1; if (javaPad.hat & 2) state.hatY = 1;
        if (javaPad.hat & 4) state.hatX = -1; if (javaPad.hat & 8) state.hatX = 1;
    }
    if (pad.on) {
        if (pad.stickId >= 0) { state.leftX = pad.sx; state.leftY = pad.sy; }
        for (unsigned b = 0; b < std::size(padButtons); ++b) if ((pad.pressed >> b) & 1) {
            float lt = 0, rt = 0; int hx = 0, hy = 0; state.buttons |= pilotButton(padButtons[b].name, lt, rt, hx, hy);
            state.leftTrigger = std::fmax(state.leftTrigger, lt); state.rightTrigger = std::fmax(state.rightTrigger, rt);
        }
    }
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
struct Touch { float x = 0, y = 0; std::chrono::steady_clock::time_point at{}; int fingers = 0; bool active = false; int32_t id = -1; float lx = 0, ly = 0; } touch;

// On-screen pad for fights (phase 5). Off in menus (gestures above); hold a finger ~300 ms on the left half to bring it
// up, three-finger tap or 8 s without touching it hides it again. Left: floating stick. Right: A/B/X/Y diamond,
// LT (blade mode), RT (ninja run), RB (lock-on switch), LB (sub-weapon), as on MGR's TYPE A controller screen.
// Drawn by the D3D9 bridge (setTouchOverlay) as plain coloured circles.
void padOverlay() {
    std::vector<float> c;
    auto circle = [&](float x, float y, float r, float cr, float cg, float cb, float a, bool fill) { c.insert(c.end(), {x, y, r, cr, cg, cb, a, 0.f, fill ? 1.f : 0.f}); };
    if (pad.on) {
        float aspect = screenH() / screenW();
        circle(pad.ox, pad.oy, stickR, 0.9f, 0.9f, 0.9f, 0.45f, false);
        circle(pad.ox + pad.sx * stickR * aspect, pad.oy + pad.sy * stickR, knobR, 0.9f, 0.9f, 0.9f, pad.stickId >= 0 ? 0.8f : 0.5f, true);
        for (unsigned i = 0; i < std::size(padButtons); ++i) { auto& b = padButtons[i]; circle(b.x, b.y, padButtonR, b.r, b.g, b.b, (pad.pressed >> i) & 1 ? 0.9f : 0.55f, (pad.pressed >> i) & 1); }
    }
    setTouchOverlay(c.data(), int(c.size() / 9));
}
void padShow(bool on) {
    if (pad.on == on) return;
    pad = TouchPad{}; pad.on = on; pad.lastUse = std::chrono::steady_clock::now();
    __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "touch pad %s", on ? "on" : "off"); padOverlay();
}
// All pointers of the event (minus the one going up) decide the pad state.
void padMotion(const AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event), masked = action & AMOTION_EVENT_ACTION_MASK;
    size_t upIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    size_t count = AMotionEvent_getPointerCount(event);
    if (masked == AMOTION_EVENT_ACTION_POINTER_DOWN && count >= 3) { padShow(false); return; }
    const float W = screenW(), H = screenH();
    uint32_t pressed = 0; bool stickSeen = false;
    for (size_t i = 0; i < count; ++i) {
        bool lifting = masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_CANCEL || (masked == AMOTION_EVENT_ACTION_POINTER_UP && i == upIndex);
        if (lifting) continue;
        int32_t id = AMotionEvent_getPointerId(event, i); float x = AMotionEvent_getX(event, i) / W, y = AMotionEvent_getY(event, i) / H;
        if (id == pad.stickId) {
            float dx = (x - pad.ox) * W / (stickR * H), dy = (y - pad.oy) / stickR; float len = std::sqrt(dx * dx + dy * dy);
            if (len > 1) { dx /= len; dy /= len; }
            pad.sx = dx; pad.sy = dy; stickSeen = true; continue;
        }
        bool hit = false;
        for (unsigned b = 0; b < std::size(padButtons); ++b) {
            float dx = (x - padButtons[b].x) * W, dy = (y - padButtons[b].y) * H;
            if (std::sqrt(dx * dx + dy * dy) < padButtonR * H * 1.35f) { pressed |= 1u << b; hit = true; }
        }
        bool fresh = masked == AMOTION_EVENT_ACTION_DOWN || (masked == AMOTION_EVENT_ACTION_POINTER_DOWN && i == upIndex);
        if (!hit && fresh && x < 0.5f && pad.stickId < 0) { pad.stickId = id; pad.ox = x; pad.oy = y; pad.sx = pad.sy = 0; stickSeen = true; }
    }
    if (!stickSeen) { pad.stickId = -1; pad.sx = pad.sy = 0; }
    pad.pressed = pressed; pad.lastUse = std::chrono::steady_clock::now();
    padOverlay(); publishPad();
}
// Main loop: a held finger on the left half turns the pad on; idle pad turns off.
void padTick() {
    auto now = std::chrono::steady_clock::now();
    if (!pad.on && touch.active && touch.fingers <= 1 && touch.x / screenW() < 0.5f &&
        std::hypot(touch.lx - touch.x, touch.ly - touch.y) < 60 && now - touch.at > std::chrono::milliseconds(300)) {
        float x = touch.x / screenW(), y = touch.y / screenH(); int32_t id = touch.id;
        touch.active = false; padShow(true); pad.stickId = id; pad.ox = x; pad.oy = y; padOverlay();
    }
    if (pad.on && pad.stickId < 0 && !pad.pressed && now - pad.lastUse > std::chrono::seconds(8)) padShow(false);
}
void touchPress(const char* names, int ms = 120) {
    PilotStep st; st.kind = "btn"; st.ms = ms; st.buttons = pilotButton(names, st.lt, st.rt, st.hx, st.hy);
    pilotSteps.push_back(st); PilotStep gap; gap.kind = "wait"; gap.ms = 60; pilotSteps.push_back(gap);
}
void touchGesture(const AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    int fingers = int(AMotionEvent_getPointerCount(event));
    if (action == AMOTION_EVENT_ACTION_DOWN) {
        touch = Touch{AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0), std::chrono::steady_clock::now(), 1, true, AMotionEvent_getPointerId(event, 0)};
        touch.lx = touch.x; touch.ly = touch.y;
        return;
    }
    if (!touch.active) return;
    if (action == AMOTION_EVENT_ACTION_MOVE) { touch.lx = AMotionEvent_getX(event, 0); touch.ly = AMotionEvent_getY(event, 0); }
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
        if (pad.on) padMotion(event); else touchGesture(event);
        return 1;
    }
    if (type == AINPUT_EVENT_TYPE_KEY && gamepad.key(event, false, setPadKey)) { publishPad(); return 1; }
    return 0;
}
int32_t onInputLocked(android_app* app, AInputEvent* event) { std::lock_guard<std::recursive_mutex> l(hostInput); return onInput(app, event); }
// JNI from PcGameActivity (registered on its class at start: this code is in the memfd lib, invisible to the class
// loader's name lookup). A*Event_fromJava is API 31: looked up at run time.
using FromJava = const AInputEvent* (*)(JNIEnv*, jobject);
void jniEvent(JNIEnv* env, jobject event, const char* name) {
    static FromJava key = reinterpret_cast<FromJava>(dlsym(RTLD_DEFAULT, "AKeyEvent_fromJava"));
    static FromJava motion = reinterpret_cast<FromJava>(dlsym(RTLD_DEFAULT, "AMotionEvent_fromJava"));
    static auto release = reinterpret_cast<void (*)(const AInputEvent*)>(dlsym(RTLD_DEFAULT, "AInputEvent_release"));
    FromJava from = name[0] == 'k' ? key : motion;
    if (!from || !touchApp) return;
    const AInputEvent* e = from(env, event); if (!e) return;
    onInputLocked(touchApp, const_cast<AInputEvent*>(e));
    if (release) release(e);
}
void JNICALL jniKey(JNIEnv* env, jobject, jobject event) { jniEvent(env, event, "key"); }
void JNICALL jniMotion(JNIEnv* env, jobject, jobject event) { jniEvent(env, event, "motion"); }
void JNICALL jniTouchPad(JNIEnv* env, jobject, jint buttons, jint hat, jfloatArray axes) {
    std::lock_guard<std::recursive_mutex> l(hostInput);
    javaPad.buttons = uint32_t(buttons); javaPad.hat = hat;
    if (axes && env->GetArrayLength(axes) >= 6) env->GetFloatArrayRegion(axes, 0, 6, javaPad.axes);
    publishPad();
}
// Menu lateral / editor do overlay aberto: o jogo para (maquina + audio), como no 360. O loop principal mantem a pausa
// enquanto o menu estiver aberto (antes desligava-a a cada volta).
std::atomic<bool> overlayPaused{false};
void JNICALL jniPause(JNIEnv*, jobject, jboolean paused) {
    __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "overlay menu %s", paused ? "pause" : "resume");
    overlayPaused = paused;
    setGuestPaused(paused);
}

void JNICALL jniPointer(JNIEnv*, jobject, jfloat x, jfloat y, jint phase) {
    setGuestPointer(x, y, phase);
    __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "touch click %.3f %.3f phase %d", x, y, phase);
}
JNIEnv* registerJava(android_app* app) {
    JNIEnv* env = nullptr;
    if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return nullptr;
    jclass cls = env->GetObjectClass(app->activity->clazz);
    const JNINativeMethod m[] = {
        {"nativeKey", "(Landroid/view/KeyEvent;)V", reinterpret_cast<void*>(jniKey)},
        {"nativeMotion", "(Landroid/view/MotionEvent;)V", reinterpret_cast<void*>(jniMotion)},
        {"nativeTouchPad", "(II[F)V", reinterpret_cast<void*>(jniTouchPad)},
        {"nativePointer", "(FFI)V", reinterpret_cast<void*>(jniPointer)},
        {"nativePause", "(Z)V", reinterpret_cast<void*>(jniPause)},
    };
    javaInput = env->RegisterNatives(cls, m, sizeof(m) / sizeof(m[0])) == JNI_OK;
    if (env->ExceptionCheck()) env->ExceptionClear();   // lab activity: no such methods
    __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "java input %s", javaInput ? "on (launcher overlay)" : "off");
    return env;
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
    touchApp = app;
    std::string files = app->activity->internalDataPath ? app->activity->internalDataPath : "";
    setenv("DXVK_LOG_PATH", "none", 0);
    setenv("DXVK_STATE_CACHE_PATH", files.c_str(), 0);
    loadEnv(files + "/mgr_env.txt");
    // Launcher "Limite de fps" (MGR_FPS_CAP): the guest Present waits for the pacer; 0 = the game's own 60 fps limit.
    if (const char* cap = getenv("MGR_FPS_CAP")) setGuestFrameLimit(unsigned(atoi(cap)));
    std::string exe = getenv("MGR_EXE") ? getenv("MGR_EXE") : firstLine(files + "/mgr_exe.txt");
    if (exe.empty()) exe = "/storage/6A55-7C3B/MGRPC/METAL GEAR RISING REVENGEANCE.exe";
    Host host; app->userData = &host; app->onAppCmd = onCommand; app->onInputEvent = onInputLocked;
    JNIEnv* jni = registerJava(app);
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
            setGuestPaused(overlayPaused.load());
            if (!host.sized) {
                // Free resolution (launcher MGR_RENDER_SIZE "WxH"): the window buffers get that size, the compositor scales.
                unsigned rw = 0, rh = 0; const char* rs = getenv("MGR_RENDER_SIZE");
                if (rs && sscanf(rs, "%ux%u", &rw, &rh) == 2 && rw && rh) { setD3D9RenderSize(rw, rh); __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "render size %ux%u", rw, rh); }
                host.sized = true; setGuestDisplaySize(unsigned(ANativeWindow_getWidth(app->window)), unsigned(ANativeWindow_getHeight(app->window))); }
            if (!host.started) { host.started = true; __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "boot %s", exe.c_str()); worker.start(exe); }
            pilotLoad(files + "/mgr_pilot.txt");
            { std::lock_guard<std::recursive_mutex> l(hostInput);
              if (pilotTick()) publishPad();
              if (!javaInput) padTick(); }
            auto now = worker.status();
            if (now != status) { status = now; __android_log_print(ANDROID_LOG_INFO, "MGRLAB", "status: %s", status.c_str()); }
        } catch (const std::exception& e) {
            __android_log_print(ANDROID_LOG_ERROR, "MGRLAB", "%s", e.what());
        }
    }
    acknowledgeD3D9Surface(false); setD3D9HostWindow(nullptr);
    if (jni) app->activity->vm->DetachCurrentThread();
}
