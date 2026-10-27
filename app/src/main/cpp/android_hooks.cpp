// Zandatsu - ganchos especificos do port Android.
//
// Pausa e contador de frames: o SDK chama rex_vdswap_hook no inicio de cada VdSwap
// (patch 0100), na thread do jogo que apresenta o frame. E o equivalente generico do
// BurstLimitCameraFrame do Dragon Burst: nao depende de enderecos do MGR. Com a pausa
// pedida (menus do app por cima do jogo, game_state_jni.cpp) essa thread fica parada
// aqui ate a pausa acabar; o resto do jogo espera por ela no frame seguinte. O
// contador alimenta o vigia de travamentos (watchdog.cpp).
//
// A fazer: no DBZ3 a espera do D3D do Xbox 360 por espaco no ring buffer da GPU
// girava a 100% de um nucleo (66% da thread do jogo no 8 Elite). No MGR a funcao
// equivalente tem outro endereco: encontra-la com "optxbox d3dmap --generated
// generated/default" e devolver-lhe uma pausa curta aqui (REX_HOOK_RAW), como no DBZ3.
// Atencao: um REX_HOOK_RAW de um sub_ que nao existe no codigo gerado do MGR da erro de
// link. Usar so enderecos do generated/default.

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "gpu_reset.h"

std::atomic<bool> g_swordman_pause{false};
std::mutex g_swordman_pause_mu;
std::condition_variable g_swordman_pause_cv;

// Frames apresentados pelo jogo: o vigia (watchdog.cpp) deteta travamentos por aqui.
extern std::atomic<uint64_t> g_swordman_ticks;

// SDK (patch 0100, xboxkrnl_video.cpp).
extern "C" void (*rex_vdswap_hook)(void);

void SwordManPausePoint() {
  g_swordman_ticks.fetch_add(1, std::memory_order_relaxed);
  if (!g_swordman_pause.load(std::memory_order_acquire)) {
    return;
  }
  std::unique_lock<std::mutex> lock(g_swordman_pause_mu);
  g_swordman_pause_cv.wait(lock, [] { return !g_swordman_pause.load(std::memory_order_acquire); });
}

namespace {
// Liga o gancho quando a libgame.so e carregada (antes de o jogo arrancar).
struct InstallHooks {
  InstallHooks() {
    rex_vdswap_hook = &SwordManPausePoint;
    swordman::gpu_reset::Start();  // gpu_reset.cpp: hang da GPU sem fechar o jogo
  }
} g_install_hooks;
}  // namespace
