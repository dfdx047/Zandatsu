#!/usr/bin/env python3
"""Hang da GPU no Renut: snapshot do kgsl + anel da CPU (swordman_renut_gpu_markers_cpu) -> o draw pendurado.

    python3 hangmap.py <pasta com kgsl_snapshot.bin, dmesg_hang.txt e renut_gpu_hang.txt>

1. dmesg: IB1/IB2 do CP no hang (base e dwords que faltam) -> IB2 e posição do PFP.
2. Registos de contexto (MVC): SP_VS_BASE / SP_PS_BASE = o programa que o hardware tinha (o ME só vai 1-2
   contextos à frente do draw pendurado).
3. Draws do IB2: contagem, instâncias, programa (página do estado VS/FS); os que usam o programa dos registos.
4. Anel da CPU: a sequência de contagens dos draws do IB2 alinhada com os draws gravados -> hashes do jogo.
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pm4, mvc

folder = sys.argv[1]
d = open(os.path.join(folder, 'kgsl_snapshot.bin'), 'rb').read()
dm = open(os.path.join(folder, 'dmesg_hang.txt')).read()
m = re.search(r'status (\w+).*ib1 (\w+)/(\w+) ib2 (\w+)/(\w+)', dm)
status, ib1, ib1rem, ib2, ib2rem = m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16), int(m.group(5), 16)
ibs = pm4.ibs(d)
regs = mvc.mvc(d)
ctx = {c: (r.get(0xa81c, 0) | r.get(0xa81d, 0) << 32, r.get(0xa983, 0) | r.get(0xa984, 0) << 32) for c, r in regs.items()}
print(f'status {status}; IB1 {ib1:#x} faltam {ib1rem:#x}; IB2 {ib2:#x} faltam {ib2rem:#x}')
for c, (vs, ps) in ctx.items():
    print(f'contexto {c}: SP_VS_BASE {vs:#x}  SP_PS_BASE {ps:#x}')

# Tamanho do IB2 pela chamada no IB1.
w1 = ibs.get(ib1, ())
ib2_size = None
i = 0
calls = []
while i < len(w1):
    h = w1[i]
    if h >> 28 == 7:
        cnt = h & 0x3fff; op = (h >> 16) & 0x7f
        if op == 0x3f:
            p = w1[i + 1:i + 4]; tgt = p[0] | p[1] << 32
            calls.append((i, tgt, p[2] & 0xfffff))
            if tgt == ib2: ib2_size = p[2] & 0xfffff
        i += 1 + cnt
    elif h >> 28 == 4: i += 1 + (h & 0x7f)
    else: i += 1
pos = (ib2_size - ib2rem) if ib2_size else None
print(f'IB2 com {ib2_size} dwords; PFP no dword {pos} do IB2' if ib2_size else 'IB2 não encontrado no IB1')

# Draws do IB2.
w = ibs.get(ib2, ())
groups = {}; draws = []
i = 0
while i < len(w):
    h = w[i]
    if h >> 28 == 7:
        cnt = h & 0x3fff; op = (h >> 16) & 0x7f; p = w[i + 1:i + 1 + cnt]
        if op == 0x43:
            for k in range(0, len(p) - 2, 3):
                hd = p[k]; gid = (hd >> 24) & 0x1f; c = hd & 0xffff
                if (hd >> 18) & 1: groups = {}
                elif (hd >> 17) & 1 or c == 0: groups.pop(gid, None)
                else: groups[gid] = p[k + 1] | p[k + 2] << 32
        elif op == 0x38 and len(p) >= 3:
            src = (p[0] >> 6) & 3
            draws.append(dict(at=i, count=p[2], inst=p[1], auto=(src == 2), prim=p[0] & 0x3f,
                              vs=groups.get(1, 0), fs=groups.get(8, 0)))
        i += 1 + cnt
    elif h >> 28 == 4: i += 1 + (h & 0x7f)
    else: i += 1
page = lambda a: a >> 12
hung_pages = {(page(vs), page(ps)) for vs, ps in ctx.values()}
print(f'{len(draws)} draws no IB2 capturado')
for n, dr in enumerate(draws):
    dr['n'] = n + 1
    dr['hungprog'] = (page(dr['vs']), page(dr['fs'])) in hung_pages
    dr['afterpfp'] = pos is not None and dr['at'] >= pos

# Anel da CPU.
ring_path = os.path.join(folder, 'renut_gpu_hang.txt')
if not os.path.exists(ring_path):
    for dr in draws:
        if dr['hungprog']: print(f"  draw {dr['n']:3d} @{dr['at']} count {dr['count']} programa dos registos")
    sys.exit('sem renut_gpu_hang.txt (anel da CPU)')
ring = []
for l in open(ring_path, errors='ignore'):
    p = l[1:].split()
    if len(p) >= 15 and p[0].isdigit() and p[2] == 'draw':
        ring.append(dict(idx=int(p[0]), frame=int(p[1]), ps=p[3], vs=p[4], cva=p[5], csize=p[6], cfmt=p[8],
                         dva=p[9], dfmt=p[11], prim=int(p[12]), count=int(p[13]), inst=int(p[14]), line=l.rstrip()))
seq = [dr['count'] for dr in draws]
best = (0, None)
for k in range(0, len(ring) - len(seq) + 1):
    s = sum(1 for j, c in enumerate(seq) if ring[k + j]['count'] == c)
    if s > best[0]: best = (s, k)
score, k = best
print(f'alinhamento com o anel: {score}/{len(seq)} contagens iguais a partir do comando {ring[k]["idx"] if k is not None else "-"}')
if k is None: sys.exit()
print('\n  n   IB2@   contagem  anel: índice frame  PS                VS                cor            profund.')
for dr in draws:
    r = ring[k + dr['n'] - 1]
    tag = ('P' if dr['hungprog'] else ' ') + ('>' if dr['afterpfp'] and not any(x['afterpfp'] for x in draws[:dr['n'] - 1]) else ' ')
    same = '' if r['count'] == dr['count'] else '  (contagem diferente!)'
    print(f"{tag}{dr['n']:3d} {dr['at']:6d}  {dr['count']:6d}   {r['idx']:8d} {r['frame']:6d}  {r['ps']}  {r['vs']}  {r['cva']} {r['csize']} f{r['cfmt']}  {r['dva']} f{r['dfmt']}{same}")
print("\nP = usa o programa dos registos de contexto (SP_VS_BASE/SP_PS_BASE); > = primeiro draw que o PFP ainda não leu")
