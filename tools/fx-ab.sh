#!/usr/bin/env bash
# Custo de uma opção gráfica medido A/B na mesma luta (Renut): arranca como o fight-bench (slot 3, Raiden parado), e
# troca a cvar ao vivo ("cvar nome=valor" do AutoPilot) a cada PERIOD s, CYCLES vezes ligada/desligada. Cada janela de
# 10 s do Renut ("[renut] 10.0 s: ...") que cai inteira dentro de um estado conta para esse estado. Resultado: fps de
# jogo e GPU ms médios de cada estado e a diferença, mais uma foto de cada estado (out/bench/<nome>_on/off.png).
#
# Uso:
#   bash tools/fx-ab.sh <serial> <nome> <cvar> <valor ligado> <valor desligado> [cvar = valor ...]
#   bash tools/fx-ab.sh 611e0fa0 ab_sombras swordman_renut_shadows true false \
#        'swordman_renderer = "renut"' 'sa2_render_height_handheld = 720' 'swordman_renut_cores = 2'
# PERIOD (padrão 30) e CYCLES (padrão 4). O cvars.txt do aparelho fica com as cvars do teste: repor no fim.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="$1"; N="$2"; CV_NAME="$3"; ON="$4"; OFF="$5"; shift 5
APP=com.dfdx047.swordman
A="/sdcard/Android/data/$APP/files"
OUT="$ROOT/out/bench"; mkdir -p "$OUT"
PERIOD="${PERIOD:-30}"; CYCLES="${CYCLES:-4}"
cmd() { adb -s "$D" shell am broadcast -a "$APP.CMD" -p "$APP" --es cmd "'$1'" > /dev/null; }
press() { cmd "press $1"; sleep "$2"; }
CV="env:TU_DEBUG = push_consts_per_stage\nvibration = false\n"
for l in "$@"; do CV="$CV$l\n"; done
adb -s "$D" shell am force-stop "$APP"
adb -s "$D" shell "printf '$CV' > $A/cvars.txt"
adb -s "$D" shell am start -n "$APP/.game.GameActivity" -a "$APP.PLAY" > /dev/null
sleep 50
press START 4; press DOWN 1; press DOWN 1; press A 4; press A 5; press A 25
sleep 10
for c in $(seq 1 "$CYCLES"); do
  for state in on off; do
    v="$ON"; [ "$state" = off ] && v="$OFF"
    cmd "cvar $CV_NAME=$v"
    sleep $((PERIOD / 2))
    [ "$c" = 1 ] && adb -s "$D" exec-out screencap -p > "$OUT/${N}_$state.png"
    sleep $((PERIOD - PERIOD / 2))
  done
done
cmd "cvar $CV_NAME=$ON"
adb -s "$D" pull "$A/logs/swordman_last.log" "$OUT/$N.log" > /dev/null
python3 - "$OUT/$N.log" "$N" "$CV_NAME" <<'PY'
import re, statistics, sys
log, name, cvar = sys.argv[1:]
sec = lambda t: sum(float(x) * k for x, k in zip(t.split(':'), (3600, 60, 1)))
changes, windows, lost = [], [], None
for l in open(log, errors='ignore'):
    m = re.match(r'\[\S+ (\d+:\d+:[\d.]+)\]', l)
    if not m:
        continue
    t = sec(m[1])
    c = re.search(r'autopilot: cvar ' + re.escape(cvar) + r'=(\S+)', l)
    if c:
        changes.append((t, c[1]))
    if 'device lost' in l and lost is None:
        lost = t
    r = re.search(r'\[renut\] ([\d.]+) s: (\d+) game frames \(([\d.]+) fps\).*GPU ([\d.]+) ms a frame', l)
    if r and float(r[4]) < 1000:
        windows.append((t - float(r[1]), t, float(r[3]), float(r[4])))
if not changes:
    sys.exit(f'{name}: nenhuma troca de {cvar} no log')
per = {}
for a, b, fps, gpu in windows:
    if lost and b > lost:
        continue
    state = None
    for i, (t, v) in enumerate(changes):
        end = changes[i + 1][0] if i + 1 < len(changes) else float('inf')
        if t + 1 <= a and b <= end:
            state = v
    if state is not None:
        per.setdefault(state, []).append((fps, gpu))
out = []
for state, rows in per.items():
    out.append(f'{cvar}={state}: {len(rows)} janelas, {statistics.mean(r[0] for r in rows):.1f} fps, '
               f'GPU {statistics.mean(r[1] for r in rows):.2f} ms')
hang = f'; HANG (device lost) aos {lost - changes[0][0]:.0f} s' if lost else ''
print(f'{name}: ' + ' | '.join(out) + hang)
PY
