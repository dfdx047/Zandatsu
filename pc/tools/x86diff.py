#!/usr/bin/env python3
"""x86diff: the game's lifted float functions against the original x86 bytes (Unicorn), with realistic float inputs.

funcdiff.py compares two compilations of the same lifted C, so an error in the lift itself is invisible to it (both sides
agree on the wrong answer). This tool adds the truth: the same function's x86 bytes, from the player's exe, executed
by Unicorn (QEMU's x87 and SSE). For float leaf functions of the lift it runs every (function, input) on

    x86     Unicorn, original bytes                            the reference
    host    the lifted C, host clang -O0, on this PC (x86-64)  lift semantics (a difference here = lift error)
    ref     the lifted C, NDK clang -O0, on the device          ARM vs x86-64 compilation of the same C
    fast    the lifted C, NDK clang GUEST_FAST flags, on device  the optimiser (what libmgrpc.so uses)

The inputs are not random bit patterns (which as floats are mostly huge, tiny or NaN and never exercise a game's
float paths): registers and every 16th dword are pointers into a scratch heap, the rest are floats a game holds --
coordinates up to a few thousand, unit vectors, 0, 1, -1, small angles, the odd large or denormal value.
Inputs come from init-<seed>.bin, written here and read by every side, so all four start from the same bytes.

Each run reports the eight registers, the x87 stack, the XMM file and every dword it changed in memory. Two results are
"same" when equal, "near" when they differ only in float rounding (relative 1e-5), "diff" otherwise.

    x86diff.py [-n 2000] [--seeds 3] [-s serial] [--only sub_X,...] [--cw 0x027F] [--skip-device]

Everything (C, binaries, image, inputs, results) stays under build/lab/pc/x86diff/ and is game code: never commit it.
"""
import argparse, math, os, random, re, struct, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funcdiff  # noqa: E402  (leaves(), image(), paths, flags)

WORK = os.path.join(funcdiff.ROOT, 'build', 'lab', 'pc', 'x86diff')
FLOATY = re.compile(r'g_xmm|g_st\b|g_st\[|fp_push|fp_pop|rdmf32|rdmf64|\bfloat\b|\bdouble\b')
SCRATCH, SCRATCH_SIZE, STACK, STACK_SIZE = 0x30000000, 0x00100000, 0x38000000, 0x00100000
ESP = STACK + STACK_SIZE // 2
SENTINEL = 0x0000F000
MAXCHG = 512
RET_RE = re.compile(r'/\* 0x([0-9A-F]{8}): ret')

HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>

uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp;
#ifndef RECOMP_X87_WINDOW
double g_st[8];
#endif
int g_fp_top; uint16_t g_fpu_cw = 0x027F;
uint16_t g_seg_cs, g_seg_ds, g_seg_es, g_seg_fs, g_seg_gs, g_seg_ss;
uint32_t g_fs_base, g_gs_base, g_cur_func;
ptrdiff_t g_mem_base;
V128 g_xmm[8]; uint64_t g_mm[8]; uint32_t g_mxcsr = 0x1F80;
void recomp_dump_trace(const char *w) { (void)w; }
void recomp_unimpl(uint32_t va, const char *w) { (void)va; (void)w; _exit(97); }
void recomp_hook(uint32_t va) { (void)va; }
int nfs_android_resolution(void) { return 0; }

#define SCRATCH 0x30000000u
#define SCRATCH_SIZE 0x00100000u
#define STACK 0x38000000u
#define STACK_SIZE 0x00100000u
#define IMAGE 0x00400000u
static unsigned char *base, *image0; static size_t image_n;
static uint32_t init_regs[8], init_xmm[32];
static unsigned char *init_mem;   /* scratch then stack */

static uint32_t initial(uint32_t va) {
    uint32_t v = 0;
    if (va >= SCRATCH && va < SCRATCH + SCRATCH_SIZE) memcpy(&v, init_mem + (va - SCRATCH), 4);
    else if (va >= STACK && va < STACK + STACK_SIZE) memcpy(&v, init_mem + SCRATCH_SIZE + (va - STACK), 4);
    else if (va >= IMAGE && va - IMAGE + 4 <= image_n) memcpy(&v, image0 + (va - IMAGE), 4);
    return v;
}

static void run_one(int fi, int seed) {
    alarm(3);
    g_eax = init_regs[0]; g_ecx = init_regs[1]; g_edx = init_regs[2]; g_ebx = init_regs[3];
    g_esp = init_regs[4]; g_ebp = init_regs[5]; g_esi = init_regs[6]; g_edi = init_regs[7];
    g_fp_top = 0;
    for (int i = 0; i < 8; i++) { g_st[i] = 0; for (int k = 0; k < 4; k++) g_xmm[i].u32[k] = init_xmm[i * 4 + k]; }
    /* three x87 arguments, ST(0..2) = xmm7 lanes 0..2 as floats (same on the x86 side) */
    for (int i = 0; i < 3; i++) { float v; memcpy(&v, &init_xmm[28 + i], 4); g_st[i] = v; }
    g_fp_top = 3;
    g_mxcsr = 0x1F80; g_fpu_cw = CW; g_flag_k = g_flag_a = g_flag_b = g_flag_cf = 0;
    FUNCS[fi]();
    printf("R %s %d %08x %08x %08x %08x %08x %08x %08x %08x\n", NAMES[fi], seed,
           g_eax, g_ecx, g_edx, g_ebx, g_esp, g_ebp, g_esi, g_edi);
    printf("F %s %d %d", NAMES[fi], seed, ((g_fp_top % 8) + 8) % 8);
    for (int i = 0; g_fp_top >= 0 && i < g_fp_top && i < 8; i++) printf(" %.17g", g_st[i]);
    printf("\nX %s %d", NAMES[fi], seed);
    for (int i = 0; i < 8; i++) for (int k = 0; k < 4; k++) printf(" %08x", g_xmm[i].u32[k]);
    printf("\nM %s %d", NAMES[fi], seed);
    int pm = open("/proc/self/pagemap", O_RDONLY);
    static uint64_t ent[4096];
    uint64_t first = (uintptr_t)base / 4096; int nchg = 0;
    for (uint64_t p = 0; p < (1ull << 20) + 16; p += 4096) {
        uint64_t n = (1ull << 20) + 16 - p; if (n > 4096) n = 4096;
        if (pread(pm, ent, n * 8, (off_t)((first + p) * 8)) != (ssize_t)(n * 8)) { printf(" PAGEMAP\n"); _exit(0); }
        for (uint64_t i = 0; i < n; i++)
            if ((ent[i] >> 63) & 1 && (ent[i] >> 56) & 1) {
                uint32_t pva = (uint32_t)((p + i) * 4096);
                for (uint32_t o = 0; o < 4096; o += 4) {
                    uint32_t v; memcpy(&v, base + pva + o, 4);
                    if (v != initial(pva + o) && nchg++ < MAXCHG) printf(" %08x:%08x", pva + o, v);
                }
            }
    }
    printf("\n"); fflush(stdout); _exit(0);
}

int main(int argc, char **argv) {
    /* argv: image dir-with-init-files seeds */
    int seeds = atoi(argv[3]);
    base = mmap(NULL, (1ull << 32) + 0x10000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (base == MAP_FAILED) { perror("mmap"); return 1; }
    g_mem_base = (ptrdiff_t)base;
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror("image"); return 1; }
    image_n = fread(base + IMAGE, 1, 0x04000000u, f); fclose(f);
    image0 = malloc(image_n); memcpy(image0, base + IMAGE, image_n);
    init_mem = malloc(SCRATCH_SIZE + STACK_SIZE);
    fprintf(stderr, "image %zu bytes, %d functions x %d seeds\n", image_n, NFUNCS, seeds);
    for (int s = 0; s < seeds; s++) {
        char path[512]; snprintf(path, sizeof path, "%s/init-%d.bin", argv[2], s);
        f = fopen(path, "rb"); if (!f) { perror(path); return 1; }
        if (fread(init_regs, 4, 8, f) != 8 || fread(init_xmm, 4, 32, f) != 32 ||
            fread(init_mem, 1, SCRATCH_SIZE + STACK_SIZE, f) != SCRATCH_SIZE + STACK_SIZE) { fprintf(stderr, "short init\n"); return 1; }
        fclose(f);
        memcpy(base + SCRATCH, init_mem, SCRATCH_SIZE); memcpy(base + STACK, init_mem + SCRATCH_SIZE, STACK_SIZE);
        for (int fi = 0; fi < NFUNCS; fi++) {
            fflush(stdout);
            pid_t pid = fork();
            if (pid == 0) run_one(fi, s);
            int st = 0; waitpid(pid, &st, 0);
            if (WIFSIGNALED(st)) printf("E %s %d SIG%d\n", NAMES[fi], s, WTERMSIG(st));
            else if (WEXITSTATUS(st)) printf("E %s %d EXIT%d\n", NAMES[fi], s, WEXITSTATUS(st));
        }
    }
    return 0;
}
'''


# ---------------------------------------------------------------- inputs
def make_inputs(seeds):
    def f2u(x):
        return struct.unpack('<I', struct.pack('<f', x))[0]

    for s in range(seeds):
        path = os.path.join(WORK, f'init-{s}.bin')
        if os.path.exists(path):
            continue
        r = random.Random(1000 + s)

        def flt():
            k = r.random()
            if k < 0.30: return f2u(r.uniform(-1.0, 1.0))
            if k < 0.50: return f2u(r.uniform(-50.0, 50.0))
            if k < 0.62: return f2u(r.uniform(-3000.0, 3000.0))
            if k < 0.70: return f2u(0.0)
            if k < 0.76: return f2u(1.0)
            if k < 0.79: return f2u(-1.0)
            if k < 0.85: return f2u(r.uniform(-3.2, 3.2))          # angles
            if k < 0.90: return f2u(r.uniform(0.0, 0.05))          # dt, small weights
            if k < 0.93: return r.randrange(0, 64)                 # small ints / indices / flags
            if k < 0.95: return f2u(r.choice([1e-30, -1e-30, 1e20, 3.0e38, 1e-40]))  # edge
            if k < 0.97: return f2u(r.uniform(0.999, 1.001))       # nearly normalised lengths
            return r.getrandbits(32)

        def ptr():
            return SCRATCH + r.randrange(0, (SCRATCH_SIZE - 0x1000) // 16) * 16

        def unit_block():
            # a normalised vec3 + w, or a quaternion: the shape of skeleton data
            v = [r.gauss(0, 1) for _ in range(4)]
            n = math.sqrt(sum(x * x for x in v)) or 1.0
            if r.random() < 0.5:
                return [f2u(x / n) for x in v]
            n3 = math.sqrt(sum(x * x for x in v[:3])) or 1.0
            return [f2u(v[0] / n3), f2u(v[1] / n3), f2u(v[2] / n3), f2u(r.choice([0.0, 1.0]))]

        mem = bytearray()
        for region in range(2):
            words = []
            for i in range(0, SCRATCH_SIZE // 4, 4):
                k = r.random()
                if k < 0.25: blk = [ptr(), flt(), flt(), flt()]
                elif k < 0.55: blk = unit_block()
                else: blk = [flt(), flt(), flt(), flt()]
                words += blk
            mem += struct.pack(f'<{len(words)}I', *words)
        # return-address slot: a sentinel Unicorn stops at (the lifted C never reads it back)
        so = SCRATCH_SIZE + (ESP - STACK)
        mem[so:so + 4] = struct.pack('<I', SENTINEL)
        regs = [ptr(), ptr(), ptr(), ptr(), ESP, ESP + 0x200, ptr(), ptr()]
        for i in (0, 1, 2, 3, 6, 7):
            if r.random() < 0.3:
                regs[i] = r.randrange(0, 64)
        xmm = [flt() for _ in range(32)]
        with open(path, 'wb') as f:
            f.write(struct.pack('<8I', *regs) + struct.pack('<32I', *xmm) + bytes(mem))


def read_init(s):
    d = open(os.path.join(WORK, f'init-{s}.bin'), 'rb').read()
    regs = struct.unpack_from('<8I', d, 0)
    xmm = struct.unpack_from('<32I', d, 32)
    mem = d[160:]
    return regs, xmm, mem


# ---------------------------------------------------------------- unicorn
def to_x80(x):
    if x == 0.0:
        return (0, 0x8000 if math.copysign(1.0, x) < 0 else 0)
    f, e = math.frexp(abs(x))
    return (int(f * (1 << 64)) & 0xFFFFFFFFFFFFFFFF, (e - 1 + 16383) | (0x8000 if x < 0 else 0))


def from_x80(m, e):
    sign = -1.0 if e & 0x8000 else 1.0
    ex = e & 0x7FFF
    if ex == 0x7FFF:
        return float('nan') if (m << 1) & 0xFFFFFFFFFFFFFFFF else sign * float('inf')
    if m == 0:
        return sign * 0.0
    try:
        return sign * math.ldexp(m, ex - 16383 - 63)
    except OverflowError:
        return sign * float('inf')


def run_unicorn(funcs, seeds, cw):
    """funcs: [(name, [ret VAs])]. Stops at any ret of the function (leaf: the first ret ends it) and applies it."""
    from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_MEM_UNMAPPED, UC_HOOK_CODE, UC_PROT_ALL
    from unicorn import x86_const as X
    REGS = [X.UC_X86_REG_EAX, X.UC_X86_REG_ECX, X.UC_X86_REG_EDX, X.UC_X86_REG_EBX, X.UC_X86_REG_ESP,
            X.UC_X86_REG_EBP, X.UC_X86_REG_ESI, X.UC_X86_REG_EDI]
    img = open(os.path.join(WORK, 'image.bin'), 'rb').read()
    img_size = (len(img) + 0xFFF) & ~0xFFF
    img = img + bytes(img_size - len(img))
    out = []
    for s in range(seeds):
        regs0, xmm0, mem0 = read_init(s)
        st0 = [struct.unpack('<f', struct.pack('<I', xmm0[28 + i]))[0] for i in range(3)]
        regions = [(0x00400000, img), (SCRATCH, mem0[:SCRATCH_SIZE]), (STACK, mem0[SCRATCH_SIZE:])]
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        for va, data in regions:
            uc.mem_map(va, len(data), UC_PROT_ALL)
            uc.mem_write(va, data)
        dyn = set()

        def on_unmapped(u, access, addr, size, value, data):
            for pg in {addr >> 12, (addr + max(size, 1) - 1) >> 12}:
                if pg in dyn:
                    continue
                try:
                    u.mem_map(pg << 12, 0x1000, UC_PROT_ALL)
                except UcError:
                    return False
                dyn.add(pg)
            return True

        uc.hook_add(UC_HOOK_MEM_UNMAPPED, on_unmapped)
        state = {}

        def on_ret(u, address, size, data):
            b = bytes(u.mem_read(address, 3))
            imm = struct.unpack_from('<H', b, 1)[0] if b[0] == 0xC2 else 0
            u.reg_write(X.UC_X86_REG_ESP, (u.reg_read(X.UC_X86_REG_ESP) + 4 + imm) & 0xFFFFFFFF)
            state['ret'] = address
            u.emu_stop()

        for name, rets in funcs:
            va = int(name[4:], 16)
            state.clear()
            hooks = [uc.hook_add(UC_HOOK_CODE, on_ret, begin=r, end=r) for r in rets]
            for i, rg in enumerate(REGS):
                uc.reg_write(rg, regs0[i])
            for i in range(8):
                v = xmm0[i * 4] | xmm0[i * 4 + 1] << 32 | xmm0[i * 4 + 2] << 64 | xmm0[i * 4 + 3] << 96
                uc.reg_write(X.UC_X86_REG_XMM0 + i, v)
            uc.reg_write(X.UC_X86_REG_CR0, 0x33)
            uc.reg_write(X.UC_X86_REG_CR4, (1 << 9) | (1 << 10))
            uc.reg_write(X.UC_X86_REG_EFLAGS, 0x2)
            uc.reg_write(X.UC_X86_REG_MXCSR, 0x1F80)
            uc.reg_write(X.UC_X86_REG_FPCW, cw)
            uc.reg_write(X.UC_X86_REG_FPSW, 5 << 11)
            uc.reg_write(X.UC_X86_REG_FPTAG, 0x03FF)
            for i in range(3):
                uc.reg_write(X.UC_X86_REG_FP0 + ((5 + i) & 7), to_x80(st0[i]))
            err = None
            try:
                uc.emu_start(va, 0xFFFFFFFF, timeout=2_000_000, count=1_000_000)
            except UcError as e:
                err = str(e)
            for h in hooks:
                uc.hook_del(h)
            if err or 'ret' not in state:
                out.append(f'E {name} {s} UC:{err or "no ret (eip %08x)" % uc.reg_read(X.UC_X86_REG_EIP)}')
            else:
                out.append(f'R {name} {s} ' + ' '.join(f'{uc.reg_read(rg):08x}' for rg in REGS))
                top = (uc.reg_read(X.UC_X86_REG_FPSW) >> 11) & 7
                depth = (8 - top) % 8
                vals = [repr(from_x80(*uc.reg_read(X.UC_X86_REG_FP0 + ((top + i) & 7)))) for i in range(depth)]
                out.append(f'F {name} {s} {depth}' + ''.join(' ' + v for v in vals))
                xs = []
                for i in range(8):
                    v = uc.reg_read(X.UC_X86_REG_XMM0 + i)
                    xs += [f'{(v >> (32 * k)) & 0xFFFFFFFF:08x}' for k in range(4)]
                out.append(f'X {name} {s} ' + ' '.join(xs))
            chg = []
            for base, ini in regions:
                cur = bytes(uc.mem_read(base, len(ini)))
                if cur == ini:
                    continue
                for o in range(0, len(ini), 4096):
                    if cur[o:o + 4096] != ini[o:o + 4096]:
                        a = struct.unpack('<1024I', cur[o:o + 4096]); b = struct.unpack('<1024I', ini[o:o + 4096])
                        chg += [f'{base + o + 4 * k:08x}:{a[k]:08x}' for k in range(1024) if a[k] != b[k]]
                        uc.mem_write(base + o, ini[o:o + 4096])
            for pg in sorted(dyn):
                cur = struct.unpack('<1024I', bytes(uc.mem_read(pg << 12, 4096)))
                chg += [f'{(pg << 12) + 4 * k:08x}:{cur[k]:08x}' for k in range(1024) if cur[k]]
                uc.mem_unmap(pg << 12, 0x1000)
            dyn.clear()
            if not (err or 'ret' not in state):
                out.append(f'M {name} {s}' + ''.join(' ' + c for c in sorted(chg)[:MAXCHG]))
    return out


# ---------------------------------------------------------------- compare
def parse(lines):
    res = {}
    for line in lines:
        p = line.split()
        if len(p) < 3 or p[0] not in 'RFXME':
            continue
        res.setdefault((p[1], int(p[2])), {})[p[0]] = p[3:]
    return res


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


def close_bits(a, b):
    """dword a vs b: 'same', 'near' (float rounding), 'diff'."""
    if a == b:
        return 'same'
    fa, fb = u2f(a), u2f(b)
    if math.isnan(fa) or math.isnan(fb) or math.isinf(fa) or math.isinf(fb):
        return 'diff'
    if abs(fa - fb) <= 1e-5 * max(abs(fa), abs(fb), 1e-30) or abs(fa - fb) < 1e-35:
        return 'near'
    return 'diff'


def close_double(a, b):
    fa, fb = float(a), float(b)
    if fa == fb or (math.isnan(fa) and math.isnan(fb)):
        return 'same'
    if math.isnan(fa) or math.isnan(fb) or math.isinf(fa) or math.isinf(fb):
        return 'diff'
    if abs(fa - fb) <= 1e-6 * max(abs(fa), abs(fb)):
        return 'near'
    return 'diff'


def compare(t, o):
    """t = truth record, o = other. Returns (verdict, detail)."""
    if 'E' in t or 'E' in o:
        if 'E' in t and 'E' in o:
            return 'skip', ''
        return 'skip1', ('truth ' + ' '.join(t.get('E', ['ok'])) + ' / other ' + ' '.join(o.get('E', ['ok'])))
    worst, det = 'same', []

    def note(v, what):
        nonlocal worst
        if v == 'diff':
            worst = 'diff'; det.append(what)
        elif v == 'near' and worst == 'same':
            worst = 'near'
    names = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
    for i, (a, b) in enumerate(zip(t['R'], o['R'])):
        note(close_bits(int(a, 16), int(b, 16)), f'{names[i]} {a}/{b}')
    ft, fo = t['F'], o['F']
    if ft[0] != fo[0]:
        note('diff', f'x87 depth {ft[0]}/{fo[0]}')
    else:
        for i, (a, b) in enumerate(zip(ft[1:], fo[1:])):
            note(close_double(a, b), f'st{i} {a}/{b}')
    for i, (a, b) in enumerate(zip(t['X'], o['X'])):
        note(close_bits(int(a, 16), int(b, 16)), f'xmm{i // 4}.{i % 4} {u2f(int(a, 16)):.7g}/{u2f(int(b, 16)):.7g}')
    mt = dict(x.split(':') for x in t['M'])
    mo = dict(x.split(':') for x in o['M'])
    for addr in sorted(set(mt) | set(mo)):
        a, b = mt.get(addr), mo.get(addr)
        if a is None or b is None:
            note('diff', f'[{addr}] {a}/{b}')
        else:
            note(close_bits(int(a, 16), int(b, 16)), f'[{addr}] {a}={u2f(int(a, 16)):.7g} / {b}={u2f(int(b, 16)):.7g}')
    return worst, '; '.join(det[:6])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-n', type=int, default=2000)
    ap.add_argument('--seeds', type=int, default=3)
    ap.add_argument('-s', '--serial', default='192.168.1.206:5555')
    ap.add_argument('--only', default='')
    ap.add_argument('--cw', default='0x027F', help='x87 control word for the x86 side (0x007F = D3D9 single precision)')
    ap.add_argument('--skip-device', action='store_true')
    ap.add_argument('-j', '--jobs', type=int, default=4)
    ap.add_argument('--reuse', default='', help='comma list of sides whose out-<side>.txt is reused')
    a = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)
    cw = int(a.cw, 16)
    reuse = set(a.reuse.split(',')) if a.reuse else set()
    allf = [f for f in funcdiff.leaves() if FLOATY.search(f[1])]
    if a.only:
        want = set(a.only.split(','))
        pick = [f for f in allf if f[0] in want]
    else:
        random.Random(23).shuffle(allf)
        pick = sorted(allf[:a.n])
    print(f'{len(allf)} float leaf functions, testing {len(pick)} x {a.seeds} inputs', flush=True)
    make_inputs(a.seeds)
    img = os.path.join(WORK, 'image.bin')
    if not os.path.exists(img):
        open(img, 'wb').write(funcdiff.image(funcdiff.EXE))
    src = os.path.join(WORK, 'x86diff.c')
    with open(src, 'w') as f:
        f.write('#include "recomp_funcs.h"\n#include <math.h>\n#include <string.h>\n')
        f.write(f'#define MAXCHG {MAXCHG}\n#define CW 0x{cw:04X}\n')
        for _, body in pick:
            f.write(body + '\n')
        f.write(f'#define NFUNCS {len(pick)}\n')
        f.write('static void (*const FUNCS[])(void) = {' + ','.join(n for n, _ in pick) + '};\n')
        f.write('static const char *const NAMES[] = {' + ','.join(f'"{n}"' for n, _ in pick) + '};\n')
        f.write(HARNESS)
    names = [n for n, _ in pick]
    outs = {}

    def cached(side):
        p = os.path.join(WORK, f'out-{side}.txt')
        if side in reuse and os.path.exists(p):
            return open(p).read().splitlines()
        return None

    # x86 truth
    lines = cached('x86')
    if lines is None:
        rets = {n: [int(x, 16) for x in RET_RE.findall(body)] for n, body in pick}
        import multiprocessing
        work = [(n, rets[n]) for n in names]
        chunks = [work[i::a.jobs] for i in range(a.jobs)]
        with multiprocessing.Pool(a.jobs) as pool:
            parts = pool.starmap(run_unicorn, [(c, a.seeds, cw) for c in chunks])
        lines = [l for part in parts for l in part]
        open(os.path.join(WORK, 'out-x86.txt'), 'w').write('\n'.join(lines) + '\n')
    outs['x86'] = parse(lines)
    print(f'x86: {len(outs["x86"])} runs', flush=True)
    # host lifted C
    lines = cached('host')
    if lines is None:
        exe = os.path.join(WORK, 'x86diff-host')
        r = subprocess.run(['clang', '-O0', *funcdiff.COMMON, src, '-o', exe, '-lm'], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('host clang failed\n' + r.stderr[-3000:])
        r = subprocess.run([exe, img, WORK, str(a.seeds)], capture_output=True, text=True)
        lines = r.stdout.splitlines()
        open(os.path.join(WORK, 'out-host.txt'), 'w').write(r.stdout)
    outs['host'] = parse(lines)
    print(f'host: {len(outs["host"])} runs', flush=True)
    if not a.skip_device:
        adb = ['adb', '-s', a.serial]
        dev = '/data/local/tmp/mgr-xd'
        subprocess.run([*adb, 'shell', f'mkdir -p {dev}'], check=True)
        pushed = False
        for v, flags in funcdiff.VARIANTS.items():
            lines = cached(v)
            if lines is None:
                if not pushed:
                    subprocess.run([*adb, 'push', img, *[os.path.join(WORK, f'init-{s}.bin') for s in range(a.seeds)],
                                    dev + '/'], check=True, capture_output=True)
                    pushed = True
                exe = os.path.join(WORK, f'x86diff-{v}')
                r = subprocess.run([funcdiff.CLANG, *flags, *funcdiff.COMMON, src, '-o', exe, '-static', '-lm'],
                                   capture_output=True, text=True)
                if r.returncode:
                    raise SystemExit(f'{v}: clang failed\n{r.stderr[-3000:]}')
                subprocess.run([*adb, 'push', exe, f'{dev}/{v}'], check=True, capture_output=True)
                r = subprocess.run([*adb, 'shell', f'chmod 755 {dev}/{v} && {dev}/{v} {dev}/image.bin {dev} {a.seeds}'],
                                   capture_output=True, text=True)
                lines = r.stdout.splitlines()
                open(os.path.join(WORK, f'out-{v}.txt'), 'w').write(r.stdout)
            outs[v] = parse(lines)
            print(f'{v}: {len(outs[v])} runs', flush=True)
        subprocess.run([*adb, 'shell', f'rm -rf {dev}'], capture_output=True)
    # report: every side against x86, and fast against ref
    rep = []
    pairs = [('x86', s) for s in outs if s != 'x86'] + ([('ref', 'fast')] if 'fast' in outs else [])
    for tside, oside in pairs:
        cnt = {'same': 0, 'near': 0, 'diff': 0, 'skip': 0, 'skip1': 0}
        bad = {}
        for k, t in outs[tside].items():
            o = outs[oside].get(k)
            if o is None:
                cnt['skip'] += 1
                continue
            v, d = compare(t, o)
            cnt[v] += 1
            if v in ('diff', 'skip1'):
                bad.setdefault(k[0], []).append(f'seed {k[1]} {v}: {d}')
        line = f'{oside} vs {tside}: ' + ', '.join(f'{k} {v}' for k, v in cnt.items()) + f'; functions {len(bad)}'
        print(line, flush=True)
        rep.append('== ' + line)
        for fn in sorted(bad):
            rep.append(fn)
            rep += ['   ' + x for x in bad[fn]]
    open(os.path.join(WORK, 'report.txt'), 'w').write('\n'.join(rep) + '\n')
    print('report:', os.path.join(WORK, 'report.txt'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
