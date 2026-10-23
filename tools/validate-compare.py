#!/usr/bin/env python3
"""Compara uma validacao (tools/validate-run.sh) com a da base.

    python3 tools/validate-compare.py out/validate/<base> out/validate/<variante>

- vida: o jogo nao pode cair (mesmo pid em todos os passos da fase A; vivo no fim da B);
- erros no log: "FATAL", "access violation", "DEVICE_LOST";
- posicao: as capturas logo apos carregar o save (12 s e 20 s) contra as da base;
- cutscene: cada captura da variante emparelhada com a mais parecida da base dentro de +-6 s. O desvio de tempo
  tem de ser estavel (o mesmo do inicio ao fim, a diferenca de carregamento) e a diferenca de imagem baixa.
Diferenca = media do erro absoluto em cinzento (0-255) a 320x180, sem as barras do app (topo e direita).
"""
import glob
import os
import re
import statistics
import sys

from PIL import Image


def load(path):
    im = Image.open(path).convert('L')
    w, h = im.size
    # Sem a barra de cima do app (FPS, CPU...) nem o painel do overlay a direita e o botao lateral.
    im = im.crop((0, int(h * 0.10), int(w * 0.80), h))
    return list(im.resize((320, 160)).getdata())


def diff(a, b):
    return sum(abs(x - y) for x, y in zip(a, b)) / len(a)


def alive(folder):
    lines = [l.split() for l in open(os.path.join(folder, 'alive.txt'))]
    a_pids = {l[1] if len(l) > 1 else '' for l in lines if l[0].startswith('A')}
    b_end = [l for l in lines if l[0] == 'B']
    ok_a = len(a_pids) == 1 and '' not in a_pids
    ok_b = bool(b_end) and len(b_end[0]) > 2 and b_end[0][2] != ''
    return ok_a, ok_b


def log_errors(folder):
    out = []
    for name in ('A.log', 'B.log'):
        path = os.path.join(folder, name)
        if os.path.exists(path):
            for line in open(path, errors='ignore'):
                if re.search(r'FATAL|access violation|DEVICE_LOST', line):
                    out.append(f'{name}: {line.strip()[:160]}')
    return out


def cutscene(folder):
    shots = []
    for path in sorted(glob.glob(os.path.join(folder, 'B_*.png'))):
        t = float(os.path.basename(path)[2:-4])
        shots.append((t, load(path)))
    return shots


def main():
    base, var = sys.argv[1], sys.argv[2]
    result = {}
    ok_a, ok_b = alive(var)
    result['vivo'] = ok_a and ok_b
    print(f'vivo: fase A {"ok" if ok_a else "CAIU"}, fase B {"ok" if ok_b else "CAIU"}')
    errors = log_errors(var)
    print(f'erros no log: {len(errors)}')
    for e in errors[:5]:
        print('  ' + e)
    for name in ('A05_save_12s', 'A06_save_20s'):
        pb, pv = os.path.join(base, name + '.png'), os.path.join(var, name + '.png')
        if os.path.exists(pb) and os.path.exists(pv):
            print(f'posicao {name}: diferenca {diff(load(pb), load(pv)):.1f}')
    sb, sv = cutscene(base), cutscene(var)
    offsets, diffs = [], []
    for t, im in sv:
        cands = [(diff(im, bim), bt - t) for bt, bim in sb if abs(bt - t) <= 6.0]
        if cands:
            d, off = min(cands)
            offsets.append(off)
            diffs.append(d)
    if offsets:
        n = len(offsets)
        first, last = offsets[: n // 4], offsets[-(n // 4):]
        drift = statistics.median(last) - statistics.median(first)
        print(f'cutscene: {n} capturas, desvio de tempo mediano {statistics.median(offsets):+.1f} s '
              f'(deriva do inicio ao fim {drift:+.1f} s), diferenca mediana {statistics.median(diffs):.1f}, '
              f'max {max(diffs):.1f}')


if __name__ == '__main__':
    main()
