#!/usr/bin/env bash
# Builds DXVK 2.7.1 (D3D9) for Android arm64 into build/lab/pc/dxvk2-android/build/libdxvk_android.so.
# Needs the v2.7.1 checkout with submodules in build/lab/pc/dxvk2, glslangValidator and the NDK.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
PORT="$ROOT/pc/ports/dxvk2_android"
OUT="$ROOT/build/lab/pc/dxvk2-android"
NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/29.0.14206865}"
export TMPDIR="$ROOT/build/tmp"
mkdir -p "$TMPDIR" "$OUT"
python3 "$PORT/prepare.py" "$ROOT/build/lab/pc/dxvk2" "$OUT/source"
python3 "$PORT/shaders.py" "$OUT/source"
cmake -S "$PORT" -B "$OUT/build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DCMAKE_BUILD_TYPE=Release -DDXVK2_SOURCE="$OUT/source"
cmake --build "$OUT/build" -j"${JOBS:-5}"
ls -la "$OUT/build/libdxvk_android.so"
