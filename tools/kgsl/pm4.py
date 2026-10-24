# Descodifica IBs de um snapshot do kgsl (a6xx): pacotes type4 (escrita de registos) e type7 (opcodes do adreno_pm4.xml).
import os, re, struct, sys
# XML dos registos do Mesa (src/freedreno/registers/adreno): MESA_XML=<pasta com adreno_pm4.xml e a6xx.xml>
X = os.environ.get('MESA_XML', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'xml')) + '/'
pm4 = open(X + 'adreno_pm4.xml').read()
ops = {}
m = re.search(r'<enum name="adreno_pm4_type3_packets"[^>]*>(.*?)</enum>', pm4, re.S)
for n, v, attrs in re.findall(r'<value name="(\w+)" value="(\w+)"([^>]*)>', m.group(1)):
    if 'variants' in attrs and 'A6XX' not in attrs and 'A5XX-' not in attrs and 'A4XX-' not in attrs:
        if int(v, 0) not in ops: ops.setdefault(int(v, 0), n + '?')
        continue
    ops[int(v, 0)] = n
a6 = open(X + 'a6xx.xml').read()
dom = a6[a6.index('<domain name="A6XX"'):]
dom = dom[:dom.index('</domain>')]
regs = {}
for kind, off, name in re.findall(r'<(reg32|reg64)\s+offset="(\w+)"\s+name="(\w+)"', dom):
    regs.setdefault(int(off, 0), name)
for off, name, stride, length in re.findall(r'<array\s+offset="(\w+)"\s+name="(\w+)"\s+stride="(\w+)"\s+length="(\w+)"', dom):
    for i in range(int(length, 0)):
        for j in range(int(stride, 0)):
            regs.setdefault(int(off, 0) + i * int(stride, 0) + j, f'{name}[{i}]+{j}')

def sections(d):
    off = 12
    while off + 8 <= len(d):
        mg, sid, size = struct.unpack_from('<HHI', d, off)
        if mg != 0xABCD or size == 0: break
        yield off, sid, size
        if sid == 0xFFFF: break
        off += size

def ibs(d):
    out = {}
    for off, sid, size in sections(d):
        if sid == 0x0402:
            ga, pt, sz = struct.unpack_from('<QQQ', d, off + 8)
            out[ga] = struct.unpack_from(f'<{sz}I', d, off + 8 + 24)
    return out

def decode(words, base, start=0, end=None, mark=None):
    i = 0; end = len(words) if end is None else end
    while i < len(words):
        h = words[i]; t = h >> 28
        if t == 7:
            cnt = h & 0x3fff; op = (h >> 16) & 0x7f
            payload = words[i + 1:i + 1 + cnt]
            name = ops.get(op, f'op{op:#x}')
            if start <= i < end:
                tag = '>>' if mark is not None and i <= mark < i + 1 + cnt else '  '
                extra = ''
                if name.startswith('CP_INDIRECT_BUFFER') and len(payload) >= 3:
                    extra = f' -> {payload[0] | payload[1] << 32:#x} size {payload[2] & 0xfffff}'
                if name == 'CP_SET_MARKER' and payload: extra = f' mode {payload[0]:#x}'
                print(f'{tag}{i:6d} {base + 4 * i:#x}  {name}({cnt}) ' + ' '.join(f'{w:08x}' for w in payload[:8]) + extra)
            i += 1 + cnt
        elif t == 4:
            cnt = h & 0x7f; reg = (h >> 8) & 0x3ffff
            if start <= i < end:
                tag = '>>' if mark is not None and i <= mark < i + 1 + cnt else '  '
                vals = words[i + 1:i + 1 + cnt]
                desc = ', '.join(f'{regs.get(reg + k, hex(reg + k))}={v:#x}' for k, v in enumerate(vals[:6]))
                print(f'{tag}{i:6d} {base + 4 * i:#x}  REG {desc}' + (' ...' if cnt > 6 else ''))
            i += 1 + cnt
        else:
            if start <= i < end:
                print(f'  {i:6d} {base + 4 * i:#x}  ?? {h:08x}')
            i += 1

if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    for ga, w in ibs(d).items():
        print(f'IB {ga:#x}: {len(w)} dwords')
    args = sys.argv[2:]
    if args:
        ga = int(args[0], 0); s = int(args[1]); e = int(args[2]); mark = int(args[3]) if len(args) > 3 else None
        decode(ibs(d)[ga], ga, s, e, mark)
