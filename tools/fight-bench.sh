#!/usr/bin/env bash
# Medicao na primeira luta do MGR, reprodutivel (Renut e Xenia lado a lado).
#
# Requisito: o slot 1 dos saves de teste (tools/test-saves.sh; SLOT) com um jogo novo (Story > New Game > Normal, sem tutorial; o autosave fica no inicio da
# primeira luta do prologo). O script: arranca o jogo com as cvars dadas, espera o titulo, START, slot 1 dos saves de teste (tools/test-saves.sh; SLOT), OK,
# Continue, espera a luta, carrega X a cada 1,5 s durante 45 s e puxa o log. O resumo (fps de jogo, GPU ms, draws)
# sai das linhas "[renut] 10.0 s: ..." (Renut) ou "dbz3: perf ... gpu_ms" (Xenia) dentro da janela da luta.
#
# Uso:
#   bash tools/fight-bench.sh <serial> <nome> [cvar = valor ...]
#   bash tools/fight-bench.sh 611e0fa0 renut720 'swordman_renderer = "renut"' 'sa2_render_height_handheld = 720'
#   bash tools/fight-bench.sh 611e0fa0 xenos540 'draw_resolution_percent = 75'
# FIGHT_S = segundos de luta (padrão 45). Um "device lost" (GPU pendurada) é contado: minutos desde o início da luta,
# e só as janelas anteriores entram nas médias (depois dele o jogo continua a contar frames sem desenhar). Um device
# lost antes da luta (título, menus, carregamento) também conta: segundos desde o arranque, e a corrida não tem médias.
# ATTACK=0 deixa o Raiden parado (a luta vem ter com ele; a cena varia muito menos que com golpes: para comparar
# mudanças pequenas). O resumo usa a mediana das janelas e descarta GPU ms impossíveis (> 1000; o Renut às vezes
# reporta uma janela errada).
# Resultado em out/bench/<nome>.log, <nome>_luta.png e uma linha de resumo. O jogo fecha no fim (KEEP=1 deixa-o
# aberto). O cvars.txt do aparelho fica com as cvars do teste: repor no fim.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="$1"; N="$2"; shift 2
APP=com.dfdx047.zandatsu
A="/sdcard/Android/data/$APP/files"
OUT="$ROOT/out/bench"; mkdir -p "$OUT"
press() { adb -s "$D" shell am broadcast -a "$APP.CMD" -p "$APP" --es cmd "'press $1'" > /dev/null; sleep "$2"; }
slot() { for _ in $(seq 2 "${SLOT:-1}"); do press DOWN 1; done; }  # SLOT (padrão 1) dos saves de teste
bash "$ROOT/tools/test-saves.sh" "$D" status | grep -q teste || { echo "antes: bash tools/test-saves.sh $D on"; exit 1; }
CV="env:TU_DEBUG = push_consts_per_stage\nvibration = false\n"
for l in "$@"; do CV="$CV$l\n"; done
adb -s "$D" shell am force-stop "$APP"
adb -s "$D" shell "printf '$CV' > $A/cvars.txt"
adb -s "$D" shell am start -n "$APP/.game.GameActivity" -a "$APP.PLAY" > /dev/null
sleep 50
press START 4; slot; press A 4; press A 5; press A 25
T0=$(date +%T)
# CPU da thread principal do jogo (a do Swap; o Renut escreve "[t<tid>] [renut] game thread"), entre as voltas 5 e 25.
ticks() {
  adb -s "$D" shell "sed 's/.*) //' /proc/$PID/task/$GTID/stat 2>/dev/null | awk '{print \$12+\$13}'" | tr -d '\r'
}
PID=$(adb -s "$D" shell pidof "$APP:game" | tr -d '\r')
GTID=$(adb -s "$D" shell "grep -m1 'game thread' $A/logs/swordman_last.log" | sed -nE 's/.*\[t([0-9]+)\] \[renut\] game thread.*/\1/p' || true)
MAIN_CPU=""
ROUNDS=$(( ${FIGHT_S:-45} * 2 / 3 ))
for i in $(seq 1 "$ROUNDS"); do
  # A a cada 8 voltas: na luta é um salto; no game over (o Raiden morre em ~3 min de golpes) escolhe Continue e a
  # luta recomeça do checkpoint (senão o resto da corrida ficava no ecrã de game over, ~78 draws/frame).
  if [ "${ATTACK:-1}" = 1 ]; then if [ $((i % 8)) = 0 ]; then press A 1.5; else press X 1.5; fi; else sleep 1.5; fi
  [ "$i" = 12 ] && adb -s "$D" exec-out screencap -p > "$OUT/${N}_luta.png"
  if [ -n "$GTID" ] && [ "$i" = 5 ]; then C0=$(ticks); S0=$(date +%s.%N); fi
  if [ -n "$GTID" ] && [ "$i" = $(( ROUNDS - 5 )) ] && [ -n "${C0:-}" ]; then
    C1=$(ticks); S1=$(date +%s.%N)
    MAIN_CPU=$(awk -v a="$C0" -v b="$C1" -v s0="$S0" -v s1="$S1" 'BEGIN{printf "%.0f", (b-a)/(s1-s0)}')
  fi
done
T1=$(date +%T)
adb -s "$D" pull "$A/logs/swordman_last.log" "$OUT/$N.log" > /dev/null
# Fecha o jogo (o aparelho aquecia entre corridas com o jogo a correr no game over/título). KEEP=1 deixa-o aberto.
[ "${KEEP:-0}" = 1 ] || adb -s "$D" shell am force-stop "$APP"
python3 - "$OUT/$N.log" "$T0" "$T1" "$N" "$MAIN_CPU" <<'PY'
import re, sys
log, t0, t1, name, main_cpu = sys.argv[1:]
sec = lambda t: sum(float(x) * k for x, k in zip(t.split(':'), (3600, 60, 1)))
t0, t1 = sec(t0) + 5, sec(t1) + 1
rows, swaps, cpu = [], [], []
game_tid = None
lines = open(log, errors='ignore').read().splitlines()
lost = start = None
for l in lines:
    m = re.match(r'\[\S+ (\d+:\d+:[\d.]+)\]', l)
    start = start if start is not None or not m else sec(m[1])
    if m and 'device lost' in l:
        lost = sec(m[1])
        break
if lost is not None and lost < t0 - 5:
    sys.exit(f'{name}: HANG antes da luta, {lost - start:.0f} s depois do arranque')
end = min(t1, lost) if lost else t1
for l in lines:
    m = re.match(r'\[\S+ (\d+:\d+:[\d.]+)\]', l)
    if not m or not t0 <= sec(m[1]) <= end:
        continue
    r = re.search(r'\[renut\] [\d.]+ s: (\d+) game frames \(([\d.]+) fps\).*GPU ([\d.]+) ms a frame.*?; (\d+) draws', l)
    if r:
        rows.append((float(r[2]), float(r[3]), int(r[4]) / int(r[1])))
    w = re.search(r'\[t(\d+)\] \[renut\] game thread, [\d.]+ s: \d+ frames, ([\d.]+) ms a frame from Swap to Swap', l)
    if w:
        swaps.append(float(w[2]))
        game_tid = w[1]
    x = re.search(r'game_fps=([\d.]+).*gpu_ms avg=([\d.]+).*draws=(\d+)', l)
    if x:
        rows.append((float(x[1]), float(x[2]), float(x[3])))
hang = f', HANG aos {(lost - (t0 - 5)) / 60:.1f} min de luta' if lost else ', sem hang'
if not rows:
    sys.exit(f'{name}: sem linhas de perf na janela{hang}')
import statistics
n = len(rows)
med = [statistics.median([r[i] for r in rows if i != 1 or r[1] < 1000] or [0]) for i in range(3)]
swap = f', Swap a Swap {statistics.median(swaps):.1f} ms' if swaps else ''
if main_cpu:
    swap += f', thread principal {main_cpu}% de um núcleo'
print(f'{name}: {n} janelas, {med[0]:.1f} fps de jogo, GPU {med[1]:.1f} ms/frame{swap}, {med[2]:.0f} draws/frame '
      f'(medianas){hang}')
PY
