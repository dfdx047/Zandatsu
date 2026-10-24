#!/usr/bin/env bash
# Validacao de uma variante de codegen/CPU no MGR (Renut), para comparar com a base. Duas fases:
#   A: titulo e menus -> slot 1 dos saves de teste (tools/test-saves.sh; SLOT) -> Continue (save carregado na primeira luta) -> 3 min de luta com X/Y e Blade Mode
#      (LT segurado, X/Y a cortar) -> parado ate ao game over -> Continue.
#   B: abre de novo -> slot 1 dos saves de teste (tools/test-saves.sh; SLOT) -> Story -> New Game -> Normal -> sem tutorial: a cutscene do prologo do inicio ao fim
#      (uma captura a cada ~2 s ate o gameplay voltar ou 6 min). No fim o slot 1 dos saves de teste (tools/test-saves.sh; SLOT) volta a ter o autosave da luta.
# Saida em out/validate/<nome>/: capturas (A_*.png, B_*.png com o segundo desde o inicio da cutscene), o log, e
# "alive.txt" com o pid em cada passo (um pid diferente ou vazio = o jogo caiu).
#
# Uso: bash tools/validate-run.sh <serial> <nome> [cvar = valor ...]
# Comparacao com a base: python3 tools/validate-compare.py out/validate/<base> out/validate/<nome>
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="$1"; N="$2"; shift 2
APP=com.dfdx047.swordman
A="/sdcard/Android/data/$APP/files"
OUT="$ROOT/out/validate/$N"; rm -rf "$OUT"; mkdir -p "$OUT"
cmd() { adb -s "$D" shell am broadcast -a "$APP.CMD" -p "$APP" --es cmd "'$*'" > /dev/null; }
press() { cmd press "$1"; sleep "$2"; }
slot() { for _ in $(seq 2 "${SLOT:-1}"); do press DOWN 1; done; }  # SLOT (padrão 1) dos saves de teste
bash "$ROOT/tools/test-saves.sh" "$D" status | grep -q teste || { echo "antes: bash tools/test-saves.sh $D on"; exit 1; }
shot() { adb -s "$D" exec-out screencap -p > "$OUT/$1.png"; echo "$1 $(adb -s "$D" shell pidof "$APP:game" | tr -d '\r')" >> "$OUT/alive.txt"; }
launch() {
  CV="env:TU_DEBUG = push_consts_per_stage\nvibration = false\n"
  for l in "$@"; do CV="$CV$l\n"; done
  adb -s "$D" shell am force-stop "$APP"
  adb -s "$D" shell "printf '$CV' > $A/cvars.txt"
  adb -s "$D" shell am start -n "$APP/.game.GameActivity" -a "$APP.PLAY" > /dev/null
  sleep 50
}

# --- A ---
launch "$@"
shot A01_titulo
press START 4; shot A02_slots
slot; press A 4; shot A03_aviso
press A 5; shot A04_menu
press A 12; shot A05_save_12s
sleep 8; shot A06_save_20s
for i in $(seq 1 30); do   # ~3 min
  press X 1; press Y 1
  cmd seq "LT:40 LT+X:6 LT:6 LT+Y:6 LT:6 LT+X:6 LT:6 LT+Y:6 LT:20"; sleep 2
  press FORWARD 1
  [ "$i" = 10 ] && shot A07_luta_1min
  [ "$i" = 20 ] && shot A08_luta_2min
done
shot A09_luta_3min
for i in $(seq 1 16); do sleep 15; shot "A10_parado_$(printf %02d "$i")"; done   # ate 4 min parado: game over
press A 3; shot A11_gameover_a
press A 20; shot A12_continue
adb -s "$D" pull "$A/logs/swordman_last.log" "$OUT/A.log" > /dev/null

# --- B ---
launch "$@"
press START 4; slot; press A 4; press A 5
press DOWN 1; press A 4; shot B00_story
press A 4; shot B01_newgame
press A 4; shot B02_dificuldade   # (um eventual "substituir?" fica nesta captura)
press A 6; shot B03_tutorial
press RIGHT 1; cmd press A
T0=$(date +%s.%N)
for i in $(seq 1 180); do
  sleep 1
  S=$(awk -v t0="$T0" -v t="$(date +%s.%N)" 'BEGIN{printf "%05.1f", t-t0}')
  adb -s "$D" exec-out screencap -p > "$OUT/B_$S.png"
done
echo "B fim $(adb -s "$D" shell pidof "$APP:game" | tr -d '\r')" >> "$OUT/alive.txt"
adb -s "$D" pull "$A/logs/swordman_last.log" "$OUT/B.log" > /dev/null
echo "$N: $(wc -l < "$OUT/alive.txt") passos, pids $(awk '{print $2}' "$OUT/alive.txt" | sort -u | paste -sd' ')"
