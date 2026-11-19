#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build_dir="${NFS_RUNTIME_BUILD:-$HOME/.cache/nfsu2-runtime-check}"
game_exe="${1:-$repo/../Need for Speed Underground 2/SPEED2.EXE}"
cmake -S "$repo/tests/android_runtime" -B "$build_dir" -G Ninja \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DNFS_D3D9_NATIVE_LIBRARY="${NFS_RUNTIME_D3D9_LIBRARY:-}"
cmake --build "$build_dir" -j2
"$build_dir/runtime_check" "$game_exe"
