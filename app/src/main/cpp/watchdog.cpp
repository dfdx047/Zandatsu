// Project SwordMan - vigia de travamento ("watchdog").
//
// O jogo passa uma vez por frame apresentado em SwordManPausePoint() (android_hooks.cpp,
// chamado pelo VdSwap, patch 0100). Se esse contador parar 8 s sem uma pausa pedida (ou
// nao arrancar em 15 s), o jogo encravou: o vigia regista a pilha de TODAS as threads do
// processo em logs/watchdog.txt (vai no zip do "Exportar logs") e no logcat (tag swordman).
//
// Como: um sinal de tempo real a cada thread; o handler le pc/lr/fp do contexto
// interrompido e segue a cadeia de frame pointers (async-signal-safe, sem malloc). Os
// enderecos sao nomeados com dladdr: as funcoes recompiladas (sub_XXXXXXXX, o endereco
// no default.xex) e as do runtime sao exportadas pela libmain.so.

#include <android/log.h>
#include <cxxabi.h>
#include <dirent.h>
#include <dlfcn.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

extern std::atomic<bool> g_swordman_pause;
std::atomic<uint64_t> g_swordman_ticks{0};

namespace {

constexpr int kMaxFrames = 40;
constexpr int kStallMs = 8000;      // jogo sem apresentar frames este tempo = travamento (MGR: carregamentos longos)
constexpr int kStartupMs = 15000;   // primeiro frame tem de chegar ate aqui
constexpr int kMaxDumps = 3;

// Um pedido de cada vez (o vigia e a unica thread que envia o sinal).
struct Capture {
  std::atomic<pid_t> target{0};
  uintptr_t pcs[kMaxFrames];
  int count = 0;
  sem_t done;
};
Capture g_capture;
int g_signal = 0;

void Handler(int, siginfo_t*, void* raw) {
  const pid_t self = static_cast<pid_t>(syscall(SYS_gettid));
  if (g_capture.target.load(std::memory_order_acquire) != self) return;
  auto* uc = static_cast<ucontext_t*>(raw);
  int n = 0;
#if defined(__aarch64__)
  const uintptr_t pc = uc->uc_mcontext.pc;
  const uintptr_t lr = uc->uc_mcontext.regs[30];
  const uintptr_t sp = uc->uc_mcontext.sp;
  uintptr_t fp = uc->uc_mcontext.regs[29];
  g_capture.pcs[n++] = pc;
  g_capture.pcs[n++] = lr;
  // Cadeia de frame records do AArch64: [fp] = fp anterior, [fp+8] = retorno.
  uintptr_t prev = sp;
  while (n < kMaxFrames && fp && (fp & 0xF) == 0 && fp >= prev && fp - sp < (16u << 20)) {
    const uintptr_t next = reinterpret_cast<const uintptr_t*>(fp)[0];
    const uintptr_t ret = reinterpret_cast<const uintptr_t*>(fp)[1];
    if (!ret) break;
    g_capture.pcs[n++] = ret;
    prev = fp;
    if (next <= fp) break;
    fp = next;
  }
#elif defined(__x86_64__)
  // So para testar o vigia no PC (mesma logica, registos do x86-64).
  uintptr_t pc = uc->uc_mcontext.gregs[REG_RIP];
  uintptr_t sp = uc->uc_mcontext.gregs[REG_RSP];
  uintptr_t fp = uc->uc_mcontext.gregs[REG_RBP];
  g_capture.pcs[n++] = pc;
  uintptr_t prev = sp;
  while (n < kMaxFrames && fp && (fp & 0x7) == 0 && fp >= prev && fp - sp < (16u << 20)) {
    const uintptr_t next = reinterpret_cast<const uintptr_t*>(fp)[0];
    const uintptr_t ret = reinterpret_cast<const uintptr_t*>(fp)[1];
    if (!ret) break;
    g_capture.pcs[n++] = ret;
    prev = fp;
    if (next <= fp) break;
    fp = next;
  }
#else
  (void)uc;
#endif
  g_capture.count = n;
  g_capture.target.store(0, std::memory_order_release);
  sem_post(&g_capture.done);
}

std::string ReadSmall(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return {};
  char buf[512];
  const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
  std::fclose(f);
  buf[n] = 0;
  std::string s(buf);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
  return s;
}

std::string Describe(uintptr_t pc) {
  Dl_info info{};
  char line[512];
  if (dladdr(reinterpret_cast<void*>(pc), &info) && info.dli_fname) {
    const char* lib = std::strrchr(info.dli_fname, '/');
    lib = lib ? lib + 1 : info.dli_fname;
    const uintptr_t off = pc - reinterpret_cast<uintptr_t>(info.dli_fbase);
    if (info.dli_sname) {
      int status = 0;
      char* dem = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
      std::string name = (status == 0 && dem) ? dem : info.dli_sname;
      std::free(dem);
      if (name.size() > 160) name = name.substr(0, 160) + "...";
      std::snprintf(line, sizeof(line), "%s+0x%zx  %s+0x%zx", lib, size_t(off), name.c_str(),
                    size_t(pc - reinterpret_cast<uintptr_t>(info.dli_saddr)));
    } else {
      std::snprintf(line, sizeof(line), "%s+0x%zx", lib, size_t(off));
    }
  } else {
    std::snprintf(line, sizeof(line), "0x%zx", size_t(pc));
  }
  return line;
}

void Emit(FILE* out, const std::string& text) {
  __android_log_print(ANDROID_LOG_WARN, "swordman", "%s", text.c_str());
  if (out) std::fprintf(out, "%s\n", text.c_str());
}

void DumpAllThreads(const char* reason, uint64_t ticks) {
  static int dumps = 0;
  ++dumps;
  std::string path;
  if (const char* dir = std::getenv("REX_APP_DIR")) {
    path = std::string(dir) + "/logs/watchdog.txt";
  }
  FILE* out = path.empty() ? nullptr : std::fopen(path.c_str(), dumps == 1 ? "w" : "a");
  char head[256];
  std::snprintf(head, sizeof(head), "swordman: watchdog %s (ticks=%llu, despejo %d de %d)", reason,
                static_cast<unsigned long long>(ticks), dumps, kMaxDumps);
  Emit(out, head);

  const pid_t pid = getpid();
  const pid_t me = static_cast<pid_t>(syscall(SYS_gettid));
  DIR* dir = opendir("/proc/self/task");
  if (!dir) {
    Emit(out, "swordman: watchdog sem /proc/self/task");
    if (out) std::fclose(out);
    return;
  }
  while (dirent* e = readdir(dir)) {
    if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
    const pid_t tid = static_cast<pid_t>(std::atoi(e->d_name));
    if (tid == me) continue;
    const std::string base = std::string("/proc/self/task/") + e->d_name;
    const std::string comm = ReadSmall(base + "/comm");
    const std::string stat = ReadSmall(base + "/stat");
    // Estado: o caracter a seguir ao ")" do nome.
    char state = '?';
    if (const size_t p = stat.rfind(')'); p != std::string::npos && p + 2 < stat.size()) {
      state = stat[p + 2];
    }
    const std::string wchan = ReadSmall(base + "/wchan");
    char title[256];
    std::snprintf(title, sizeof(title), "swordman: watchdog -- thread %d \"%s\" estado=%c wchan=%s", tid,
                  comm.c_str(), state, wchan.empty() ? "-" : wchan.c_str());
    Emit(out, title);

    g_capture.count = 0;
    g_capture.target.store(tid, std::memory_order_release);
    if (syscall(SYS_tgkill, pid, tid, g_signal) != 0) {
      g_capture.target.store(0, std::memory_order_release);
      continue;
    }
    timespec until{};
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_nsec += 300 * 1000 * 1000;
    if (until.tv_nsec >= 1000000000) {
      until.tv_sec += 1;
      until.tv_nsec -= 1000000000;
    }
    if (sem_timedwait(&g_capture.done, &until) != 0) {
      g_capture.target.store(0, std::memory_order_release);
      Emit(out, "swordman: watchdog    (sem resposta ao sinal)");
      continue;
    }
    for (int i = 0; i < g_capture.count; ++i) {
      char line[600];
      std::snprintf(line, sizeof(line), "swordman: watchdog    #%02d %s", i,
                    Describe(g_capture.pcs[i]).c_str());
      Emit(out, line);
    }
  }
  closedir(dir);
  Emit(out, "swordman: watchdog fim");
  if (out) std::fclose(out);
}

void WatchLoop() {
  using clock = std::chrono::steady_clock;
  const auto start = clock::now();
  auto last_change = start;
  uint64_t last_ticks = 0;
  bool armed = true;
  int dumps = 0;
  bool first_tick_logged = false;
  __android_log_print(ANDROID_LOG_INFO, "swordman", "swordman: watchdog ativo (despeja apos %d s sem ticks)",
                      kStartupMs / 1000);
  while (dumps < kMaxDumps) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const uint64_t ticks = g_swordman_ticks.load(std::memory_order_relaxed);
    const auto now = clock::now();
    if (ticks && !first_tick_logged) {
      first_tick_logged = true;
      __android_log_print(
          ANDROID_LOG_INFO, "swordman", "swordman: watchdog primeiro tick do loop do jogo apos %lld ms",
          static_cast<long long>(
              std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count()));
    }
    if (ticks != last_ticks || g_swordman_pause.load(std::memory_order_relaxed)) {
      if (ticks != last_ticks) armed = true;  // voltou a andar: pode despejar outra vez
      last_ticks = ticks;
      last_change = now;
      continue;
    }
    const auto still_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change).count();
    const bool never = ticks == 0;
    if (armed && still_ms >= (never ? kStartupMs : kStallMs)) {
      DumpAllThreads(never ? "o loop do jogo nunca arrancou" : "o loop do jogo parou", ticks);
      armed = false;
      ++dumps;
    }
  }
}

struct Starter {
  Starter() {
    sem_init(&g_capture.done, 0, 0);
    g_signal = SIGRTMIN + 6;
    struct sigaction sa{};
    sa.sa_sigaction = Handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(g_signal, &sa, nullptr) != 0) return;
    std::thread(WatchLoop).detach();
  }
};
Starter g_starter;

}  // namespace
