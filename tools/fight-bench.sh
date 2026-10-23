#!/usr/bin/env bash
# Medicao na primeira luta do MGR, reprodutivel (Renut e Xenia lado a lado).
#
# Requisito: o slot 3 com um jogo novo (Story > New Game > Normal, sem tutorial; o autosave fica no inicio da
# primeira luta do prologo). O script: arranca o jogo com as cvars dadas, espera o titulo, START, slot 3, OK,
# Continue, espera a luta, carrega X a cada 1,5 s durante 45 s e puxa o log. O resumo (fps de jogo, GPU ms, draws)
# sai das linhas "[renut] 10.0 s: ..." (Renut) ou "dbz3: perf ... gpu_ms" (Xenia) dentro da janela da luta.
#
# Uso:
#   bash tools/fight-bench.sh <serial> <nome> [cvar = valor ...]
#   bash tools/fight-bench.sh 611e0fa0 renut720 'swordman_renderer = "renut"' 'sa2_render_height_handheld = 720'
#   bash tools/fight-bench.sh 611e0fa0 xenos540 'draw_resolution_percent = 75'
# Resultado em out/bench/<nome>.log, <nome>_luta.png e uma linha de resumo. O cvars.txt do aparelho fica com as
# cvars do teste: repor no fim.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="$1"; N="$2"; shift 2
APP=com.dfdx047.swordman
A="/sdcard/Android/data/$APP/files"
OUT="$ROOT/out/bench"; mkdir -p "$OUT"
press() { adb -s "$D" shell am broadcast -a "$APP.CMD" -p "$APP" --es cmd "'press $1'" > /dev/null; sleep "$2"; }
CV="env:TU_DEBUG = push_consts_per_stage\nvibration = false\n"
for l in "$@"; do CV="$CV$l\n"; done
adb -s "$D" shell am force-stop "$APP"
adb -s "$D" shell "printf '$CV' > $A/cvars.txt"
adb -s "$D" shell am start -n "$APP/.game.GameActivity" -a "$APP.PLAY" > /dev/null
sleep 50
press START 4; press DOWN 1; press DOWN 1; press A 4; press A 5; press A 25
T0=$(date +%T)
for i in $(seq 1 30); do
  press X 1.5
  [ "$i" = 12 ] && adb -s "$D" exec-out screencap -p > "$OUT/${N}_luta.png"
done
T1=$(date +%T)
adb -s "$D" pull "$A/logs/swordman_last.log" "$OUT/$N.log" > /dev/null
python3 - "$OUT/$N.log" "$T0" "$T1" "$N" <<'PY'
import re, sys
log, t0, t1, name = sys.argv[1:]
sec = lambda t: sum(float(x) * k for x, k in zip(t.split(':'), (3600, 60, 1)))
t0, t1 = sec(t0) + 5, sec(t1) + 1
rows = []
for l in open(log, errors='ignore'):
    m = re.match(r'\[\S+ (\d+:\d+:[\d.]+)\]', l)
    if not m or not t0 <= sec(m[1]) <= t1:
        continue
    r = re.search(r'\[renut\] [\d.]+ s: (\d+) game frames \(([\d.]+) fps\).*GPU ([\d.]+) ms a frame.*?; (\d+) draws', l)
    if r:
        rows.append((float(r[2]), float(r[3]), int(r[4]) / int(r[1])))
    x = re.search(r'game_fps=([\d.]+).*gpu_ms avg=([\d.]+).*draws=(\d+)', l)
    if x:
        rows.append((float(x[1]), float(x[2]), float(x[3])))
if not rows:
    sys.exit(f'{name}: sem linhas de perf na janela')
n = len(rows)
avg = [sum(r[i] for r in rows) / n for i in range(3)]
print(f'{name}: {n} janelas, {avg[0]:.1f} fps de jogo, GPU {avg[1]:.1f} ms/frame, {avg[2]:.0f} draws/frame')
PY
