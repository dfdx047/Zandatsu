#!/usr/bin/env bash
set -euo pipefail
port_dir="$(cd "$(dirname "$0")" && pwd)"
project_dir="$(cd "$port_dir/../.." && pwd)"
task_build="$HOME/.cache/nfsu2-dxvk-query"
cmake -S "$port_dir" -B "$task_build" -G Ninja -DDXVK_HOST_HEADLESS=ON \
  -DDXVK_SOURCE="$project_dir/build/dxvk-source" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build "$task_build" -j2
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export DXVK_LOG_LEVEL=warn
export DXVK_LOG_PATH=none
"$task_build/dxvk_query_check"
