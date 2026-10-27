// Zandatsu - a GPU reinicia (hang ou falha) sem o jogo fechar (27/10).
//
// No RP5 com o Turnip MrPurple T19 o kgsl recupera de um hang ("GPU hang detected") e o submit seguinte do driver
// cai com SIGSEGV em kgsl_queue_submit (3 vezes em 2 regressoes em 27/10). Depois de um hang o contexto do kgsl fica
// invalido: o jogo nao pode continuar a desenhar. Aqui:
//   1. Um vigia le /sys/class/kgsl/kgsl-3d0/snapshot/faultcount (legivel pelo app; sobe a cada hang/falha que o kgsl
//      apanha) a cada 100 ms; o Vulkan (VK_ERROR_DEVICE_LOST no plume e a sonda do Renut) tambem avisa.
//   2. A partir dai o Renut nao submete mais nada (zandatsu_gpu_gate no plume, renut/0026: a thread fica parada antes
//      do vkQueueSubmit/vkQueuePresentKHR), o motivo vai para o log e para logs/gpu_reset.txt.
//   3. O app (GameActivity, nativeGpuReset) volta ao launcher, que mostra "A GPU reiniciou" com o botao Jogar.
// Se o driver cair mesmo assim (SIGSEGV/SIGBUS com o pc numa biblioteca do driver), o tratador escreve o mesmo
// logs/gpu_reset.txt antes do crash seguir e o launcher mostra o mesmo aviso em vez de "o jogo fechou".
//
// Teste: swordman_test_gpu_reset = N (segundos depois do primeiro frame; 0 = desligado) faz como se o faultcount
// subisse.

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/logging/api.h>
#include <rex/main_android.h>

#include "gpu_reset.h"

REXCVAR_DEFINE_INT32(swordman_test_gpu_reset, 0, "SwordMan",
                     "Teste: N segundos depois do primeiro frame faz como se a GPU tivesse travado (o jogo para de "
                     "submeter e volta ao launcher com o aviso \"A GPU reiniciou\"); 0 = desligado")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

extern std::atomic<uint64_t> g_swordman_ticks;

namespace {

constexpr const char* kFaultCount = "/sys/class/kgsl/kgsl-3d0/snapshot/faultcount";

std::atomic<bool> g_stopped{false};
char g_reason[256] = {};
char g_marker[512] = {};  // logs/gpu_reset.txt (caminho pronto para o tratador de sinais)
struct sigaction g_old_segv = {}, g_old_bus = {};

int64_t NowMs() {
  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  return int64_t(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

// So chamadas seguras dentro de um tratador de sinais: open/write/close e numeros feitos a mao.
void WriteMarker(const char* reason) {
  if (!g_marker[0]) return;
  const int fd = open(g_marker, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return;
  char num[24];
  int n = 0;
  for (int64_t v = NowMs(); v > 0 && n < 23; v /= 10) num[n++] = char('0' + v % 10);
  char line[24];
  for (int i = 0; i < n; ++i) line[i] = num[n - 1 - i];
  line[n] = '\n';
  (void)!write(fd, line, size_t(n + 1));
  (void)!write(fd, reason, strlen(reason));
  (void)!write(fd, "\n", 1);
  close(fd);
}

int ReadFaultCount() {
  FILE* f = std::fopen(kFaultCount, "r");
  if (!f) return -1;
  int v = -1;
  if (std::fscanf(f, "%d", &v) != 1) v = -1;
  std::fclose(f);
  return v;
}

bool IsDriverLibrary(const char* path) {
  return path && (std::strstr(path, "vulkan") || std::strstr(path, "freedreno") || std::strstr(path, "purple") ||
                  std::strstr(path, "adreno") || std::strstr(path, "turnip"));
}

// As faixas de codigo das bibliotecas do driver (lidas uma vez de /proc/self/maps): o tratador so compara o pc.
struct Range {
  uintptr_t begin = 0, end = 0;
  char name[64] = {};
};
Range g_driver[8];
int g_driver_count = 0;

void ReadDriverRanges() {
  FILE* f = std::fopen("/proc/self/maps", "r");
  if (!f) return;
  char line[512];
  while (g_driver_count < 8 && std::fgets(line, sizeof(line), f)) {
    unsigned long begin = 0, end = 0;
    char perms[8] = {}, path[400] = {};
    if (std::sscanf(line, "%lx-%lx %7s %*s %*s %*s %399s", &begin, &end, perms, path) < 4) continue;
    if (perms[2] != 'x' || !IsDriverLibrary(path) || std::strstr(path, "libvulkan.so")) continue;
    Range& r = g_driver[g_driver_count++];
    r.begin = begin;
    r.end = end;
    const char* base = std::strrchr(path, '/');
    std::snprintf(r.name, sizeof(r.name), "%s", base ? base + 1 : path);
  }
  std::fclose(f);
}

void OnFatalSignal(int sig, siginfo_t* info, void* context) {
  const auto* uc = static_cast<const ucontext_t*>(context);
  const uintptr_t pc = uc ? uintptr_t(uc->uc_mcontext.pc) : 0;
  for (int i = 0; i < g_driver_count; ++i) {
    if (pc < g_driver[i].begin || pc >= g_driver[i].end) continue;
    char reason[160] = "o driver Vulkan caiu (";
    std::strcat(reason, sig == SIGBUS ? "SIGBUS em " : "SIGSEGV em ");
    std::strcat(reason, g_driver[i].name);
    std::strcat(reason, ")");
    WriteMarker(reason);
    break;
  }
  // Segue para o tratador anterior (o do runtime, que trata as falhas de memoria do guest, ou o do Android).
  const struct sigaction& old = sig == SIGBUS ? g_old_bus : g_old_segv;
  if (old.sa_flags & SA_SIGINFO) {
    if (old.sa_sigaction) old.sa_sigaction(sig, info, context);
    return;
  }
  if (old.sa_handler == SIG_IGN) return;
  if (old.sa_handler == SIG_DFL || !old.sa_handler) {
    signal(sig, SIG_DFL);
    raise(sig);
    return;
  }
  old.sa_handler(sig);
}

void InstallSignalNet() {
  struct sigaction sa = {};
  sa.sa_sigaction = OnFatalSignal;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, &g_old_segv);
  sigaction(SIGBUS, &sa, &g_old_bus);
}

}  // namespace

namespace swordman::gpu_reset {

void Trigger(const char* why) {
  if (g_stopped.exchange(true)) return;
  std::snprintf(g_reason, sizeof(g_reason), "%s", why);
  REXLOG_ERROR("GPU: {}; o jogo para de desenhar e volta ao launcher (\"A GPU reiniciou\")", why);
  WriteMarker(g_reason);
  rex::FlushLogging();
}

bool Stopped() { return g_stopped.load(std::memory_order_acquire); }

// O vigia: chamado ao carregar a libgame.so (android_hooks.cpp); espera pelo primeiro frame (o tratador de sinais
// fica por cima do do runtime, ja instalado).
void Start() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::thread([] {
      while (g_swordman_ticks.load(std::memory_order_relaxed) == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      std::snprintf(g_marker, sizeof(g_marker), "%s",
                    (rex::GetAndroidAppDirectory() / "logs" / "gpu_reset.txt").c_str());
      ReadDriverRanges();
      InstallSignalNet();
      const int base = ReadFaultCount();
      const int test_s = REXCVAR_GET(swordman_test_gpu_reset);
      const auto t0 = std::chrono::steady_clock::now();
      REXLOG_INFO("GPU: vigia de hang ligado (faultcount {}, {} faixas do driver{})", base, g_driver_count,
                  test_s > 0 ? ", teste em " + std::to_string(test_s) + " s" : std::string());
      if (base < 0 && test_s <= 0) return;  // sem kgsl (Mali, Xclipse): so o Vulkan e o tratador de sinais
      while (!Stopped()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const int now = base >= 0 ? ReadFaultCount() : -1;
        if (now > base) {
          Trigger(("o kgsl apanhou um hang/falha da GPU (faultcount " + std::to_string(base) + " -> " +
                   std::to_string(now) + ")").c_str());
        } else if (test_s > 0 && std::chrono::steady_clock::now() - t0 >= std::chrono::seconds(test_s)) {
          Trigger("teste (swordman_test_gpu_reset)");
        }
      }
    }).detach();
  });
}

}  // namespace swordman::gpu_reset

// plume (renut/0026): antes de cada vkQueueSubmit / vkQueuePresentKHR. Depois de um hang a thread fica aqui ate o
// app fechar o processo; se ninguem o fizer em 5 s, sai.
extern "C" void zandatsu_gpu_gate() {
  if (!swordman::gpu_reset::Stopped()) return;
  for (int i = 0; i < 50; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  REXLOG_ERROR("GPU: o app nao fechou o jogo em 5 s depois do reinicio da GPU; a sair");
  rex::FlushLogging();
  _exit(0);
}

// plume (renut/0026): VK_ERROR_DEVICE_LOST numa chamada do Vulkan.
extern "C" void zandatsu_gpu_lost(const char* call) {
  swordman::gpu_reset::Trigger((std::string("VK_ERROR_DEVICE_LOST em ") + call).c_str());
}

// JNI (NativeGameState.gpuReset): o motivo, ou "" enquanto a GPU estiver bem.
extern "C" const char* rex_swordman_gpu_reset_reason() {
  return swordman::gpu_reset::Stopped() ? g_reason : "";
}
