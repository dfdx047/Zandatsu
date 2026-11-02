#!/usr/bin/env bash
# Apanha um hang da GPU que o kgsl recupera, com o anel da CPU do Renut, para o hangmap.py.
#
# Requisito: o jogo a correr com swordman_renut_gpu_markers = true e swordman_renut_gpu_markers_cpu = true.
# Segue o faultcount do kgsl. O proprio jogo grava logs/renut_gpu_hang.txt ao ver o hang (gpu_reset.cpp), antes de
# fechar. Cada hang fica em <pasta>/<n>/ com dmesg_hang.txt, kgsl_snapshot.bin e renut_gpu_hang.txt; depois:
# python3 tools/kgsl/hangmap.py <pasta>/<n>.
#
# 02/11: seguia o "dmesg -w", que rele os hangs antigos ao abrir, e os adb do ciclo comiam as linhas seguintes.
#
# Uso: bash tools/kgsl/hangwatch.sh <serial> <pasta> [max. de hangs, padrao 3]
set -uo pipefail
D="$1"; OUT="$2"; MAX="${3:-3}"
APP=com.dfdx047.zandatsu
L="/sdcard/Android/data/$APP/files/logs"
FC=/sys/class/kgsl/kgsl-3d0/snapshot/faultcount
mkdir -p "$OUT"
sh_() { adb -s "$D" shell "$@" < /dev/null | tr -d '\r'; }
sh_ "rm -f $L/renut_gpu_hang.txt"
last=$(sh_ "cat $FC")
n=0
while [ "$n" -lt "$MAX" ]; do
  sleep 1
  cur=$(sh_ "cat $FC")
  [ -n "$cur" ] && [ "$cur" != "$last" ] || continue
  last=$cur
  n=$((n + 1)); H="$OUT/$n"; mkdir -p "$H"
  sh_ "dmesg | grep -E 'GPU hang detected|gpu fault ctx' | tail -2" > "$H/dmesg_hang.txt"
  adb -s "$D" exec-out cat /sys/class/kgsl/kgsl-3d0/snapshot/dump < /dev/null > "$H/kgsl_snapshot.bin"
  sleep 3
  adb -s "$D" pull "$L/renut_gpu_hang.txt" "$H/renut_gpu_hang.txt" < /dev/null > /dev/null 2>&1
  sh_ "rm -f $L/renut_gpu_hang.txt"
  echo "hang $n: $(TZ=Europe/Lisbon date +%T), snapshot $(stat -c %s "$H/kgsl_snapshot.bin") B, anel $([ -s "$H/renut_gpu_hang.txt" ] && echo ok || echo NENHUM)"
done
