#!/usr/bin/env python3
"""insnfuzz: every float/SIMD instruction form of the game, lifted alone, against Unicorn, with many realistic inputs.

x86diff.py tests whole leaf functions, so an instruction that only goes wrong for some inputs (NaN, -0, a lane the
function's inputs never fill) can hide behind the function's own control flow. This tests each instruction FORM the
catalogued code contains (insnforms.py: memory operands rewritten to [abs32]) in isolation, through pcrecomp's own
difftest machinery (tools/lift/difftest.py: Unicorn reference, lifted C at -O0, every written byte compared):

    prologue   movups xmm0..7 <- inputs; fld x n (x87 forms); movq mm0..7 <- inputs (MMX forms)
    the form   its memory operand reads/writes a 32-byte input block
    epilogue   movups -> out; fnstsw -> out; fstp x depth (depth read from Unicorn first); movq mm -> out; emms

GPRs start from random values (ESP excepted) and are compared; EFLAGS only for forms that define them (comis/fcomi).
A byte difference is classified: "near" (float32/float64 lanes within 1e-6 relative, 4e-3 for rcp/rsqrt and the
x87 transcendentals), "nan" (both NaN, sign or payload differ) or "diff".

    insnfuzz.py <forms.json> [--seeds 8] [--only mnem,..] [--batch 1500] [-j 4]

Writes build/lab/pc/insnfuzz/report.txt. Game bytes stay under build/lab/ (never commit).
"""
import argparse, json, math, os, random, struct, sys, zlib, multiprocessing as mp

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
PCRECOMP = os.environ.get('PCRECOMP', os.path.join(ROOT, 'build', 'lab', 'pc', 'pcrecomp-pinned'))
sys.path.insert(0, PCRECOMP)
from tools.lift import difftest as DT   # noqa: E402
from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32  # noqa: E402
from unicorn import x86_const as X  # noqa: E402

WORK = os.path.join(ROOT, 'build', 'lab', 'pc', 'insnfuzz')
S = DT.SCRATCH
XMMIN, STIN, MMIN, OPMEM = S + 0x000, S + 0x080, S + 0x0C0, S + 0x200
OUT_X, OUT_ST, OUT_SW, OUT_MM = S + 0x400, S + 0x480, S + 0x4C0, S + 0x4D0
MARK = 0x30000200
CMP_X87 = ('fcom', 'fucom', 'ftst', 'fxam', 'fprem', 'ficom')
FLAGS_DEF = ('comiss', 'ucomiss', 'comisd', 'ucomisd', 'fcomi', 'fucomi', 'fcompi', 'fucompi', 'fcomip', 'fucomip')
LOOSE = ('rcp', 'rsqrt', 'fsin', 'fcos', 'fptan', 'fpatan', 'f2xm1', 'fyl2x', 'fscale', 'fprem', 'fsincos')


def a32(v):
    return struct.pack('<I', v)


def rnd_f(r):
    k = r.random()
    if k < 0.10: return r.choice([0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0])
    if k < 0.35: return r.uniform(-1, 1)
    if k < 0.60: return r.uniform(-2000, 2000)
    if k < 0.70: return r.uniform(-1e6, 1e6)
    if k < 0.78: return r.choice([1, -1]) * 10 ** r.uniform(-8, -2)
    if k < 0.83: return float(r.randint(-300, 300))
    if k < 0.88: return r.choice([float('inf'), float('-inf'), float('nan'), -float('nan')])
    if k < 0.92: return r.choice([1e-40, -1e-40, 1e-39, 3e38, -3e38, 1e30, 1e-30])
    if k < 0.96: return r.uniform(0, 6.3)
    return r.uniform(-50, 50)


def f32b(v):
    try:
        return struct.pack('<f', v)
    except OverflowError:
        return struct.pack('<f', math.copysign(float('inf'), v))


def lane_bytes(r, n, mode):
    out = b''
    while len(out) < n:
        m = mode if mode != 'mix' else r.choice(['f32', 'f32', 'f32', 'f64', 'int'])
        if m == 'f32':
            out += f32b(rnd_f(r))
        elif m == 'f64':
            out += struct.pack('<d', rnd_f(r))
        else:
            out += a32(r.choice([r.getrandbits(32), r.randint(0, 255), r.randint(-40000, 40000) & 0xFFFFFFFF,
                                 0x80000000, 0x7FFFFFFF, 0xFFFFFFFF, 0]))
    return out[:n]


def mode_for(m):
    if m.endswith(('sd', 'pd')) and not m.startswith('movsd') or 'pd' in m or m.startswith('cvtsd') or 'sd2' in m:
        return 'f64'
    if m.startswith('p') or m in ('movd', 'movq', 'cvtdq2ps', 'cvtpi2ps', 'cvtsi2ss', 'cvtsi2sd'):
        return 'int'
    return 'f32'


def max_st(op_str):
    import re
    ix = [int(x) for x in re.findall(r'st\((\d)\)', op_str)]
    return max(ix) if ix else 0


def build(form, seed, depth_after=None):
    """(code, mem, regs) for one form and seed; epilogue fstp count = depth_after (None: no x87 stores)."""
    r = random.Random(zlib.crc32(f"{form['bytes']}/{seed}".encode()))
    m = form['mnemonic']
    mode = mode_for(m)
    xin = b''.join(lane_bytes(r, 16, mode if r.random() < 0.7 else 'mix') for _ in range(8))
    stv = [rnd_f(r) for _ in range(8)]
    stin = struct.pack('<8d', *stv)
    mmin = lane_bytes(r, 64, 'int')
    opm = lane_bytes(r, 32, 'f64' if m.startswith(('fld', 'fadd', 'fmul', 'fsub', 'fdiv', 'fcom')) and 'qword' in form['op_str']
                     else ('int' if m.startswith('fi') else mode))
    code = b''
    for i in range(8):
        code += bytes([0x0F, 0x10, (i << 3) | 5]) + a32(XMMIN + 16 * i)
    nst = 0
    if form['x87']:
        nst = min(7, max(3, max_st(form['op_str']) + 1))
        for k in reversed(range(nst)):
            code += bytes([0xDD, 0x05]) + a32(STIN + 8 * k)
    if form['mmx']:
        for i in range(8):
            code += bytes([0x0F, 0x6F, (i << 3) | 5]) + a32(MMIN + 8 * i)
    ib = bytes.fromhex(form['bytes'])
    mk = a32(MARK)
    if mk in ib:
        ib = ib.replace(mk, a32(OPMEM), 1)
    pre = len(code)
    code += ib
    post = len(code)
    for i in range(8):
        code += bytes([0x0F, 0x11, (i << 3) | 5]) + a32(OUT_X + 16 * i)
    if form['x87']:
        code += bytes([0xDD, 0x3D]) + a32(OUT_SW)
        for k in range(depth_after or 0):
            code += bytes([0xDD, 0x1D]) + a32(OUT_ST + 8 * k)
    if form['mmx']:
        for i in range(8):
            code += bytes([0x0F, 0x7F, (i << 3) | 5]) + a32(OUT_MM + 8 * i)
        code += bytes([0x0F, 0x77])
    regs = {n: r.choice([r.getrandbits(32), r.randint(0, 1000), struct.unpack('<I', f32b(rnd_f(r)))[0]])
            for n in ('eax', 'ecx', 'edx', 'ebx', 'ebp', 'esi', 'edi')}
    mem = {XMMIN: xin, STIN: stin, MMIN: mmin, OPMEM: opm}
    return code, mem, regs, (pre, post)


def mk_case(form, seed, depth_after):
    code, mem, regs, _ = build(form, seed, depth_after)
    undef = () if form['mnemonic'] in FLAGS_DEF else tuple(n for n, _ in DT.FLAGS)
    return DT.Case(f"{form['mnemonic']}#{form['idx']}#{seed}", code, regs=regs, mem=mem, undef=undef,
                   precise_carry=True)


def uc_depth(args):
    form, seed = args
    code, mem, regs, (pre, post) = build(form, seed, None)
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(DT.BASE, DT.SIZE)
    mu.mem_write(DT.BASE, code)
    for va, d in mem.items():
        mu.mem_write(va, d)
    st = dict(DT.DEFAULT_REGS); st.update(regs)
    for n, v in st.items():
        mu.reg_write(DT.UC_REGS[n], v)
    mu.reg_write(X.UC_X86_REG_EFLAGS, DT.EFLAGS_START)
    mu.reg_write(X.UC_X86_REG_FPCW, DT.FPU_CONTROL_WORD)
    mu.reg_write(X.UC_X86_REG_MXCSR, 0x1F80)
    try:
        mu.emu_start(DT.BASE, DT.BASE + post, count=200)
    except Exception as e:
        return None
    return DT.fpu_depth(mu)


def uc_run(case):
    try:
        return DT.run_unicorn(case)
    except Exception as e:
        return {'error': str(e)}


def byte_at(res, case, va):
    if va in res['mem']:
        return res['mem'][va]
    for base, d in case.mem.items():
        if base <= va < base + len(d):
            return d[va - base]
    return 0


def near(a, b, tol):
    if math.isnan(a) and math.isnan(b): return 'nan'
    if a == b: return 'same'
    if math.isinf(a) or math.isinf(b): return None
    return 'near' if abs(a - b) <= tol * max(abs(a), abs(b), 1e-30) else None


def classify(form, fields):
    """fields from DT.diff -> (verdict, details). Memory bytes are regrouped into 4/8-byte lanes."""
    mem = {}
    other = []
    for name, a, b in fields:
        if name.startswith('['):
            mem[int(name[1:-1], 16)] = (a, b)
        else:
            other.append((name, a, b))
    tol = 4e-3 if form['mnemonic'].startswith(LOOSE) else 1e-6
    verdict = 'diff' if other else None
    lanes = []
    seen = set()
    for addr in sorted(mem):
        if OUT_SW <= addr < OUT_SW + 2:
            continue                       # fnstsw handled below
        w = 8 if OUT_ST <= addr < OUT_SW else 4
        base = addr - ((addr - S) % w)
        if base in seen:
            continue
        seen.add(base)
        lanes.append((base, w))
    return verdict, other, lanes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('forms')
    ap.add_argument('--seeds', type=int, default=8)
    ap.add_argument('--only')
    ap.add_argument('--batch', type=int, default=1500)
    ap.add_argument('-j', type=int, default=4)
    ap.add_argument('--top', type=int, default=0, help='only the N most used forms')
    ap.add_argument('--window', action='store_true', help='RECOMP_X87_WINDOW storage (compile with RECOMP_CC=cc-fast.sh)')
    a = ap.parse_args()
    if a.window:
        DT.PRELUDE = DT.PRELUDE.replace('double   g_st[8];', '#ifndef RECOMP_X87_WINDOW\ndouble   g_st[8];\n#endif')
        DT.PRELUDE = DT.PRELUDE.replace('#include "recomp_types.h"', '#include <math.h>\n#include "recomp_types.h"')
    forms = json.load(open(a.forms))
    for i, f in enumerate(forms):
        f['idx'] = i
    if a.only:
        want = set(a.only.split(','))
        forms = [f for f in forms if f['mnemonic'] in want]
    if a.top:
        forms = forms[:a.top]
    global WORK
    if a.window:
        WORK = os.path.join(WORK, 'window')
    os.makedirs(WORK, exist_ok=True)
    keep = []
    for f in forms:   # forms the lifter refuses (RECOMP_UNIMPL) would stop the whole batch: listed, not run
        c = mk_case(f, 0, 0)
        if any('RECOMP_UNIMPL' in ln for ln in DT.lift_case(c)):
            print(f"UNIMPL {f['mnemonic']} {f['op_str']} ({f['count']} uses, e.g. 0x{f['va']:08X})")
        else:
            keep.append(f)
    forms = keep
    jobs = [(f, s) for f in forms for s in range(a.seeds)]
    with mp.Pool(a.j) as pool:
        depths = pool.map(uc_depth, jobs, chunksize=64)
    cases, meta = [], []
    for (f, s), d in zip(jobs, depths):
        if d is None:
            continue
        cases.append(mk_case(f, s, d if f['x87'] else 0))
        meta.append(f)
    print(f'{len(forms)} forms, {len(cases)} cases (of {len(jobs)})', flush=True)
    with mp.Pool(a.j) as pool:
        reals = pool.map(uc_run, cases, chunksize=32)
    lifted = []
    for b0 in range(0, len(cases), a.batch):
        wd = os.path.join(WORK, f'b{b0 // a.batch:03d}')
        os.makedirs(wd, exist_ok=True)
        lifted += DT.run_lifted(cases[b0:b0 + a.batch], wd, True)
        print(f'  lifted {len(lifted)}/{len(cases)}', flush=True)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    stats = {}
    rep = []
    for case, f, got, want in zip(cases, meta, lifted, reals):
        key = f['idx']
        st = stats.setdefault(key, {'form': f, 'same': 0, 'near': 0, 'nan': 0, 'diff': 0, 'err': 0, 'ex': []})
        if 'error' in want:
            st['err'] += 1
            continue
        fields = DT.diff(case, got, want)
        # fnstsw: TOP always, C0/C2/C3 only for compares (C1 and the rest are not modelled)
        swm = 0x3800 | (0x4700 if f['mnemonic'].startswith(CMP_X87) else 0)
        sw_l = got['mem'].get(OUT_SW, 0) | (got['mem'].get(OUT_SW + 1, 0) << 8)
        sw_r = want['mem'].get(OUT_SW, 0) | (want['mem'].get(OUT_SW + 1, 0) << 8)
        verdict, other, lanes = classify(f, fields)
        if f['x87'] and (sw_l ^ sw_r) & swm:
            other.append(('fsw', f'{sw_l:04X}', f'{sw_r:04X}'))
            verdict = 'diff'
        tol = 4e-3 if f['mnemonic'].startswith(LOOSE) else 1e-6
        worst = verdict or 'same'
        details = list(other)
        for base, w in lanes:
            def get(res):
                bs = bytes(res['mem'].get(base + i, 0) for i in range(w))
                return bs
            # value before = whatever the window held; mem dict only has changed bytes, so rebuild from both
            lb = bytes(byte_at(got, case, base + i) for i in range(w))
            rb = bytes(byte_at(want, case, base + i) for i in range(w))
            if lb == rb:
                continue
            fmt = '<d' if w == 8 else '<f'
            lv, rv = struct.unpack(fmt, lb)[0], struct.unpack(fmt, rb)[0]
            c = near(lv, rv, tol)
            if c is None and w == 4 and base % 8 == (S % 8):
                # maybe the low half of a double lane
                lb8 = bytes(byte_at(got, case, base + i) for i in range(8))
                rb8 = bytes(byte_at(want, case, base + i) for i in range(8))
                c2 = near(struct.unpack('<d', lb8)[0], struct.unpack('<d', rb8)[0], tol)
                if c2 in ('near', 'nan'):
                    c = c2
            if c is None and w == 4 and (base - S) % 8 == 4:
                lb8 = bytes(byte_at(got, case, base - 4 + i) for i in range(8))
                rb8 = bytes(byte_at(want, case, base - 4 + i) for i in range(8))
                c2 = near(struct.unpack('<d', lb8)[0], struct.unpack('<d', rb8)[0], tol)
                if c2 in ('near', 'nan'):
                    c = c2
            c = c or 'diff'
            details.append((f'{base:08X}', f'{lb.hex()}={lv:.9g}', f'{rb.hex()}={rv:.9g}', c))
            rank = {'same': 0, 'near': 1, 'nan': 2, 'diff': 3}
            if rank[c] > rank[worst]:
                worst = c
        st[worst] += 1
        if worst in ('diff', 'nan') and len(st['ex']) < 3:
            ins = DT.Case  # noqa
            code, mem, regs, (pre, post) = build(f, int(case.name.split('#')[2]), None)
            st['ex'].append({'seed': case.name, 'kind': worst, 'details': details,
                             'opm': mem[OPMEM].hex(), 'xin': mem[XMMIN].hex(), 'stin': list(struct.unpack('<8d', mem[STIN])),
                             'regs': {k: f'{v:08X}' for k, v in regs.items()}})
    tot = {'same': 0, 'near': 0, 'nan': 0, 'diff': 0, 'err': 0}
    out = []
    for key, st in sorted(stats.items(), key=lambda kv: (-kv[1]['diff'], -kv[1]['nan'], -kv[1]['form']['count'])):
        for k in tot:
            tot[k] += st[k]
        if st['diff'] or st['nan']:
            f = st['form']
            out.append(f"{'DIFF' if st['diff'] else 'NAN '} {f['mnemonic']} {f['op_str']}  (form {key}, {f['count']} uses, "
                       f"e.g. 0x{f['va']:08X})  same {st['same']} near {st['near']} nan {st['nan']} diff {st['diff']} err {st['err']}")
            for ex in st['ex']:
                out.append(f"    {ex['seed']} {ex['kind']}")
                for d in ex['details'][:6]:
                    out.append('        ' + '  '.join(str(x) for x in d))
                out.append(f"        regs {ex['regs']}")
                out.append(f"        opm {ex['opm'][:64]}  st {['%.6g' % v for v in ex['stin'][:4]]}")
    head = f"forms {len(stats)}  cases same {tot['same']} near {tot['near']} nan {tot['nan']} diff {tot['diff']} err {tot['err']}"
    open(os.path.join(WORK, 'report.txt'), 'w').write(head + '\n' + '\n'.join(out) + '\n')
    print(head)
    print('\n'.join(out[:200]))


if __name__ == '__main__':
    main()
