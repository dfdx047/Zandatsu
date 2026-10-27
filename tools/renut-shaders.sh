#!/usr/bin/env bash
# Shaders do Renut (renderizador nativo do mgr-nx-deko), gerados no PC a partir do jogo do utilizador:
#
#   assets/media/data000.cpk -> cpk_shader_scan (contentores Xenos) -> XenosRecomp (RENUT_RECOMP) + DXC Linux
#     -> out/renut/shader_cache.cpp           (os 1202 shaders do jogo em SPIR-V: dados do jogo, nunca no git)
#   shaders do proprio Renut (gpu/shaders/host/*.hlsl) -> DXC
#     -> out/renut/host_shaders/<nome>.spirv.h
#
# O cmake/renut.cmake compila estes ficheiros no libgame.so (encriptado no APK). As ferramentas (XenosRecomp, leitor do cpk) sao
# compiladas com o compilador do PC em out/host/build (tools/renut-host/CMakeLists.txt), nunca com o NDK. O DXC
# vem do release Linux oficial (fixo abaixo) para out/host/dxc.
#
# So refaz o trabalho quando muda alguma entrada (carimbo em out/renut/.stamp): o data000.cpk (tamanho e data),
# o commit do mgr-nx-deko, os patches de patches/renut, este script ou a versao do DXC.
#
# Uso (a partir da raiz do repositorio):
#   bash tools/renut-shaders.sh            # so se mudou alguma coisa
#   bash tools/renut-shaders.sh --force    # sempre
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

MGRNX="third_party/mgr-nx-deko"
XR="$MGRNX/app/thirdparty/reblue-XenosRecomp"
HOST_HLSL="$MGRNX/app/src/renut_engine/gpu/shaders/host"
CPK="assets/media/data000.cpk"
OUT="out/renut"
HOST="out/host"

DXC_TAG="v1.9.2609"
DXC_TGZ="linux_dxc_2026_09_28.x86_x64.tar.gz"
DXC_SHA256="96faadc7f5c282d2ffda49804beb4c3ee38127bc252b723234e3c5cdf7aa39a1"

if [ ! -f "$CPK" ]; then
  echo "ERRO: falta $CPK (o jogo, copiado do disco)." >&2
  exit 1
fi
if [ ! -f "$XR/XenosRecomp/main.cpp" ]; then
  echo "ERRO: submodulo $MGRNX vazio. Corre: git submodule update --init $MGRNX" >&2
  exit 1
fi

STAMP="$( {
  stat -c '%s %Y' "$CPK"
  git -C "$MGRNX" rev-parse HEAD
  cat patches/renut/*.patch 2>/dev/null || true
  cat "$0" tools/renut-host/CMakeLists.txt
} | sha1sum | cut -d' ' -f1)"
if [ "${1:-}" != "--force" ] && [ -f "$OUT/shader_cache.cpp" ] && [ "$(cat "$OUT/.stamp" 2>/dev/null)" = "$STAMP" ]; then
  echo "shaders do Renut: nada mudou, saltado"
  exit 0
fi

# DXC (release Linux oficial, fixo).
if [ ! -x "$HOST/dxc/bin/dxc" ]; then
  echo "DXC $DXC_TAG -> $HOST/dxc"
  mkdir -p "$HOST/dxc-dl" "$HOST/dxc"
  if [ ! -f "$HOST/dxc-dl/$DXC_TGZ" ]; then
    curl -fL -o "$HOST/dxc-dl/$DXC_TGZ" \
      "https://github.com/microsoft/DirectXShaderCompiler/releases/download/$DXC_TAG/$DXC_TGZ"
  fi
  echo "$DXC_SHA256  $HOST/dxc-dl/$DXC_TGZ" | sha256sum -c --quiet
  tar xzf "$HOST/dxc-dl/$DXC_TGZ" -C "$HOST/dxc"
fi
DXC="$ROOT/$HOST/dxc/bin/dxc"

# Ferramentas do PC.
GEN=()
command -v ninja > /dev/null && GEN=(-G Ninja)
CC_HOST="$(command -v clang || command -v gcc)"
CXX_HOST="$(command -v clang++ || command -v g++)"
cmake -S tools/renut-host -B "$HOST/build" "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$CC_HOST" -DCMAKE_CXX_COMPILER="$CXX_HOST" -DDXC_ROOT="$ROOT/$HOST/dxc" \
  -Wno-dev > "$HOST/configure.log" 2>&1 || { cat "$HOST/configure.log"; exit 1; }
cmake --build "$HOST/build" --target XenosRecomp cpk_shader_scan > "$HOST/build.log" 2>&1 \
  || { tail -40 "$HOST/build.log"; exit 1; }

# 1. Contentores do data000.cpk.
rm -rf "$OUT/containers"
python3 "$MGRNX/tools/extract_mgr_cpk_shaders.py" assets --out "$OUT/containers" \
  --scanner "$HOST/build/cpk_shader_scan" | tail -1

# 2. Traducao (sem a passagem de early alpha, por agora).
TMP="$OUT/shader_cache.cpp.tmp"
if ! "$HOST/build/xr/XenosRecomp" "$OUT/containers" "$TMP" "$XR/XenosRecomp/shader_common.h" \
     > "$OUT/xenosrecomp.log" 2>&1; then
  grep -v '^Recompiling shaders' "$OUT/xenosrecomp.log" | tail -40
  exit 1
fi
grep -v '^Recompiling shaders' "$OUT/xenosrecomp.log" | sed 's/^/  /'
mv "$TMP" "$OUT/shader_cache.cpp"
echo "  $(grep -o 'g_shaderCacheEntryCount = [0-9]*' "$OUT/shader_cache.cpp")"

# 3. Shaders do Renut (como o renut_host_shader do gpu.cmake deles: SPIR-V, layout D3D, Y invertido nos VS).
mkdir -p "$OUT/host_shaders"
for SP in overlay_vs:vs_6_0 overlay_ps:ps_6_0 present_vs:vs_6_0 present_ps:ps_6_0 \
          depth_resolve_ps:ps_6_0 depth_resolve_ms_ps:ps_6_0 texture_mip_ps:ps_6_0 resolve_copy_ps:ps_6_0 \
          rectangle_list_gs:gs_6_0 point_sprite_gs:gs_6_0 stream_convert_cs:cs_6_0 target_readback_cs:cs_6_0 bc_decode_cs:cs_6_0 \
          present_easu_ps:ps_6_0 present_rcas_ps:ps_6_0 present_cas_ps:ps_6_0 frame_interp_ps:ps_6_0; do
  S="${SP%%:*}"; P="${SP##*:}"
  EXTRA=()
  HV=2021
  [[ "$P" == vs* ]] && EXTRA=(-fvk-invert-y)
  # FSR 1 e CAS (renut/0019): os headers da AMD (third_party/fidelityfx, MIT) sao de antes do HLSL 2021 (ternario
  # com vetores).
  [[ "$S" == present_easu_ps || "$S" == present_rcas_ps || "$S" == present_cas_ps ]] &&
    HV=2018 && EXTRA+=(-I "$ROOT/third_party/fidelityfx")
  "$DXC" -T "$P" -E main -HV "$HV" -all-resources-bound -Wno-ignored-attributes -spirv -fvk-use-dx-layout \
    "${EXTRA[@]}" -Fh "$OUT/host_shaders/$S.spirv.h" -Vn "g_${S}_spirv" "$HOST_HLSL/$S.hlsl"
done
echo "  $(ls "$OUT/host_shaders" | wc -l) shaders do host"

echo "$STAMP" > "$OUT/.stamp"
echo "shaders do Renut prontos em $OUT"
