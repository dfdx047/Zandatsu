#!/usr/bin/env bash
# Gera o codigo recompilado (generated/default) a partir de assets/default.xex, com
# o rexglue do SDK do projeto (external/rexglue-sdk), compilado aqui para o PC numa
# pasta propria (external/build-host). Como o Dragon Burst faz com o Burst Limit.
#
# Confere o SHA-1 do default.xex (tools/versions.env): os enderecos de
# mgr_overrides.toml e dos hooks so valem para esse executavel.
#
# Precisa de: clang/clang++ (20+), cmake (3.25+), ninja, e os pacotes de
# desenvolvimento que o SDK pede no Linux (gtk3, x11-xcb e wayland-client).
#
# Uso (a partir da raiz do repositorio, depois de tools/setup-sdk.sh):
#   bash tools/codegen.sh            (gera de novo)
#   bash tools/codegen.sh --force    (gera mesmo com erros de validacao)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT/tools/versions.env"
SDK="$ROOT/external/rexglue-sdk"
BUILD="$ROOT/external/build-host"
XEX="$ROOT/assets/default.xex"

if [ ! -f "$SDK/.swordman-patches" ]; then
  echo "ERRO: SDK nao preparado. Corre primeiro: bash tools/setup-sdk.sh" >&2
  exit 1
fi
if [ ! -f "$XEX" ]; then
  echo "ERRO: falta assets/default.xex (extrai o jogo para assets/, ver README)." >&2
  exit 1
fi
SHA="$(sha1sum "$XEX" | cut -d' ' -f1)"
if [ "$SHA" != "$XEX_SHA1" ]; then
  echo "ERRO: default.xex com SHA-1 $SHA, esperado $XEX_SHA1." >&2
  echo "  O projeto usa o disco 'Metal Gear Rising - Revengeance (USA, Europe)'." >&2
  exit 1
fi

CC_BIN="${CC:-clang}"
CXX_BIN="${CXX:-clang++}"
# x86-64-v2: o SDK usa SSSE3/SSE4 nas trocas de bytes e o clang, sem -march,
# compila para x86-64 basico.
echo "== compilar o rexglue do SDK para o PC (so a primeira vez demora)"
cmake -S "$SDK" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$CC_BIN" -DCMAKE_CXX_COMPILER="$CXX_BIN" \
  -DCMAKE_C_FLAGS="-march=x86-64-v2" -DCMAKE_CXX_FLAGS="-march=x86-64-v2" \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_BUILD_TESTS=OFF \
  -DREXGLUE_OUTPUT_DIR="$BUILD/bin" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 > /dev/null
cmake --build "$BUILD" --target rexglue

# O SDK pode pô-lo em external/build-host/bin ou na pasta out/ dele (REXGLUE_OUTPUT_DIR).
REXGLUE="$(find "$BUILD" "$SDK/out" -type f -name rexglue -perm -u+x 2>/dev/null | head -n 1)"
if [ -z "$REXGLUE" ]; then
  echo "ERRO: o rexglue nao foi gerado (external/build-host ou external/rexglue-sdk/out)." >&2
  exit 1
fi

cd "$ROOT"
FORCE=()
[ "${1:-}" = "--force" ] && FORCE=(--force)
echo "== codegen ($REXGLUE)"
"$REXGLUE" "${FORCE[@]}" codegen mgr_manifest.toml 2>&1 | tee codegen.log
if [ ! -f "$ROOT/generated/default/sources.cmake" ]; then
  echo "ERRO: o codegen nao gerou generated/default/sources.cmake (ver codegen.log)." >&2
  exit 1
fi
COUNT="$(grep -c '\.cpp' "$ROOT/generated/default/sources.cmake" || true)"
echo
echo "Codigo recompilado pronto: $COUNT ficheiros em generated/default."
echo "Proximo passo: ./gradlew assembleRelease (ou Build no Android Studio)."
