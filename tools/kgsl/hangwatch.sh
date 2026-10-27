#!/usr/bin/env bash
# Apanha um hang da GPU que o kgsl recupera (sem device lost) com o anel da CPU do Renut, para o hangmap.py.
#
# Requisito: o jogo a correr com swordman_renut_gpu_markers = true e swordman_renut_gpu_markers_cpu = true.
# Segue o dmesg; no "GPU hang detected" cria logs/renut_gpu_ring.req (o jogo escreve o anel no frame seguinte),
# guarda as linhas do hang e o snapshot do kgsl, e puxa o anel. Cada hang fica em <pasta>/<n>/ com
# dmesg_hang.txt, kgsl_snapshot.bin e renut_gpu_hang.txt; depois: python3 tools/kgsl/hangmap.py <pasta>/<n>.
#
# Uso: bash tools/kgsl/hangwatch.sh <serial> <pasta> [máx. de hangs, padrão 3]
set -uo pipefail
D="$1"; OUT="$2"; MAX="${3:-3}"
APP=com.dfdx047.zandatsu
L="/sdcard/Android/data/$APP/files/logs"
mkdir -p "$OUT"
adb -s "$D" shell "rm -f $L/renut_gpu_ring.req $L/renut_gpu_ring_*.txt"
n=0
adb -s "$D" shell dmesg -w | while IFS= read -r line; do
  case "$line" in
    *"GPU hang detected"*)
      adb -s "$D" shell "touch $L/renut_gpu_ring.req"
      n=$((n + 1)); H="$OUT/$n"; mkdir -p "$H"
      echo "$line" > "$H/dmesg_hang.txt"
      IFS= read -r fault && echo "$fault" >> "$H/dmesg_hang.txt"
      adb -s "$D" exec-out cat /sys/class/kgsl/kgsl-3d0/snapshot/dump > "$H/kgsl_snapshot.bin"
      sleep 3
      ring=$(adb -s "$D" shell "ls -t $L/renut_gpu_ring_*.txt 2>/dev/null | head -1" | tr -d '\r')
      if [ -n "$ring" ]; then
        adb -s "$D" pull "$ring" "$H/renut_gpu_hang.txt" > /dev/null
      fi
      echo "hang $n: $(TZ=Europe/Lisbon date +%T), snapshot $(stat -c %s "$H/kgsl_snapshot.bin") B, anel ${ring:-NENHUM}"
      [ "$n" -ge "$MAX" ] && break
      ;;
  esac
done
