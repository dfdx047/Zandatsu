// Widescreen do MGR no Renut (swordman_aspect = widescreen): o jogo cria o dispositivo D3D com a largura da tela
// (hor+: a altura fica 720, o campo de visao vertical o do jogo). A apresentacao (proporcao original / esticar) e do
// Renut (backend/present.cpp, patch renut/0022).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include "renut_engine/gpu/backend/native_window.h"
#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/guest/widescreen.h"

REXCVAR_DECLARE(std::string, swordman_aspect);  // Renut (backend/present.cpp)

namespace {

constexpr uint32_t kNativeWidth = 1280;
constexpr uint32_t kNativeHeight = 720;
constexpr double kMaxAspect = 32.0 / 9.0;

// A largura de 720 linhas na proporcao da tela, multipla de 16 (metades e quartos inteiros); 1280 se a tela nao
// for mais larga que 16:9.
uint32_t WidthForWindow() {
  uint32_t w = 0, h = 0;
  if (!renut::gpu::GetRenderWindowPixelSize(w, h) || !w || !h) {
    REXLOG_WARN("Widescreen: sem o tamanho da janela, fica 16:9");
    return kNativeWidth;
  }
  if (w < h) std::swap(w, h);  // a janela ainda em retrato no arranque
  const double aspect = std::min(double(w) / double(h), kMaxAspect);
  const uint32_t width = uint32_t(std::lround(kNativeHeight * aspect / 16.0)) * 16;
  REXLOG_INFO("Widescreen: janela {}x{}, jogo {}x{}", w, h, std::max(width, kNativeWidth), kNativeHeight);
  return std::max(width, kNativeWidth);
}

}  // namespace

// Cria o D3D device (Direct3D_CreateDevice dentro): r3 = {largura, altura, ...}, guardadas em 0x83733A40/44 e no
// BackBufferWidth/Height dos parametros de apresentacao.
REX_EXTERN(__imp__sub_82794540);
REX_HOOK_RAW(sub_82794540) {
  if (banjo::renut_gpu::Active() && REXCVAR_GET(swordman_aspect) == "widescreen") {
    auto* mode = renut::gpu::GuestPtr<be_u32>(ctx.r3.u32);
    const uint32_t w = mode ? mode[0].get() : 0, h = mode ? mode[1].get() : 0;
    if (w == kNativeWidth && h == kNativeHeight) {
      const uint32_t width = WidthForWindow();
      mode[0] = width;
      renut::gpu::SetGuestScreenWidth(width);
    } else {
      REXLOG_WARN("Widescreen: o jogo pediu {}x{}, fica como esta", w, h);
    }
  }
  __imp__sub_82794540(ctx, base);
}
