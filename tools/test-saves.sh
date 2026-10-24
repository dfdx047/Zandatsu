#!/usr/bin/env bash
# Saves de teste: o MGR só tem 3 slots e os do jogador não são para testes. "on" guarda a pasta SaveData do jogador
# em SaveData.jogador e põe no lugar os 3 slots de teste (out/test_saves/, todos no autosave do início da primeira
# luta; o slot 1 é o que o fight-bench, o fx-ab e o validate-run usam). "off" apaga a pasta de teste e devolve a do
# jogador. Com os saves de teste há um ficheiro .swordman_test na pasta (os scripts de medição exigem-no).
#
# Uso: bash tools/test-saves.sh <serial> on|off|status
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="$1"; OP="$2"
APP=com.dfdx047.zandatsu
P="/sdcard/Android/data/$APP/files/mgr/B13EBABEBABEBABE/4B4E080A/00000001"
SRC="$ROOT/out/test_saves"
on() { adb -s "$D" shell "[ -f $P/SaveData/.swordman_test ]"; }
case "$OP" in
  on)
    if on; then echo "saves de teste já estão"; exit 0; fi
    [ -f "$SRC/GameData_00" ] || { echo "faltam os saves de teste em $SRC"; exit 1; }
    adb -s "$D" shell "[ ! -e $P/SaveData.jogador ]" || { echo "já existe SaveData.jogador: ver à mão"; exit 1; }
    adb -s "$D" shell am force-stop "$APP"
    adb -s "$D" shell "mv $P/SaveData $P/SaveData.jogador && mkdir $P/SaveData && touch $P/SaveData/.swordman_test"
    for f in "$SRC"/GameData_0*; do adb -s "$D" push "$f" "$P/SaveData/" > /dev/null; done
    echo "saves de teste ligados" ;;
  off)
    on || { echo "saves do jogador já estão"; exit 0; }
    adb -s "$D" shell "[ -d $P/SaveData.jogador ]" || { echo "falta SaveData.jogador: ver à mão"; exit 1; }
    adb -s "$D" shell am force-stop "$APP"
    adb -s "$D" shell "rm -r $P/SaveData && mv $P/SaveData.jogador $P/SaveData"
    echo "saves do jogador de volta" ;;
  status)
    if on; then echo "teste"; else echo "jogador"; fi ;;
esac
