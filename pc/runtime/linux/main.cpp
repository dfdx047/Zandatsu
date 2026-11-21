// Linux x86-64 bench host: boots the lifted MGR:R exe on a 64 MB stack (like GuestBootWorker on Android) and drives the
// pad with a lab pilot, so the same generated code can be compared between x86-64 and arm64.
//   mgr_pc <exe>                     run until the guest exits (or MGR_SECONDS)
//   MGR_SECONDS=<s>                  stop the guest after s seconds (default: no limit; the 5 min bring-up budget is
//                                    lifted to s+60 s by the runtime when set)
//   MGR_PILOT=<file>                 polled every 50 ms; when it exists its steps are queued and the file is deleted
//                                    (same format as pc/android/mgr_android_main.cpp):
//     stick <x> <y> <ms> | rstick <x> <y> <ms> | btn <A|B|X|Y|LB|RB|LT|RT|BACK|START|LS|RS|UP|DOWN|LEFT|RIGHT>[+...] <ms>
//     | wait <ms>        (x/y in -1..1, y down positive)
//   MGR_RENDER_SIZE=<w>x<h>          host backbuffer size (as the Android launcher)
//   MGR_FPS_CAP=<n>                  guest frame limit (0 = the game's own)
#include "guest_runtime.h"
#include "d3d9_bridge.h"
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace {
struct PilotStep { std::string kind; float x = 0, y = 0; uint32_t buttons = 0; float lt = 0, rt = 0; int ms = 0, hx = 0, hy = 0; };

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

int pilotLoad(const std::string& path, std::deque<PilotStep>& steps) {
    std::ifstream f(path); if (!f) return 0;
    std::string line; int n = 0;
    while (std::getline(f, line)) {
        std::stringstream in(line); PilotStep st; in >> st.kind;
        if (st.kind == "stick" || st.kind == "rstick") in >> st.x >> st.y >> st.ms;
        else if (st.kind == "btn") { std::string b; in >> b >> st.ms; st.buttons = pilotButton(b, st.lt, st.rt, st.hx, st.hy); }
        else if (st.kind == "wait") in >> st.ms;
        else continue;
        steps.push_back(st); ++n;
    }
    f.close(); std::remove(path.c_str());
    return n;
}

void publish(const PilotStep& s) {
    float lx = 0, ly = 0, rx = 0, ry = 0;
    if (s.kind == "stick") { lx = s.x; ly = s.y; }
    if (s.kind == "rstick") { rx = s.x; ry = s.y; }
    const float axes[6] = {lx, ly, s.lt - s.rt, rx, ry, 0.f};
    setGuestPadConnected(true);
    setGuestPad(axes, s.buttons, s.hx, s.hy);
}

std::atomic<bool> guestDone{false};
std::string guestStatus;
void* guestMain(void* exe) { guestStatus = connectGuestRuntime(static_cast<const char*>(exe)); guestDone = true; return nullptr; }

double secondsSince(std::chrono::steady_clock::time_point t) { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count(); }
}

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: %s <game exe>\n", argv[0]); return 64; }
    setenv("DXVK_LOG_LEVEL", "warn", 0);
    unsigned rw = 0, rh = 0;
    if (const char* rs = getenv("MGR_RENDER_SIZE"); rs && sscanf(rs, "%ux%u", &rw, &rh) == 2 && rw && rh) setD3D9RenderSize(rw, rh);
    if (const char* cap = getenv("MGR_FPS_CAP")) setGuestFrameLimit(unsigned(atoi(cap)));
    // The game polls XInput only while it saw a pad at start (as on Android: report the pad before the boot).
    publish(PilotStep{});
    const double limit = getenv("MGR_SECONDS") ? atof(getenv("MGR_SECONDS")) : 0;
    const std::string pilotPath = getenv("MGR_PILOT") ? getenv("MGR_PILOT") : "";

    pthread_attr_t attributes; pthread_attr_init(&attributes); pthread_attr_setstacksize(&attributes, 64u << 20);
    pthread_t guest; if (pthread_create(&guest, &attributes, guestMain, argv[1])) { std::perror("pthread_create"); return 70; }
    pthread_attr_destroy(&attributes);

    const auto start = std::chrono::steady_clock::now();
    std::deque<PilotStep> steps; PilotStep now; bool active = false;
    auto stepEnd = start; bool stopSent = false;
    while (!guestDone.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!pilotPath.empty()) {
            static auto lastPoll = start;
            if (std::chrono::steady_clock::now() - lastPoll > std::chrono::milliseconds(50)) {
                lastPoll = std::chrono::steady_clock::now();
                if (int n = pilotLoad(pilotPath, steps)) std::fprintf(stderr, "[pilot %.1f s] %d steps loaded\n", secondsSince(start), n);
            }
        }
        auto t = std::chrono::steady_clock::now();
        if (!active || t >= stepEnd) {
            bool was = active; active = false; now = PilotStep{};
            if (!steps.empty()) {
                now = steps.front(); steps.pop_front(); active = true; stepEnd = t + std::chrono::milliseconds(now.ms);
                std::fprintf(stderr, "[pilot %.1f s] %s %g %g %d\n", secondsSince(start), now.kind.c_str(), now.x, now.y, now.ms);
            }
            if (was || active) publish(now);
        }
        if (limit > 0 && !stopSent && secondsSince(start) >= limit) {
            stopSent = true; std::fprintf(stderr, "[host] MGR_SECONDS=%g reached, stopping the guest\n", limit);
            requestGuestRuntimeStop();
        }
        if (stopSent && secondsSince(start) >= limit + 20) { std::fprintf(stderr, "[host] guest did not stop, exiting\n"); std::fflush(nullptr); _exit(0); }
    }
    pthread_join(guest, nullptr);
    std::printf("%s\n", guestStatus.c_str());
    std::fflush(nullptr);
    return 0;
}
