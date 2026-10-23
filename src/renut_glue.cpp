// Renut (renderizador nativo de Direct3D do mgr-nx-deko) no SwordMan: a escolha no arranque e os hooks que o Renut
// partilha com a cola do MGR deles (app/mgr_src/mgr_me_hooks.cpp). Compilado so com SM_RENUT (cmake/renut.cmake).
//
// swordman_renderer = xenos (padrao): o Xenia emulado de sempre; todos os hooks do Renut chamam a funcao original.
// swordman_renderer = renut: o Direct3D do jogo corre no Renut (plume/Vulkan), sem anel PM4 nem EDRAM.

#include <atomic>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/ppc.h>

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

REX_EXTERN(__imp__sub_826B3510);
REX_HOOK_RAW(sub_826B3510) {  // D3DDevice_Swap
  if (banjo::renut_gpu::Active()) {
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
