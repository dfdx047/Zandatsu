#!/usr/bin/env bash
# Prepara external/rexglue-sdk-0.10: o ReXGlue SDK v0.10.0 com os submodulos que o
# build Android usa e o overlay de sdk/overlay. Depois disto: tools/apply-sdk-patches.sh.
#
# Se ja existir um clone do SDK na v0.10.0 com os submodulos (por omissao
# ../rexglue-sdk, o que compilou o rexglue do PC), copia-o em vez de baixar tudo
# outra vez. Outra pasta: REXSDK_SRC=/caminho bash tools/setup-sdk.sh
#
# Idempotente: com o SDK ja preparado, so confere a versao.
#
# Uso (a partir da raiz do repositorio):
#   bash tools/setup-sdk.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT/tools/versions.env"
SDK="$ROOT/external/rexglue-sdk-0.10"
SRC="${REXSDK_SRC:-$ROOT/../rexglue-sdk}"

# Submodulos do SDK que o build Android usa (moltenvk, tracy, catch2, dxc e
# renderdoc ficam de fora: macOS, profiler, testes e Windows).
SUBMODULES=(
  libmspack glslang FFmpeg tomlplusplus simde xxHash spdlog fmt utfcpp
  vulkan-headers vulkan-memory-allocator imgui spirv-tools spirv-headers cli11
  o1heap sdl3 inja vulkan-loader
)

mkdir -p "$ROOT/external"

if [ -f "$SDK/.swordman-overlay" ]; then
  echo "external/rexglue-sdk-0.10 ja preparado (overlay aplicado)."
  echo "Proximo passo: bash tools/apply-sdk-patches.sh"
  exit 0
fi

if [ ! -d "$SDK/src" ]; then
  if [ -d "$SRC/.git" ] && [ "$(git -C "$SRC" rev-parse HEAD)" = "$SDK_COMMIT" ] \
     && [ -f "$SRC/thirdparty/sdl3/CMakeLists.txt" ]; then
    echo "== copiar o SDK de $SRC (sem out/)"
    mkdir -p "$SDK"
    tar -C "$SRC" --exclude=./out --exclude=./.git -cf - . | tar -C "$SDK" -xf -
  else
    echo "== clonar o SDK $SDK_TAG"
    git clone --branch "$SDK_TAG" --depth 1 "$SDK_REPO" "$SDK"
    HEAD="$(git -C "$SDK" rev-parse HEAD)"
    if [ "$HEAD" != "$SDK_COMMIT" ]; then
      echo "ERRO: o SDK clonado esta em $HEAD, esperado $SDK_COMMIT." >&2
      exit 1
    fi
    for M in "${SUBMODULES[@]}"; do
      echo "   submodulo $M"
      git -C "$SDK" submodule update --init --recursive --depth 1 "thirdparty/$M"
    done
  fi
fi

if [ ! -f "$SDK/thirdparty/sdl3/CMakeLists.txt" ]; then
  echo "ERRO: faltam submodulos em $SDK (thirdparty/sdl3)." >&2
  echo "  Para recomecar: rm -rf external/rexglue-sdk-0.10 && bash tools/setup-sdk.sh" >&2
  exit 1
fi

echo "== overlay (sdk/overlay -> external/rexglue-sdk-0.10)"
cp -a "$ROOT/sdk/overlay/." "$SDK/"
echo "$OVERLAY_COMMIT" > "$SDK/.swordman-overlay"

echo
echo "SDK pronto. Proximo passo:"
echo "  bash tools/apply-sdk-patches.sh"
