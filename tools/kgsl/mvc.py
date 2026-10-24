# Registos de contexto (secções MVC 0x1501 do snapshot a6xx): {ctxt: {reg: valor}}.
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pm4
def mvc(d):
    out = {}
    for off, sid, size in pm4.sections(d):
        if sid != 0x1501: continue
        w = struct.unpack_from(f'<{(size - 8) // 4}I', d, off + 8)
        ctxt, cluster = w[0], w[1]
        regs = out.setdefault(ctxt, {})
        i = 2
        while i + 1 < len(w):
            a, b = w[i], w[i + 1]
            if not (a & 0x80000000): break
            start, end = a & 0x3ffff, b
            if end < start or end - start > 4096: break
            vals = w[i + 2:i + 2 + end - start + 1]
            for k, v in enumerate(vals): regs[start + k] = v
            i += 2 + end - start + 1
    return out
if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    m = mvc(d)
    want = sys.argv[2:]
    for ctxt, regs in sorted(m.items()):
        print('contexto', ctxt, len(regs), 'registos')
        for off, v in sorted(regs.items()):
            n = pm4.regs.get(off, hex(off))
            if any(n.startswith(x) for x in want): print(f'  {off:#06x} {n} = {v:#x}')
