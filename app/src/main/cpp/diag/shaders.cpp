// zdiag: tabela dos shaders SPIR-V embutidos (shaders/*.inc gerados com glslc -mfmt=c no build).
#include <stdexcept>

#include "zdiag.h"

namespace zdiag {
namespace {
const uint32_t k_fs_tri_vert[] =
#include "fs_tri.vert.inc"
;
const uint32_t k_solid_frag[] =
#include "solid.frag.inc"
;
const uint32_t k_pl_basic_vert[] =
#include "pl_basic.vert.inc"
;
const uint32_t k_pl_full_vert[] =
#include "pl_full.vert.inc"
;
const uint32_t k_pl_inst_vert[] =
#include "pl_inst.vert.inc"
;
const uint32_t k_pl_f0_frag[] =
#include "pl_f0.frag.inc"
;
const uint32_t k_pl_f1_frag[] =
#include "pl_f1.frag.inc"
;
const uint32_t k_pl_f2_frag[] =
#include "pl_f2.frag.inc"
;
const uint32_t k_pl_f3_frag[] =
#include "pl_f3.frag.inc"
;
const uint32_t k_pl_f4_frag[] =
#include "pl_f4.frag.inc"
;
const uint32_t k_pl_f5_frag[] =
#include "pl_f5.frag.inc"
;
const uint32_t k_pl_f6_frag[] =
#include "pl_f6.frag.inc"
;
const uint32_t k_pl_f7_frag[] =
#include "pl_f7.frag.inc"
;
const uint32_t k_workload_comp[] =
#include "workload.comp.inc"
;
const uint32_t k_reduce_comp[] =
#include "reduce.comp.inc"
;
const uint32_t k_bc_fetch_comp[] =
#include "bc_fetch.comp.inc"
;

const Spirv kShaders[] = {
    {"fs_tri.vert", k_fs_tri_vert, sizeof(k_fs_tri_vert)},
    {"solid.frag", k_solid_frag, sizeof(k_solid_frag)},
    {"pl_basic.vert", k_pl_basic_vert, sizeof(k_pl_basic_vert)},
    {"pl_full.vert", k_pl_full_vert, sizeof(k_pl_full_vert)},
    {"pl_inst.vert", k_pl_inst_vert, sizeof(k_pl_inst_vert)},
    {"pl_f0.frag", k_pl_f0_frag, sizeof(k_pl_f0_frag)},
    {"pl_f1.frag", k_pl_f1_frag, sizeof(k_pl_f1_frag)},
    {"pl_f2.frag", k_pl_f2_frag, sizeof(k_pl_f2_frag)},
    {"pl_f3.frag", k_pl_f3_frag, sizeof(k_pl_f3_frag)},
    {"pl_f4.frag", k_pl_f4_frag, sizeof(k_pl_f4_frag)},
    {"pl_f5.frag", k_pl_f5_frag, sizeof(k_pl_f5_frag)},
    {"pl_f6.frag", k_pl_f6_frag, sizeof(k_pl_f6_frag)},
    {"pl_f7.frag", k_pl_f7_frag, sizeof(k_pl_f7_frag)},
    {"workload.comp", k_workload_comp, sizeof(k_workload_comp)},
    {"reduce.comp", k_reduce_comp, sizeof(k_reduce_comp)},
    {"bc_fetch.comp", k_bc_fetch_comp, sizeof(k_bc_fetch_comp)},
};
}  // namespace

const Spirv& FindShader(const char* name) {
  for (const Spirv& s : kShaders)
    if (!strcmp(s.name, name)) return s;
  throw std::runtime_error(std::string("shader embutido inexistente: ") + name);
}

}  // namespace zdiag

#ifdef ZDIAG_HAS_BC_DECODE_CS
// O shader do Renut (third_party/mgr-nx-deko/.../bc_decode_cs.hlsl), o mesmo que o jogo usa.
#include "bc_decode_cs.spirv.h"
#endif

namespace zdiag {
const Spirv* BcDecodeShader() {
#ifdef ZDIAG_HAS_BC_DECODE_CS
  // O array e de bytes (sem alinhamento garantido): copia para palavras de 32 bits.
  static const std::vector<uint32_t> words = [] {
    std::vector<uint32_t> w((sizeof(g_bc_decode_cs_spirv) + 3) / 4);
    memcpy(w.data(), g_bc_decode_cs_spirv, sizeof(g_bc_decode_cs_spirv));
    return w;
  }();
  static const Spirv s{"bc_decode_cs", words.data(), sizeof(g_bc_decode_cs_spirv)};
  return &s;
#else
  return nullptr;
#endif
}
}  // namespace zdiag
