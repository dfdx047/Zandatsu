// livediff (Linux x86-64 lab host only): run a lifted function on live game state, then run the ORIGINAL x86 bytes of
// the same function from a snapshot of the same state in Unicorn (callees included, all in x86), and compare.
//
// x86diff.py/insnfuzz.py test leaf functions and single instructions with made-up inputs; this tests any function
// (callees included) with the inputs the game really passes, e.g. during a fight. Wrapping is done at link time
// (-Wl,--wrap=sub_X, see pc/tools/livediff_wraps.py), so only calls from other translation units are seen.
//
//   MGR_LIVEDIFF_LOG=<file>     enables the tests; one line per test (OK / DIFF / SKIP), details for DIFF
//   MGR_LIVEDIFF_ARM=<file>     tests run only while this file exists (e.g. created when the fight starts)
//   MGR_LIVEDIFF_MAX=<n>        tests per function (default 4)
//   MGR_LIVEDIFF_UNICORN=<so>   libunicorn.so.2 (default: the lab venv's)
//   MGR_LIVEDIFF_LIST=<file>    entry hook (gen built with RECOMP_NATIVE_RANGE, CMake MGR_LIVEDIFF_ENTRY=ON): the
//                               functions to test, one sub_X per line. One target at a time is armed in
//                               g_native_lo/hi; a timer moves to the next every MGR_LIVEDIFF_STEP ms (default 150)
//                               and a tested target moves on at once. Catches every call, also from the same file.
//
// A test: snapshot registers, XMM, x87 and every readable guest page; call the lifted body; diff live memory against
// the snapshot (the lift's writes); load the snapshot into Unicorn (text executable, the rest read/write, a stub
// that loads the x87 stack and jumps to the function, the return address replaced by a sentinel); run to the
// sentinel. A run that leaves the image text (an import), faults or runs too long is SKIP. Otherwise registers, XMM,
// x87 and every byte either side wrote are compared; float words that differ by < 1e-5 relative count as "near".
extern "C" {
#include "recomp_types.h"
}
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <algorithm>

namespace {
typedef void uc_engine;
typedef size_t uc_hook;
struct uc_x86_mmr { uint16_t selector; uint64_t base; uint32_t limit; uint32_t flags; };
int (*p_uc_open)(int, int, uc_engine**);
int (*p_uc_close)(uc_engine*);
int (*p_uc_mem_map)(uc_engine*, uint64_t, size_t, uint32_t);
int (*p_uc_mem_map_ptr)(uc_engine*, uint64_t, size_t, uint32_t, void*);
int (*p_uc_mem_write)(uc_engine*, uint64_t, const void*, size_t);
int (*p_uc_mem_read)(uc_engine*, uint64_t, void*, size_t);
int (*p_uc_reg_write)(uc_engine*, int, const void*);
int (*p_uc_reg_read)(uc_engine*, int, void*);
int (*p_uc_emu_start)(uc_engine*, uint64_t, uint64_t, uint64_t, size_t);
int (*p_uc_hook_add)(uc_engine*, uc_hook*, int, void*, void*, uint64_t, uint64_t, ...);
const char* (*p_uc_strerror)(int);

enum { R_EAX = 19, R_ECX = 22, R_EDX = 24, R_EBX = 21, R_ESP = 30, R_EBP = 20, R_ESI = 29, R_EDI = 23, R_EIP = 26,
       R_EFLAGS = 25, R_XMM0 = 122, R_FPCW = 246, R_FPSW = 31, R_MXCSR = 249, R_FS = 32, R_GDTR = 243 };
const uint32_t TEXT0 = 0x00401000u, TEXT1 = 0x015FC000u;
const uint64_t GSIZE = uint64_t{1} << 32;

bool g_on = false, g_loaded = false;
FILE* g_log = nullptr;
std::string g_arm;
int g_max = 4;
std::atomic<bool> g_armed{false};
std::mutex g_mu;
std::map<uint32_t, int> g_count;
thread_local bool t_busy = false;
unsigned char* g_shadow = nullptr;
std::vector<std::pair<uint32_t, uint32_t>> g_ranges;   // [va, end)

void init() {
    static std::once_flag once;
    std::call_once(once, [] {
        const char* log = getenv("MGR_LIVEDIFF_LOG");
        if (!log) return;
        g_log = fopen(log, "w");
        if (!g_log) return;
        if (const char* a = getenv("MGR_LIVEDIFF_ARM")) g_arm = a;
        if (const char* m = getenv("MGR_LIVEDIFF_MAX")) g_max = atoi(m);
        const char* so = getenv("MGR_LIVEDIFF_UNICORN");
        std::string def = std::string(getenv("HOME") ? getenv("HOME") : "") +
            "/AndroidStudioProjects/ProjectSwordMan/build/lab/pc/venv/lib/python3.14/site-packages/unicorn/lib/libunicorn.so.2";
        void* h = dlopen(so ? so : def.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!h) { fprintf(g_log, "unicorn: %s\n", dlerror()); fflush(g_log); return; }
#define LD(n) p_##n = reinterpret_cast<decltype(p_##n)>(dlsym(h, #n)); if (!p_##n) { fprintf(g_log, "no %s\n", #n); return; }
        LD(uc_open) LD(uc_close) LD(uc_mem_map) LD(uc_mem_map_ptr) LD(uc_mem_write) LD(uc_mem_read) LD(uc_reg_write)
        LD(uc_reg_read) LD(uc_emu_start) LD(uc_hook_add) LD(uc_strerror)
#undef LD
        g_shadow = static_cast<unsigned char*>(mmap(nullptr, GSIZE, PROT_READ | PROT_WRITE,
                                                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
        if (g_shadow == MAP_FAILED) { fprintf(g_log, "shadow mmap failed\n"); return; }
        g_on = true;
        if (!g_arm.empty()) {
            std::thread([] { for (;;) { struct stat st; g_armed = stat(g_arm.c_str(), &st) == 0;
                                         std::this_thread::sleep_for(std::chrono::milliseconds(200)); } }).detach();
        } else g_armed = true;
        fprintf(g_log, "livediff on, max %d per function\n", g_max); fflush(g_log);
    });
}

// readable guest ranges from /proc/self/maps, split at the image text so text can be the only executable part
void scan_ranges() {
    g_ranges.clear();
    uintptr_t base = uintptr_t(g_mem_base);
    FILE* f = fopen("/proc/self/maps", "r");
    char line[512];
    while (f && fgets(line, sizeof line, f)) {
        unsigned long a, b; char perm[8];
        if (sscanf(line, "%lx-%lx %7s", &a, &b, perm) != 3) continue;
        if (perm[0] != 'r' || b <= base || a >= base + GSIZE) continue;
        uint64_t va = (a < base ? base : a) - base, end = (b > base + GSIZE ? base + GSIZE : b) - base;
        uint64_t cuts[2] = {TEXT0, TEXT1};
        for (uint64_t c : cuts) if (va < c && c < end) { g_ranges.push_back({uint32_t(va), uint32_t(c)}); va = c; }
        g_ranges.push_back({uint32_t(va), uint32_t(end == GSIZE ? 0xFFFFF000u : end)});
    }
    if (f) fclose(f);
}

struct Cpu { uint32_t r[8]; V128 x[8]; int depth; double st[8]; uint32_t mxcsr; uint16_t cw; };
const char* RN[8] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};

void grab(Cpu& c) {
    c.r[0] = g_eax; c.r[1] = g_ecx; c.r[2] = g_edx; c.r[3] = g_ebx; c.r[4] = g_esp; c.r[5] = g_ebp; c.r[6] = g_esi;
    c.r[7] = g_edi;
    for (int i = 0; i < 8; ++i) c.x[i] = g_xmm[i];
    c.depth = g_fp_top;
    for (int i = 0; i < 8; ++i) c.st[i] = i < c.depth ? g_st[i] : 0.0;
    c.mxcsr = g_mxcsr; c.cw = g_fpu_cw;
}

struct UcRun { std::vector<uint32_t> writes; bool overflow = false; uint64_t bad = 0; int badtype = 0; };
bool on_bad(uc_engine*, int type, uint64_t addr, int, int64_t, void* ud) {
    UcRun* u = static_cast<UcRun*>(ud); u->bad = addr; u->badtype = type; return false;
}
void on_write(uc_engine*, int, uint64_t addr, int size, int64_t, void* ud) {
    UcRun* u = static_cast<UcRun*>(ud);
    if (u->writes.size() > 4000000) { u->overflow = true; return; }
    for (int i = 0; i < size; ++i) u->writes.push_back(uint32_t(addr + i));
}

bool near(float a, float b) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (!std::isfinite(a) || !std::isfinite(b)) return a == b;
    return std::fabs(a - b) <= 1e-5f * std::fmax(std::fmax(std::fabs(a), std::fabs(b)), 1e-20f);
}

void desc(unsigned char* p, uint32_t base, uint8_t access) {
    uint32_t lim = 0xFFFFF;
    p[0] = lim & 0xFF; p[1] = (lim >> 8) & 0xFF; p[2] = base & 0xFF; p[3] = (base >> 8) & 0xFF;
    p[4] = (base >> 16) & 0xFF; p[5] = access; p[6] = ((lim >> 16) & 0xF) | 0xC0; p[7] = (base >> 24) & 0xFF;
}

void test(uint32_t va, void (*real)(void), int nth) {
    unsigned char* gm = reinterpret_cast<unsigned char*>(g_mem_base);
    Cpu in, lifted;
    grab(in);
    uint32_t fs = g_fs_base;
    scan_ranges();
    auto t0 = std::chrono::steady_clock::now();
    for (auto& r : g_ranges) memcpy(g_shadow + r.first, gm + r.first, r.second - r.first);
    real();
    grab(lifted);
    // the lift's writes: live vs snapshot
    std::vector<uint32_t> lw;
    bool lover = false;
    for (auto& r : g_ranges) {
        for (uint32_t p = r.first; p < r.second; p += 4096) {
            if (memcmp(g_shadow + p, gm + p, 4096) == 0) continue;
            for (uint32_t i = 0; i < 4096; ++i)
                if (g_shadow[p + i] != gm[p + i]) { if (lw.size() < 4000000) lw.push_back(p + i); else lover = true; }
        }
    }
    // Unicorn on the snapshot
    uint32_t stub = 0;
    for (uint32_t cand = 0x7FF00000u; cand < 0xF0000000u && !stub; cand += 0x01000000u) {
        bool clash = false;
        for (auto& r : g_ranges) if (r.first < cand + 0x10000 && cand < r.second) clash = true;
        if (!clash) stub = cand;
    }
    uc_engine* uc = nullptr;
    char verdict[64] = "OK";
    std::string detail;
    UcRun ur;
    if (p_uc_open(4, 4, &uc)) { fprintf(g_log, "uc_open failed\n"); return; }
    for (auto& r : g_ranges) {
        uint32_t prot = (r.first >= TEXT0 && r.second <= TEXT1) ? 1 | 4 : 1 | 2;
        int e = p_uc_mem_map_ptr(uc, r.first, size_t(r.second - r.first), prot, g_shadow + r.first);
        if (e) { fprintf(g_log, "map %08X-%08X: %s\n", r.first, r.second, p_uc_strerror(e)); }
    }
    p_uc_mem_map(uc, stub, 0x10000, 7);
    std::vector<unsigned char> code;
    auto a32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) code.push_back((v >> (8 * i)) & 0xFF); };
    const uint32_t STDATA = stub + 0x800, STOUT = stub + 0x900, RET = stub + 0x100, GDT = stub + 0x1000;
    p_uc_mem_write(uc, STDATA, in.st, sizeof in.st);
    for (int k = in.depth - 1; k >= 0; --k) { code.push_back(0xDD); code.push_back(0x05); a32(STDATA + 8 * k); }
    uint32_t jmp_at = stub + uint32_t(code.size());
    code.push_back(0xE9); a32(va - (jmp_at + 5));
    p_uc_mem_write(uc, stub, code.data(), code.size());
    unsigned char gdt[32] = {0};
    desc(gdt + 8, 0, 0x9B); desc(gdt + 16, 0, 0x93); desc(gdt + 24, fs, 0xF3);
    p_uc_mem_write(uc, GDT, gdt, sizeof gdt);
    uc_x86_mmr gdtr = {0, GDT, 31, 0};
    p_uc_reg_write(uc, R_GDTR, &gdtr);
    uint32_t fsel = 3 << 3; p_uc_reg_write(uc, R_FS, &fsel);
    int ids[8] = {R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI};
    for (int i = 0; i < 8; ++i) { uint32_t v = in.r[i]; p_uc_reg_write(uc, ids[i], &v); }
    uint32_t fl = 0x202; p_uc_reg_write(uc, R_EFLAGS, &fl);
    for (int i = 0; i < 8; ++i) p_uc_reg_write(uc, R_XMM0 + i, &in.x[i]);
    uint32_t mx = in.mxcsr; p_uc_reg_write(uc, R_MXCSR, &mx);
    uint32_t cw = in.cw; p_uc_reg_write(uc, R_FPCW, &cw);
    uint32_t retslot; memcpy(&retslot, g_shadow + in.r[4], 4);
    memcpy(g_shadow + in.r[4], &RET, 4);       // the lift's call pushed a dummy; x86 returns to the sentinel
    uc_hook hw;
    p_uc_hook_add(uc, &hw, 2048, reinterpret_cast<void*>(&on_write), &ur, 1, 0);
    uc_hook hb;
    p_uc_hook_add(uc, &hb, 16 | 32 | 64 | 128 | 256 | 512, reinterpret_cast<void*>(&on_bad), &ur, 1, 0);
    int e = p_uc_emu_start(uc, stub, RET, 3000000, 200000000);
    uint32_t eip = 0; p_uc_reg_read(uc, R_EIP, &eip);
    memcpy(g_shadow + in.r[4], &retslot, 4);
    if (e || eip != RET) {
        snprintf(verdict, sizeof verdict, "SKIP eip %08X %s", eip, e ? p_uc_strerror(e) : "timeout");
        char buf[200];
        snprintf(buf, sizeof buf, " bad %08llX/%d in eax %08X ecx %08X edx %08X esp %08X fs %08X ranges %zu",
                 (unsigned long long)ur.bad, ur.badtype, in.r[0], in.r[1], in.r[2], in.r[4], fs, g_ranges.size());
        detail += buf;
    } else {
        Cpu x; uint32_t sw = 0; p_uc_reg_read(uc, R_FPSW, &sw);
        int depth = (8 - ((sw >> 11) & 7)) & 7;
        if (depth == 0 && lifted.depth == 8) depth = 8;
        for (int i = 0; i < 8; ++i) p_uc_reg_read(uc, ids[i], &x.r[i]);
        for (int i = 0; i < 8; ++i) p_uc_reg_read(uc, R_XMM0 + i, &x.x[i]);
        code.clear();
        for (int k = 0; k < depth; ++k) { code.push_back(0xDD); code.push_back(0x1D); a32(STOUT + 8 * k); }
        p_uc_mem_write(uc, RET, code.data(), code.size());
        if (!code.empty()) p_uc_emu_start(uc, RET, RET + code.size(), 1000000, 100);
        p_uc_mem_read(uc, STOUT, x.st, sizeof x.st);
        char buf[256];
        int nd = 0, nn = 0;
        for (int i = 0; i < 8; ++i) {
            if (i == 1 || i == 2) continue;     // ecx/edx: scratch, reported below only if eax-like use matters
            if (x.r[i] != lifted.r[i]) { snprintf(buf, sizeof buf, " %s %08X/%08X", RN[i], lifted.r[i], x.r[i]); detail += buf; ++nd; }
        }
        for (int i = 1; i <= 2; ++i)
            if (x.r[i] != lifted.r[i]) { snprintf(buf, sizeof buf, " (%s %08X/%08X)", RN[i], lifted.r[i], x.r[i]); detail += buf; }
        if (depth != lifted.depth) { snprintf(buf, sizeof buf, " x87depth %d/%d", lifted.depth, depth); detail += buf; ++nd; }
        for (int k = 0; k < depth && k < lifted.depth; ++k)
            if (!(x.st[k] == lifted.st[k] || (std::isnan(x.st[k]) && std::isnan(lifted.st[k])))) {
                bool nr = near(float(x.st[k]), float(lifted.st[k]));
                snprintf(buf, sizeof buf, " st%d %.9g/%.9g%s", k, lifted.st[k], x.st[k], nr ? "~" : ""); detail += buf;
                nr ? ++nn : ++nd;
            }
        for (int i = 0; i < 8; ++i)
            if (memcmp(&x.x[i], &lifted.x[i], 16)) {
                bool nr = true;
                for (int l = 0; l < 4; ++l) if (x.x[i].u32[l] != lifted.x[i].u32[l] && !near(x.x[i].f32[l], lifted.x[i].f32[l])) nr = false;
                snprintf(buf, sizeof buf, " xmm%d %08X.%08X.%08X.%08X/%08X.%08X.%08X.%08X%s", i, lifted.x[i].u32[3], lifted.x[i].u32[2],
                         lifted.x[i].u32[1], lifted.x[i].u32[0], x.x[i].u32[3], x.x[i].u32[2], x.x[i].u32[1], x.x[i].u32[0], nr ? "~" : "");
                detail += buf; nr ? ++nn : ++nd;
            }
        // memory: every dword either side wrote
        std::unordered_set<uint32_t> words;
        for (uint32_t a : lw) words.insert(a & ~3u);
        for (uint32_t a : ur.writes) words.insert(a & ~3u);
        std::vector<uint32_t> ws(words.begin(), words.end());
        std::sort(ws.begin(), ws.end());
        int shown = 0, md = 0, mn = 0;
        for (uint32_t w : ws) {
            if (w >= in.r[4] && w < in.r[4] + 4) continue;   // the return slot
            bool mapped = false;
            for (auto& r : g_ranges) if (w >= r.first && w + 4 <= r.second) mapped = true;
            if (!mapped) continue;
            uint32_t lv, xv; memcpy(&lv, gm + w, 4); memcpy(&xv, g_shadow + w, 4);
            if (lv == xv) continue;
            float lf, xf; memcpy(&lf, &lv, 4); memcpy(&xf, &xv, 4);
            bool nr = near(lf, xf);
            nr ? ++mn : ++md;
            if (shown < 12 && (!nr || shown < 4)) {
                snprintf(buf, sizeof buf, " [%08X] %08X=%g/%08X=%g%s", w, lv, lf, xv, xf, nr ? "~" : "");
                detail += buf; ++shown;
            }
        }
        if (md) { snprintf(buf, sizeof buf, " mem %d words differ, %d near", md, mn); detail = buf + detail; }
        else if (mn) { snprintf(buf, sizeof buf, " mem %d near", mn); detail = buf + detail; }
        if (lover || ur.overflow) detail += " (write list overflow)";
        snprintf(verdict, sizeof verdict, "%s", nd || md ? "DIFF" : (nn || mn ? "NEAR" : "OK"));
        snprintf(buf, sizeof buf, " lw %zu uw %zu", lw.size(), ur.writes.size()); detail += buf;
    }
    p_uc_close(uc);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    fprintf(g_log, "%s sub_%08X #%d %.0f ms depth %d%s\n", verdict, va, nth, ms, in.depth, detail.c_str());
    fflush(g_log);
}
}  // namespace


// ---- MGR_PROF=<file>: CPU sampler (ITIMER_PROF, 1 kHz) while armed; each sample = the interrupted thread's tid and
// up to 32 return addresses (backtrace), written as text "tid pc pc ..." relative to the executable's load base.
namespace {
const int PROF_N = 1 << 16;
struct ProfSample { int tid; int n; void* pc[32]; };
ProfSample* g_prof = nullptr;
std::atomic<int> g_prof_w{0};
int g_prof_r = 0;
uintptr_t g_exe_base = 0;
void prof_handler(int, siginfo_t*, void* ctx) {
    if (!g_armed) return;
    int i = g_prof_w.fetch_add(1);
    if (i - g_prof_r >= PROF_N) return;
    ProfSample& s = g_prof[i % PROF_N];
    s.tid = int(syscall(SYS_gettid));
    s.n = backtrace(s.pc + 1, 31) + 1;
    s.pc[0] = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.gregs[REG_RIP]);
}
int exe_base_cb(dl_phdr_info* info, size_t, void*) { if (!g_exe_base) g_exe_base = info->dlpi_addr; return 1; }
struct ProfInit {
    ProfInit() {
        const char* out = getenv("MGR_PROF");
        if (!out) return;
        if (const char* a = getenv("MGR_LIVEDIFF_ARM")) g_arm = a;
        dl_iterate_phdr(exe_base_cb, nullptr);
        g_prof = static_cast<ProfSample*>(calloc(PROF_N, sizeof(ProfSample)));
        void* prime[4]; backtrace(prime, 4);
        std::string path = out;
        std::thread([path] {
            FILE* f = fopen(path.c_str(), "w");
            fprintf(f, "base %lx\n", (unsigned long)g_exe_base);
            for (;;) {
                struct stat st; g_armed = g_arm.empty() || stat(g_arm.c_str(), &st) == 0;
                int w = g_prof_w.load();
                if (w - g_prof_r > PROF_N) g_prof_r = w - PROF_N;
                for (; g_prof_r < w; ++g_prof_r) {
                    ProfSample& s = g_prof[g_prof_r % PROF_N];
                    fprintf(f, "%d", s.tid);
                    for (int k = 0; k < s.n && k < 32; ++k) fprintf(f, " %lx", (unsigned long)(uintptr_t(s.pc[k]) - g_exe_base));
                    fputc('\n', f);
                }
                fflush(f);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }).detach();
        struct sigaction sa; memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = prof_handler; sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigaction(SIGPROF, &sa, nullptr);
        itimerval it; it.it_interval.tv_sec = 0; it.it_interval.tv_usec = 1000; it.it_value = it.it_interval;
        setitimer(ITIMER_PROF, &it, nullptr);
    }
} g_prof_init;
}  // namespace

extern "C" void livediff_call(uint32_t va, void (*real)(void)) {
    init();
    if (!g_on || t_busy || !g_armed) { real(); return; }
    int nth;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        int& n = g_count[va];
        if (n >= g_max) { real(); return; }
        nth = ++n;
    }
    static std::atomic<bool> testing{false};
    if (testing.exchange(true)) { real(); return; }   // one test at a time (the shadow is shared)
    t_busy = true;
    struct Reset { ~Reset() { t_busy = false; testing = false; } } reset;
    test(va, real, nth);
}

// ---- entry hook: RECOMP_NATIVE_CHECK(va) at every lifted function's entry calls recomp_run_native(va) while va is in
// [g_native_lo, g_native_hi): one target at a time.
extern "C" {
uint32_t g_native_lo = 0, g_native_hi = 0;
void recomp_run_native(uint32_t va);
}
namespace {
std::vector<uint32_t> g_list;
std::atomic<size_t> g_cur{0};
std::atomic<int> g_in_native{0};
void arm_target() {
    if (g_list.empty() || g_in_native) return;
    uint32_t t = g_list[g_cur % g_list.size()];
    g_native_lo = t; g_native_hi = t + 1;
}
struct EntryInit {
    EntryInit() {
        const char* lp = getenv("MGR_LIVEDIFF_LIST");
        if (!lp || !getenv("MGR_LIVEDIFF_LOG")) return;
        FILE* f = fopen(lp, "r"); char line[128];
        while (f && fgets(line, sizeof line, f)) {
            unsigned v;
            if (sscanf(line, "sub_%x", &v) == 1 || sscanf(line, "%x", &v) == 1) g_list.push_back(v);
        }
        if (f) fclose(f);
        int step = getenv("MGR_LIVEDIFF_STEP") ? atoi(getenv("MGR_LIVEDIFF_STEP")) : 150;
        std::thread([step] {
            init();
            for (;;) {
                std::this_thread::sleep_for(std::chrono::milliseconds(step));
                if (!g_armed || !g_on) continue;
                // skip targets that used up their tests
                for (size_t k = 0; k < g_list.size(); ++k) {
                    size_t c = ++g_cur;
                    std::lock_guard<std::mutex> lk(g_mu);
                    if (g_count[g_list[c % g_list.size()]] < g_max) break;
                }
                arm_target();
            }
        }).detach();
    }
} g_entry_init;
}  // namespace

extern "C" void recomp_run_native(uint32_t va) {
    ++g_in_native;
    g_native_lo = g_native_hi = 0;
    recomp_func_t fn = recomp_lookup(va);
    struct Back { ~Back() { --g_in_native; arm_target(); } } back;
    if (!fn) { fprintf(stderr, "livediff: no lifted %08X\n", va); abort(); }
    init();
    livediff_call(va, fn);
    // a tested target moves on to the next one
    ++g_cur;
}
