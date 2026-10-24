#!/usr/bin/env bash
# Compila tools/xex-image (PC) em out/host/xex-image, se mudou.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SDK="$ROOT/external/rexglue-sdk/thirdparty"
OUT="$ROOT/out/host/xex-image"
SRC=("$ROOT/tools/xex-image/main.cpp" "$ROOT/src/xex_image.cpp" "$ROOT/src/xex_image.h")
if [ -x "$OUT" ] && [ -z "$(find "${SRC[@]}" -newer "$OUT")" ]; then exit 0; fi
mkdir -p "$(dirname "$OUT")"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
cc -O2 -w -c "$SDK/libmspack/cabextract/mspack/lzxd.c" -I"$SDK/libmspack/cabextract/mspack" -o "$T/lzxd.o"
c++ -O2 -std=c++20 -w "$ROOT/tools/xex-image/main.cpp" "$ROOT/src/xex_image.cpp" "$T/lzxd.o" \
  -I"$ROOT/src" -I"$SDK" -I"$SDK/libmspack/cabextract/mspack" -o "$OUT"
