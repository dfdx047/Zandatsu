# Resume um logs/frame_dump_<N>.txt (swordman_renut_frame_dump) em passes: draws seguidos no mesmo alvo, com os
# pixel shaders e as texturas lidas; clears (C) e resolves (R) entre eles. Uso: python3 tools/renut-frame-passes.py <ficheiro>
import re, sys
from collections import OrderedDict
lines = open(sys.argv[1]).read().splitlines()[1:]
cur = None; out = []
def flush():
    if cur: out.append(cur)
for i, l in enumerate(lines):
    k = l.split()[0]
    if k == 'D':
        tgt = l.split('|')[1].strip()
        tex = re.findall(r't(\d+) ([0-9A-F]{8})( \d+x\d+ g\d+)?', l.split('|')[3])
        ps = re.search(r'ps ([0-9A-F]+)', l)[1]
        dc = re.search(r'dc ([0-9A-F]+) bl ([0-9A-F]+) m ([0-9A-F]+)', l).groups()
        if cur and cur['tgt'] == tgt:
            cur['n'] += 1; cur['ps'][ps] = cur['ps'].get(ps, 0) + 1
            for t in tex: cur['tex'][t[1]] = t[2].strip()
            cur['last'] = i
        else:
            flush()
            cur = dict(tgt=tgt, n=1, ps={ps: 1}, tex=OrderedDict((t[1], t[2].strip()) for t in tex), first=i, last=i, st=dc)
    else:
        flush(); cur = None
        out.append(l)
flush()
for p in out:
    if isinstance(p, str):
        print('   ', p)
    else:
        pss = ' '.join(f'{k[:8]}x{v}' if v > 1 else k[:8] for k, v in list(p['ps'].items())[:8]) + (' ...' if len(p['ps']) > 8 else '')
        tx = ' '.join(f'{k}({v})' if v else k for k, v in list(p['tex'].items())[:6]) + (f' +{len(p["tex"])-6}' if len(p['tex']) > 6 else '')
        print(f"{p['first']:4}-{p['last']:4} {p['n']:4}d [{p['tgt']}] bl {p['st'][1]} | PS {pss} | T {tx}")
