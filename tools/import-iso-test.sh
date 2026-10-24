#!/usr/bin/env bash
# Importa uma .iso pelo launcher (botao "Importar .iso" e o seletor do Android, pela pesquisa) e mede o tempo pelo
# app.log: da linha "[import] ISO:" ate "ISO importada" (ou o erro). O app tem de estar no ecra inicial com o botao.
#
# Uso: bash tools/import-iso-test.sh <serial> <texto da pesquisa no seletor, ex. MGR_teste>
set -euo pipefail
D="$1"; Q="$2"
APP=com.dfdx047.zandatsu
LOG="/sdcard/Android/data/$APP/files/logs/app.log"

# Toca no centro do primeiro no do ecra cujo texto/descricao bate com a expressao.
tap() {
  adb -s "$D" shell uiautomator dump /sdcard/ui.xml > /dev/null
  local xy
  xy="$(adb -s "$D" shell cat /sdcard/ui.xml | python3 -c '
import re, sys
pat = re.compile(sys.argv[1], re.I)
for n in re.finditer(r"<node [^>]*>", sys.stdin.read()):
    a = n.group(0)
    t = " ".join(m[1] for m in re.finditer(r" (?:text|content-desc)=\"([^\"]*)\"", a))
    if pat.search(t):
        x1, y1, x2, y2 = map(int, re.search(r"bounds=\"\[(\d+),(\d+)\]\[(\d+),(\d+)\]\"", a).groups())
        print((x1 + x2) // 2, (y1 + y2) // 2)
        break
' "$1")"
  [ -n "$xy" ] || { echo "nao achei: $1" >&2; return 1; }
  adb -s "$D" shell input tap $xy
}

adb -s "$D" shell am start -n "$APP/.LauncherActivity" > /dev/null
sleep 3
# O botao fica abaixo do cartao do jogo: desce aos poucos ate o achar.
for _ in 1 2 3 4; do
  tap 'Importar \.iso|Import \.iso' 2> /dev/null && break
  adb -s "$D" shell input swipe 960 700 960 450 300
  sleep 1
done
sleep 3
tap '^ *(Pesquisar|Search)|Pesquisar |Search ' || adb -s "$D" shell input keyevent 84
sleep 2
adb -s "$D" shell input text "$Q"
sleep 1
# Em paisagem o teclado fica em ecra cheio e o Enter nao pesquisa: o botao dele sim.
tap '^ *(PESQUISAR|SEARCH) *$' 2> /dev/null || adb -s "$D" shell input keyevent 66
sleep 4
tap "$Q"
start=$(date +%s)
n0=$(adb -s "$D" shell "wc -l < $LOG")
until adb -s "$D" shell "tail -n +$((n0 + 1)) $LOG" | grep -q 'ISO importada\|\[import\].*\(erro\|falh\|Exception\)'; do
  [ $(( $(date +%s) - start )) -gt 1800 ] && { echo "mais de 30 min" >&2; break; }
  sleep 1
done
adb -s "$D" shell "grep -a '\[import\]' $LOG | tail -6"
