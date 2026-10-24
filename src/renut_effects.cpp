// Opções gráficas do MGR no Renut: ver renut_effects.h.

#include "renut_effects.h"

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(swordman_renut_shadows, true, "SwordMan",
                    "Renut: sombras do sol (mapa de sombra e passe de sombra em tela). false = sem sombras");
REXCVAR_DEFINE_BOOL(swordman_renut_bloom, true, "SwordMan",
                    "Renut: brilho (bloom, cadeia de 320x176 somada na imagem final). false = imagem original");

namespace swordman::renut_effects {
namespace {

// Poucos (2 cascatas por cena): procura linear.
constexpr int kMax = 8;
uint32_t g_depth[kMax] = {}, g_atlas[kMax] = {};
int g_depth_count = 0, g_atlas_count = 0;

bool Has(const uint32_t* list, int count, uint32_t va) {
  for (int i = 0; i < count; ++i)
    if (list[i] == va) return true;
  return false;
}

void Add(uint32_t* list, int& count, uint32_t va, const char* what) {
  if (!va || Has(list, count, va) || count == kMax) return;
  list[count++] = va;
  REXLOG_INFO("Renut: {} das sombras {:08X}", what, va);
}

}  // namespace

bool ShadowsOff() { return !REXCVAR_GET(swordman_renut_shadows); }
bool BloomOff() { return !REXCVAR_GET(swordman_renut_bloom); }

void NoteResolve(bool depth, uint32_t width, uint32_t height, uint32_t source_va, uint32_t dest_texture_va) {
  if (!depth || width != height || width < 512) return;
  Add(g_depth, g_depth_count, source_va, "alvo de profundidade");
  Add(g_atlas, g_atlas_count, dest_texture_va, "atlas");
}

bool IsShadowDepth(uint32_t depth_va) { return depth_va && Has(g_depth, g_depth_count, depth_va); }
bool IsShadowAtlas(uint32_t texture_va) { return texture_va && Has(g_atlas, g_atlas_count, texture_va); }

bool IsBloomShader(uint64_t ps_hash) {
  switch (ps_hash) {
    case 0x07A5E4820E9F50C0ull:  // redução (do ecrã para 320x176 e entre níveis)
    case 0x0AD31892DCFED92Dull:  // desfoque horizontal
    case 0x240A635A36C38D19ull:  // desfoque vertical
    case 0xF71DC5AC7E1A72E0ull:  // soma dos níveis
      return true;
    default:
      return false;
  }
}

}  // namespace swordman::renut_effects
