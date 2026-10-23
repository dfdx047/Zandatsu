// Renut (renderizador nativo de Direct3D do mgr-nx-deko) no SwordMan: a escolha no arranque e os hooks que o Renut
// partilha com a cola do MGR deles (app/mgr_src/mgr_me_hooks.cpp). Compilado so com SM_RENUT (cmake/renut.cmake).
//
// swordman_renderer = xenos (padrao): o Xenia emulado de sempre; todos os hooks do Renut chamam a funcao original.
// swordman_renderer = renut: o Direct3D do jogo corre no Renut (plume/Vulkan), sem anel PM4 nem EDRAM.

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <dlfcn.h>

#include <adrenotools/driver.h>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/ppc.h>

#include <rex/logging.h>

#include "renut_engine/gpu/sa2_renut_yield.h"

REXCVAR_DEFINE_STRING(swordman_renderer, "xenos", "SwordMan",
                      "Renderizador, escolhido no arranque. xenos = GPU do Xbox emulada (Xenia, plugin xenos). "
                      "renut = Direct3D do jogo nativo no Renut do mgr-nx-deko (plume, Vulkan)")
    .allowed({"xenos", "renut"})
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mgr_renut_diag_original_draw, false, "MGR/Diagnostics",
                    "Renut: corre tambem o Draw original do jogo antes do do Renut (diagnostico)");

// Frames apresentados pelo jogo no modo Renut (o VdSwap do SDK nao e chamado: o Renut substitui o D3DDevice_Swap).
std::atomic<uint64_t> g_sa2_game_frames{0};

// SDK (patch 0100): pausa dos menus do app e contador do vigia (android_hooks.cpp). No modo Xenia o VdSwap chama-o;
// no Renut o VdSwap nunca corre, por isso e chamado aqui, no inicio do Swap, na mesma thread do jogo.
extern "C" void (*rex_vdswap_hook)(void);
void SwordManRenutShotOnSwap();  // abaixo

REX_EXTERN(__imp__sub_826B3510);
REX_HOOK_RAW(sub_826B3510) {  // D3DDevice_Swap
  if (banjo::renut_gpu::Active()) {
    if (rex_vdswap_hook) rex_vdswap_hook();
    SwordManRenutShotOnSwap();
    renut_native_sub_826B3510(ctx, base);
    g_sa2_game_frames.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  __imp__sub_826B3510(ctx, base);
}

REX_EXTERN(__imp__sub_826BCD40);
REX_HOOK_RAW(sub_826BCD40) {  // D3DDevice_DrawIndexedVertices
  if (banjo::renut_gpu::Active() && REXCVAR_GET(mgr_renut_diag_original_draw)) {
    const PPCContext arguments = ctx;
    __imp__sub_826BCD40(ctx, base);
    ctx = arguments;
  }
  SA2_RENUT_YIELD_SHARED(sub_826BCD40);
  __imp__sub_826BCD40(ctx, base);
}

REX_EXTERN(__imp__sub_826BC928);
REX_HOOK_RAW(sub_826BC928) {  // D3DDevice_DrawVertices
  if (banjo::renut_gpu::Active() && REXCVAR_GET(mgr_renut_diag_original_draw)) {
    const PPCContext arguments = ctx;
    __imp__sub_826BC928(ctx, base);
    ctx = arguments;
  }
  SA2_RENUT_YIELD_SHARED(sub_826BC928);
  __imp__sub_826BC928(ctx, base);
}

// ---------------------------------------------------------------------------------------------------------------
// O que o Renut le da cola do SA2 deles (app/src/cpu/sa2_enhancements.cpp, me/sa2_render_mode.cpp, sa2_capture.cpp).
// Esses ficheiros trazem a maquinaria do SA2 (autoverificacao do codegen, contadores do SDK deles, captura de video);
// aqui so o que o renderizador chama, com os padroes deles.
// ---------------------------------------------------------------------------------------------------------------

#include "cpu/sa2_enhancements.h"
#include "me/sa2_render_mode.h"
#include "sa2_capture.h"

REXCVAR_DEFINE_INT32(sa2_render_height_handheld, 720, "SwordMan",
                     "Renut: altura de render (720 = os 1280x720 do jogo). Multiplos de 180 de 360 a 2160; a largura "
                     "segue a 16:9")
    .range(180, 4320)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace sa2::enh {
// sa2_cpu_enhancements / sa2_noncpu_enhancements, ligados por padrao no deles: cada melhoria do Renut segue a sua
// propria cvar.
bool CpuMaster() { return true; }
bool NonCpuMaster() { return true; }
}  // namespace sa2::enh

namespace sa2::render {
// Sem modo "docked" no Android: sempre a altura de sa2_render_height_handheld.
bool Docked() { return false; }
int32_t RenderHeightOf(bool) { return REXCVAR_GET(sa2_render_height_handheld); }
void OnPresented(uint64_t) {}
}  // namespace sa2::render

namespace sa2::capture {
// Sem a captura de video em lockstep do PC deles (a do SwordMan e a do AutoPilot).
bool RenutLockstepOn() { return false; }
void RenutLockstepFrame(uint64_t, const rex::ui::RawImage*) {}
}  // namespace sa2::capture

// ---------------------------------------------------------------------------------------------------------------
// Driver Vulkan do plume (patch renut/0006): o mesmo que o SDK carrega para o Xenia (vulkan_instance.cpp,
// Dbz3OpenCustomVulkanDriver): o GameActivity define REX_VULKAN_DRIVER_DIR/NAME (aba Drivers, Turnip) e
// REX_NATIVE_LIB_DIR; sem elas, ou se falhar, o plume usa o libvulkan do sistema.
// ---------------------------------------------------------------------------------------------------------------
extern "C" void* plume_android_vulkan_library() {
  const char* driver_dir = std::getenv("REX_VULKAN_DRIVER_DIR");
  const char* driver_name = std::getenv("REX_VULKAN_DRIVER_NAME");
  const char* hook_dir = std::getenv("REX_NATIVE_LIB_DIR");
  if (!driver_dir || !*driver_dir || !driver_name || !*driver_name || !hook_dir || !*hook_dir) {
    REXLOG_INFO("Renut: driver Vulkan do sistema");
    return nullptr;
  }
  std::string dir(driver_dir), hooks(hook_dir);
  if (dir.back() != '/') dir += '/';
  if (hooks.back() != '/') hooks += '/';
  void* handle = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, nullptr, hooks.c_str(), dir.c_str(),
                                            driver_name, nullptr, nullptr);
  if (handle) {
    REXLOG_INFO("Renut: driver Vulkan alternativo {}{}", dir, driver_name);
  } else {
    const char* error = dlerror();
    REXLOG_ERROR("Renut: o driver alternativo {}{} falhou a carregar ({}); a usar o do sistema", dir, driver_name,
                 error ? error : "sem detalhe");
  }
  return handle;
}

// ---------------------------------------------------------------------------------------------------------------
// Diagnostico: swordman_renut_shot_s = N grava, N segundos apos o primeiro Swap, a imagem que o Renut apresenta
// (renut::gpu::CaptureFrame: a mesma passagem de scan-out, para um alvo fora da swapchain) em logs/renut_shot.ppm.
// ---------------------------------------------------------------------------------------------------------------
#include <chrono>
#include <fstream>
#include <thread>

#include <rex/main_android.h>

#include "renut_engine/gpu/banjo_renut_capture.h"

REXCVAR_DEFINE_INT32(swordman_renut_shot_s, 0, "SwordMan",
                     "Renut (diagnostico): segundos apos o primeiro frame para gravar logs/renut_shot.ppm; 0 = nunca");

void SwordManRenutShotOnSwap() {
  static std::atomic<bool> started{false};
  const int32_t seconds = REXCVAR_GET(swordman_renut_shot_s);
  if (seconds <= 0 || started.exchange(true)) return;
  std::thread([seconds] {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    rex::ui::RawImage image;
    if (!renut::gpu::CaptureFrame(image, 2000)) {
      REXLOG_WARN("Renut: captura falhou (nenhum frame apresentado em 2 s)");
      return;
    }
    const auto path = rex::GetAndroidAppDirectory() / "logs" / "renut_shot.ppm";
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << image.width << " " << image.height << "\n255\n";
    uint64_t sum = 0;
    for (uint32_t y = 0; y < image.height; ++y) {
      const uint8_t* row = image.data.data() + y * image.stride;
      for (uint32_t x = 0; x < image.width; ++x) {
        out.write(reinterpret_cast<const char*>(row + x * 4), 3);
        sum += row[x * 4] + row[x * 4 + 1] + row[x * 4 + 2];
      }
    }
    REXLOG_INFO("Renut: captura {}x{} em {} (media {:.1f})", image.width, image.height, path.string(),
                double(sum) / (3.0 * image.width * image.height));
  }).detach();
}
