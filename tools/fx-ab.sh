#!/usr/bin/env bash
# Teste de uma opção gráfica do Renut (no máximo ~5 min): arranca com a opção desligada (cvars.txt), foto no título,
# carrega a luta (slot 1 dos saves de teste = cópia do slot 3 do jogador, tools/test-saves.sh), foto na luta e na pausa, 2 min de luta desligada,
# a atacar (X/Y), troca ao vivo ("cvar nome=valor" do AutoPilot) e 2 min ligada. fps de jogo e GPU ms de cada estado saem das janelas
# de 10 s do Renut ("[renut] 10.0 s: ...") que caem inteiras dentro desse estado. Fotos em out/bench/<nome>_*.png.
#
# Uso:
#   bash tools/fx-ab.sh <serial> <nome> <cvar> <valor ligado> <valor desligado> [cvar = valor ...]
#   bash tools/fx-ab.sh 611e0fa0 ab_sombras swordman_renut_shadows true false \
#        'swordman_renderer = "renut"' 'sa2_render_height_handheld = 720' 'swordman_renut_cores = 2'
# Com <cvar> = "-" não troca nada (corrida de referência: 4 min com as cvars dadas). O cvars.txt fica com as cvars
# do teste: repor no fim.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="$1"; N="$2"; CV_NAME="$3"; ON="$4"; OFF="$5"; shift 5
APP=com.dfdx047.swordman
A="/sdcard/Android/data/$APP/files"
OUT="$ROOT/out/bench"; mkdir -p "$OUT"
HALF="${HALF:-120}"
cmd() { adb -s "$D" shell am broadcast -a "$APP.CMD" -p "$APP" --es cmd "'$1'" > /dev/null; }
press() { cmd "press $1"; sleep "$2"; }
shot() { adb -s "$D" exec-out screencap -p > "$OUT/${N}_$1.png"; }
slot() { for _ in $(seq 2 "${SLOT:-1}"); do press DOWN 1; done; }  # SLOT (padrão 1) dos saves de teste (o 3 de teste aparece vazio no jogo)
bash "$ROOT/tools/test-saves.sh" "$D" status | grep -q teste || { echo "antes: bash tools/test-saves.sh $D on"; exit 1; }
CV="env:TU_DEBUG = push_consts_per_stage\nvibration = false\n"
for l in "$@"; do CV="$CV$l\n"; done
[ "$CV_NAME" != - ] && CV="$CV$CV_NAME = $OFF\n"
adb -s "$D" shell am force-stop "$APP"
# O slot de teste volta ao início da luta (uma corrida anterior pode tê-lo gravado).
adb -s "$D" push "$ROOT/out/test_saves/GameData_0$((${SLOT:-1} - 1))" \
  "$A/mgr/B13EBABEBABEBABE/4B4E080A/00000001/SaveData/" > /dev/null
adb -s "$D" shell "printf '$CV' > $A/cvars.txt"
adb -s "$D" shell am start -n "$APP/.game.GameActivity" -a "$APP.PLAY" > /dev/null
sleep 45; shot titulo
press START 4; slot; press A 4; press A 6; press A 25
shot luta
press START 2; shot pausa; press START 1
[ "$CV_NAME" != - ] && cmd "cvar $CV_NAME=$OFF"   # marca o início do estado desligado no log
fight() {  # X/Y a cada ~1,5 s durante $1 s: parado, o Raiden morre em ~2 min
  local end=$((SECONDS + $1))
  while [ $SECONDS -lt $end ]; do press X 0.7; press Y 0.7; done
}
fight "$HALF"
[ "$CV_NAME" != - ] && cmd "cvar $CV_NAME=$ON"
fight "$HALF"
shot fim
adb -s "$D" pull "$A/logs/swordman_last.log" "$OUT/$N.log" > /dev/null
python3 - "$OUT/$N.log" "$N" "$CV_NAME" <<'PY'
import re, statistics, sys
log, name, cvar = sys.argv[1:]
sec = lambda t: sum(float(x) * k for x, k in zip(t.split(':'), (3600, 60, 1)))
changes, windows, lost, start = [], [], None, None
for l in open(log, errors='ignore'):
    m = re.match(r'\[\S+ (\d+:\d+:[\d.]+)\]', l)
    if not m:
        continue
    t = sec(m[1])
    if 'autopilot: press START' in l and start is None:
        start = t
    c = re.search(r'autopilot: cvar ' + re.escape(cvar) + r'=(\S+)', l)
    if c:
        changes.append((t, c[1]))
    if 'device lost' in l and lost is None:
        lost = t
    r = re.search(r'\[renut\] ([\d.]+) s: (\d+) game frames \(([\d.]+) fps\).*GPU ([\d.]+) ms a frame', l)
    if r and float(r[4]) < 1000:
        windows.append((t - float(r[1]), t, float(r[3]), float(r[4])))
if cvar == '-':
    changes = [(start + 45, 'ref')] if start else []
if not changes:
    sys.exit(f'{name}: sem marcas no log')
per = {}
for a, b, fps, gpu in windows:
    if lost and b > lost:
        continue
    for i, (t, v) in enumerate(changes):
        end = changes[i + 1][0] if i + 1 < len(changes) else float('inf')
        if t + 1 <= a and b <= end:
            per.setdefault(v, []).append((fps, gpu))
out = [f'{cvar}={s}: {len(r)} janelas, {statistics.mean(x[0] for x in r):.1f} fps, GPU {statistics.mean(x[1] for x in r):.2f} ms'
       for s, r in per.items()]
hang = f'; HANG (device lost) aos {lost - start:.0f} s do START' if lost and start else ''
print(f'{name}: ' + ' | '.join(out) + hang)
PY
