#!/usr/bin/env bash
# Uma resolucao do Renut posta a prova: 2 min de luta (tools/renut-fight.sh) e, se cair, a causa: o kernel (kgsl:
# hang/fault da GPU, lowmemorykiller/oom), o lmkd (logcat) e a memoria livre do aparelho a cada 5 s.
# Resultado em out/bench/res<altura>.txt (+ .log do jogo e fotos).
#
# Uso: bash tools/renut-res-test.sh <serial> <altura: 1080|1440|2160> [cvar = valor ...]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="$1"; H="$2"; shift 2
N="res$H"
OUT="$ROOT/out/bench"; mkdir -p "$OUT"
R="$OUT/$N.txt"
up0="$(adb -s "$D" shell cat /proc/uptime | awk '{print $1}')"
adb -s "$D" logcat -c -b all
# Memoria livre (MB) a cada 5 s, ate a luta acabar.
( while true; do
    echo "$(date +%H:%M:%S) $(adb -s "$D" shell "grep MemAvailable /proc/meminfo" | awk '{print int($2/1024)}') MB livres"
    sleep 5
  done ) > "$OUT/$N.mem" 2> /dev/null &
MEMPID=$!
trap 'kill $MEMPID 2> /dev/null || true' EXIT
SECS="${SECS:-120}" bash "$ROOT/tools/renut-fight.sh" "$D" "$N" "sa2_render_height_handheld = $H" "$@" > "$OUT/$N.fight" 2>&1 || true
kill $MEMPID 2> /dev/null || true
{
  echo "== $N ($(date +%H:%M))"
  tail -1 "$OUT/$N.fight"
  echo "-- memoria livre: min $(awk '{print $2}' "$OUT/$N.mem" | sort -n | head -1) MB, inicio $(head -1 "$OUT/$N.mem" | awk '{print $2}') MB"
  echo "-- jogo (device lost / erros Vulkan / memoria):"
  grep -a -i 'device lost\|VK_ERROR\|out of memory\|OUT_OF\|failed to alloc\|allocation failed' "$OUT/$N.log" | head -8 | cut -c1-240 || true
  echo "-- kernel desde o inicio (kgsl, lowmemorykiller, oom):"
  adb -s "$D" shell dmesg | awk -v t="$up0" '{ s=$0; gsub(/^\[ */,"",s); split(s,a,"]"); if (a[1]+0 >= t+0) print }' |
    grep -v 'avc:' | grep -i 'kgsl\|adreno\|gpu fault\|hang\|lowmem\|oom\|killed process\|lmk' | head -12 || true
  echo "-- lmkd / ActivityManager (processo :game morto?):"
  adb -s "$D" logcat -d -b main,system,events 2> /dev/null | grep -i 'lowmemorykiller\|lmkd.*kill\|am_kill.*zandatsu\|zandatsu:game.*died\|Process com.dfdx047.zandatsu:game' | head -8 || true
} | tee "$R"
