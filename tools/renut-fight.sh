#!/usr/bin/env bash
# Uma corrida de luta no Renut com cvars dadas (no máximo ~4 min): cvars.txt com as cvars, slot 1 dos saves de teste
# (tools/test-saves.sh), foto no título, no início da luta e no fim, $SECS s de luta a atacar (X/Y), GPU ms e fps de
# jogo das janelas de 10 s do Renut que caem dentro da luta. Fotos em out/bench/<nome>_*.png.
#
# Uso: bash tools/renut-fight.sh <serial> <nome> [cvar = valor ...]
#   SECS=120 bash tools/renut-fight.sh 611e0fa0 r1080 'sa2_render_height_handheld = 1080'
# O cvars.txt fica com as cvars do teste: repor no fim.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="$1"; N="$2"; shift 2
APP=com.dfdx047.zandatsu
A="/sdcard/Android/data/$APP/files"
OUT="$ROOT/out/bench"; mkdir -p "$OUT"
SECS="${SECS:-120}"
cmd() { adb -s "$D" shell am broadcast -a "$APP.CMD" -p "$APP" --es cmd "'$1'" > /dev/null; }
press() { cmd "press $1"; sleep "$2"; }
shot() { adb -s "$D" exec-out screencap -p > "$OUT/${N}_$1.png"; }
bash "$ROOT/tools/test-saves.sh" "$D" status | grep -q teste || { echo "antes: bash tools/test-saves.sh $D on"; exit 1; }
CV="env:TU_DEBUG = push_consts_per_stage\nvibration = false\nswordman_boot_fast_forward = 8.0\n"
for l in "$@"; do CV="$CV$l\n"; done
adb -s "$D" shell am force-stop "$APP"
adb -s "$D" push "$ROOT/out/test_saves/GameData_00" "$A/mgr/B13EBABEBABEBABE/4B4E080A/00000001/SaveData/" > /dev/null
adb -s "$D" shell "printf '$CV' > $A/cvars.txt"
adb -s "$D" shell am start -n "$APP/.game.GameActivity" -a "$APP.PLAY" > /dev/null
sleep "${BOOT:-45}"; shot titulo
press START 4; press A 4; press A 6; press A "${LOAD:-25}"
shot luta
cmd "cvar vibration=false"   # marca o início da luta no log (cvar neutra)
end=$((SECONDS + SECS))
while [ $SECONDS -lt $end ]; do press X 0.7; press Y 0.7; done
shot fim
adb -s "$D" pull "$A/logs/swordman_last.log" "$OUT/$N.log" > /dev/null
python3 - "$OUT/$N.log" "$N" <<'PY'
import re, statistics, sys
log, name = sys.argv[1:]
sec = lambda t: sum(float(x) * k for x, k in zip(t.split(':'), (3600, 60, 1)))
start, lost, win, res = None, None, [], ''
for l in open(log, errors='ignore'):
    m = re.match(r'\[\S+ (\d+:\d+:[\d.]+)\]', l)
    if not m:
        continue
    t = sec(m[1])
    if 'autopilot: cvar vibration' in l and start is None:
        start = t
    if 'device lost' in l and lost is None:
        lost = t
    r = re.search(r'\[renut\] ([\d.]+) s: (\d+) game frames \(([\d.]+) fps\).*GPU ([\d.]+) ms a frame', l)
    if r and start and t - float(r[1]) >= start and float(r[4]) < 1000 and not (lost and t > lost):
        win.append((float(r[3]), float(r[4])))
    # Xenia (dbz3_perf_logging): janelas de 5 s, sem GPU ms.
    x = re.search(r'dbz3: perf fps=[\d.]+ frames=\d+ window=([\d.]+)s.*game_fps=([\d.]+)', l)
    if x and start and t - float(x[1]) >= start and not (lost and t > lost):
        win.append((float(x[2]), float('nan')))
    s = re.search(r'(\d{3,4}x\d{3,4})', l) if 'scale' in l.lower() and not res else None
    if s:
        res = s[1]
if not win:
    sys.exit(f'{name}: sem janelas na luta')
print(f'{name}: {len(win)} janelas, {statistics.mean(w[0] for w in win):.1f} fps de jogo, '
      f'GPU {statistics.mean(w[1] for w in win):.2f} ms (min {min(w[1] for w in win):.2f}, max {max(w[1] for w in win):.2f})'
      + (f'; HANG aos {lost - start:.0f} s' if lost and start else ''))
PY
