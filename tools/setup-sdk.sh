#!/usr/bin/env bash
# Prepara external/rexglue-sdk: o SDK do Dragon Burst (fork do ReXGlue do Burst
# Limit, commit fixado em tools/versions.env) com os submodulos que o build usa, e
# aplica os patches de patches/ (tools/apply-sdk-patches.sh).
#
# Idempotente: com o SDK ja na versao certa, so confere os submodulos e aplica os
# patches novos. Com outra versao, para e diz o que fazer (nunca apaga trabalho).
#
# Uso (a partir da raiz do repositorio):
#   bash tools/setup-sdk.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT/tools/versions.env"
SDK="$ROOT/external/rexglue-sdk"

# Submodulos do SDK que o build usa (moltenvk, tracy e catch2 ficam de fora: macOS,
# profiler e testes).
SUBMODULES=(
  libmspack glslang FFmpeg tomlplusplus simde xxHash spdlog fmt utfcpp
  vulkan-headers vulkan-memory-allocator imgui spirv-tools spirv-headers cli11
  o1heap sdl3 inja vulkan-loader
)

mkdir -p "$ROOT/external"
if [ ! -d "$SDK/.git" ]; then
  echo "== clonar o SDK ($SDK_REPO)"
  git clone --filter=blob:none "$SDK_REPO" "$SDK"
fi

HEAD="$(git -C "$SDK" rev-parse HEAD)"
if [ "$HEAD" != "$SDK_COMMIT" ]; then
  if [ -f "$SDK/.swordman-patches" ] || [ -n "$(git -C "$SDK" status --porcelain --untracked-files=no)" ]; then
    echo "ERRO: external/rexglue-sdk esta em $HEAD (esperado $SDK_COMMIT) e tem alteracoes." >&2
    echo "  Para recomecar do zero: rm -rf external/rexglue-sdk && bash tools/setup-sdk.sh" >&2
    exit 1
  fi
  echo "== checkout $SDK_COMMIT"
  git -C "$SDK" fetch origin
  git -C "$SDK" -c advice.detachedHead=false checkout "$SDK_COMMIT"
fi

for M in "${SUBMODULES[@]}"; do
  if [ -e "$SDK/thirdparty/$M/.git" ]; then
    continue
  fi
  echo "   submodulo $M"
  git -C "$SDK" submodule update --init --recursive "thirdparty/$M"
done

bash "$ROOT/tools/apply-sdk-patches.sh"
