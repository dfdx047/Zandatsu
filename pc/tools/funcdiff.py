#!/usr/bin/env python3
"""funcdiff: A/B differential test of the game's own lifted functions, -O0 reference against the shipped flags.

difftest-android.py proves the instruction semantics under the Android compiler and flags; this proves the step that
the per-instruction cases cannot see: the optimiser working on whole lifted bodies (register locals across a body,
non-volatile memory, the x87 window). For a sample of leaf functions of the lift (no calls, no indirect jumps, no host
hooks) it builds two device binaries from the same C:

    ref   -O0, globals, volatile guest memory, plain x87 stack   (the upstream pcrecomp default)
    fast  -O2 + RECOMP_LOCAL_REGS + RECOMP_X87_WINDOW + RECOMP_MEM_QUAL=   (GUEST_FAST, what libmgrpc.so uses)

Each binary maps a full 4 GB guest space, loads the game image at its base (from the player's exe, pushed to the
device's /data/local/tmp only for the run), fills a scratch heap and a stack with seeded data and pointers into
themselves, and runs every function in a forked child per seed. The child hashes the eight registers, the published
flags, the x87 stack and the XMM file, and every guest page it wrote (pages exclusive to the child in
/proc/self/pagemap). Same hash in both binaries = same behaviour. A differing function is reported by address; the fix
is to build that function without optimisation (pc/config/guest_O0.txt).

    funcdiff.py [-n 3000] [--seeds 3] [-s serial] [--only sub_XXXXXXXX,...]

Everything (C, binaries, image, results) stays under build/lab/pc/funcdiff/ and is game code: never commit it.
"""
import argparse, glob, os, random, re, struct, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
GEN = os.path.join(ROOT, 'build', 'pc', 'gen')
INC = os.path.join(ROOT, 'build', 'lab', 'pc', 'pcrecomp-pinned', 'runtime', 'recomp32')
RT = os.path.join(ROOT, 'pc', 'runtime')
WORK = os.path.join(ROOT, 'build', 'lab', 'pc', 'funcdiff')
EXE = os.path.join(ROOT, 'build', 'lab', 'pc', 'gameroot', 'METAL GEAR RISING REVENGEANCE.exe')
NDK = os.environ.get('ANDROID_NDK', os.path.expanduser('~/Android/Sdk/ndk/28.2.13676358'))
CLANG = os.path.join(NDK, 'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang')
COMMON = ['-w', '-fwrapv', '-fno-strict-aliasing', '-ffp-contract=off', '-fexceptions', '-include',
          os.path.join(RT, 'arm64_guest.h'), '-I', INC, '-I', GEN, '-I', RT, '-DRECOMP_STRICT_ICALL']
VARIANTS = {
    'ref': ['-O0'],
    'fast': ['-O2', '-DRECOMP_LOCAL_REGS', '-DRECOMP_X87_WINDOW', '-DRECOMP_MEM_QUAL=', '-g0'],
}
FUNC_RE = re.compile(r'^void (sub_[0-9A-F]{8})\(void\) \{\n(.*?)\n\}\n', re.S | re.M)
# anything that leaves the body or touches host state the harness does not model
REJECT = re.compile(r'RECOMP_CALL|RECOMP_ICALL|RECOMP_ITAIL|RECOMP_TAIL|RECOMP_HOOK|RECOMP_UNIMPL|recomp_unimpl|'
                    r'__rdtsc|CPUID|nfs_|recomp_run_native|goto \*|\bsub_[0-9A-F]{8}\b|g_cpuid|recomp_dispatch|'
                    r'_seg_|g_fs_base|g_gs_base')

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
static unsigned char *base;

static uint64_t rng;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 16); }
static uint32_t ptr_or_int(void) {
    uint32_t k = rnd() % 8;
    if (k < 4) return SCRATCH + (rnd() % (SCRATCH_SIZE - 0x1000)) / 4 * 4;
    if (k == 4) return rnd() % 16;
    if (k == 5) return rnd();
    if (k == 6) return 0x00400000u + rnd() % 0x01800000u;   /* somewhere in the image */
    return rnd() % 0x1000;
}
static void fill(uint32_t va, uint32_t size, uint64_t seed) {
    rng = seed;
    for (uint32_t i = 0; i < size; i += 4) { uint32_t v = (i % 16 == 0) ? ptr_or_int() : rnd(); memcpy(base + va + i, &v, 4); }
}

static uint64_t h;
static void hb(const void *p, size_t n) { const unsigned char *b = p; while (n--) { h ^= *b++; h *= 0x100000001b3ull; } }

static void run_one(int fi, int seed) {
    alarm(3);
    rng = 0x9E3779B97F4A7C15ull ^ ((uint64_t)seed << 32) ^ (uint64_t)(seed * 7919 + 1);
    g_eax = ptr_or_int(); g_ecx = ptr_or_int(); g_edx = ptr_or_int(); g_ebx = ptr_or_int();
    g_esi = ptr_or_int(); g_edi = ptr_or_int(); g_esp = STACK + STACK_SIZE / 2; g_ebp = g_esp + 0x200;
    g_fp_top = 0;
    for (int i = 0; i < 8; i++) { g_st[i] = 0; for (int k = 0; k < 4; k++) g_xmm[i].u32[k] = rnd(); }
    g_mxcsr = 0x1F80; g_fpu_cw = 0x027F; g_flag_k = g_flag_a = g_flag_b = g_flag_cf = 0;
    FUNCS[fi]();
    h = 0xcbf29ce484222325ull;
    uint32_t regs[12] = {g_eax, g_ecx, g_edx, g_ebx, g_esp, g_ebp, g_esi, g_edi, g_flag_k, g_flag_a, g_flag_b, g_flag_cf};
    hb(regs, sizeof regs); hb(&g_fp_top, sizeof g_fp_top);
    for (int i = 0; i < g_fp_top && i < 8; i++) { double d = g_st[i]; hb(&d, 8); }
    hb(g_xmm, sizeof g_xmm); hb(&g_mxcsr, 4);
    int pm = open("/proc/self/pagemap", O_RDONLY);
    static uint64_t ent[4096];
    uint64_t first = (uintptr_t)base / 4096, pages = 0, wrote = 0;
    for (uint64_t p = 0; p < (1ull << 20) + 16; p += 4096) {
        uint64_t n = (1ull << 20) + 16 - p; if (n > 4096) n = 4096;
        if (pread(pm, ent, n * 8, (off_t)((first + p) * 8)) != (ssize_t)(n * 8)) { printf("%s %d PAGEMAP\n", NAMES[fi], seed); _exit(0); }
        for (uint64_t i = 0; i < n; i++)
            if ((ent[i] >> 63) & 1 && (ent[i] >> 56) & 1) {   /* present and exclusive: written by this child */
                uint64_t pg = p + i; hb(&pg, 8); hb(base + pg * 4096, 4096); wrote++;
            }
        pages += n;
    }
    printf("%s %d %016llx %llu\n", NAMES[fi], seed, (unsigned long long)h, (unsigned long long)wrote);
    fflush(stdout); _exit(0);
}

int main(int argc, char **argv) {
    int seeds = argc > 2 ? atoi(argv[2]) : 3;
    base = mmap(NULL, (1ull << 32) + 0x10000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (base == MAP_FAILED) { perror("mmap"); return 1; }
    g_mem_base = (ptrdiff_t)base;
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror("image"); return 1; }
    size_t n = fread(base + 0x00400000u, 1, 0x04000000u, f); fclose(f);
    fill(SCRATCH, SCRATCH_SIZE, 0x1234567ull); fill(STACK, STACK_SIZE, 0x7654321ull);
    fprintf(stderr, "image %zu bytes, %d functions x %d seeds\n", n, NFUNCS, seeds);
    for (int fi = 0; fi < NFUNCS; fi++)
        for (int s = 0; s < seeds; s++) {
            fflush(stdout);
            pid_t pid = fork();
            if (pid == 0) run_one(fi, s);
            int st = 0; waitpid(pid, &st, 0);
            if (WIFSIGNALED(st)) printf("%s %d SIG%d\n", NAMES[fi], s, WTERMSIG(st));
            else if (WEXITSTATUS(st)) printf("%s %d EXIT%d\n", NAMES[fi], s, WEXITSTATUS(st));
        }
    return 0;
}
'''


def image(exe):
    """The exe as mapped at its preferred base (headers + sections at their RVAs)."""
    d = open(exe, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = pe + 24
    size_img = struct.unpack_from('<I', d, opt + 56)[0]
    size_hdr = struct.unpack_from('<I', d, opt + 60)[0]
    img = bytearray(size_img)
    img[:size_hdr] = d[:size_hdr]
    tab = opt + struct.unpack_from('<H', d, pe + 20)[0]
    for i in range(nsec):
        vsize, rva, rsize, raw = struct.unpack_from('<IIII', d, tab + 40 * i + 8)
        n = min(rsize, vsize or rsize)
        img[rva:rva + n] = d[raw:raw + n]
    return bytes(img)


def leaves():
    out = []
    for path in sorted(glob.glob(os.path.join(GEN, 'recomp_*.c'))):
        txt = open(path).read()
        for m in FUNC_RE.finditer(txt):
            if not REJECT.search(m.group(2)):
                out.append((m.group(1), m.group(0)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-n', type=int, default=3000)
    ap.add_argument('--seeds', type=int, default=3)
    ap.add_argument('-s', '--serial', default='192.168.1.206:5555')
    ap.add_argument('--only', default='')
    ap.add_argument('--reuse', default='', help='variant whose out-<v>.txt from the same -n/--seeds run is reused')
    a = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)
    allf = leaves()
    if a.only:
        want = set(a.only.split(','))
        pick = [f for f in allf if f[0] in want]
    else:
        random.Random(19).shuffle(allf)
        pick = sorted(allf[:a.n])
    print(f'{len(allf)} leaf functions in the lift, testing {len(pick)} x {a.seeds} seeds', flush=True)
    src = os.path.join(WORK, 'funcdiff.c')
    with open(src, 'w') as f:
        f.write('#include "recomp_funcs.h"\n#include <math.h>\n#include <string.h>\n')
        for _, body in pick:
            f.write(body + '\n')
        f.write(f'#define NFUNCS {len(pick)}\n')
        f.write('static void (*const FUNCS[])(void) = {' + ','.join(n for n, _ in pick) + '};\n')
        f.write('static const char *const NAMES[] = {' + ','.join(f'"{n}"' for n, _ in pick) + '};\n')
        f.write(HARNESS)
    img = os.path.join(WORK, 'image.bin')
    if not os.path.exists(img):
        open(img, 'wb').write(image(EXE))
    adb = ['adb', '-s', a.serial]
    subprocess.run([*adb, 'push', img, '/data/local/tmp/mgr-fd-image.bin'], check=True, capture_output=True)
    res = {}
    for v, flags in VARIANTS.items():
        if v == a.reuse and os.path.exists(os.path.join(WORK, f'out-{v}.txt')):
            res[v] = {tuple(l.split()[:2]): l.split()[2] for l in open(os.path.join(WORK, f'out-{v}.txt')) if len(l.split()) >= 3}
            print(f'{v}: reused {len(res[v])} results', flush=True)
            continue
        exe = os.path.join(WORK, f'funcdiff-{v}')
        r = subprocess.run([CLANG, *flags, *COMMON, src, '-o', exe, '-static', '-lm'], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(f'{v}: clang failed\n{r.stderr[-3000:]}')
        subprocess.run([*adb, 'push', exe, f'/data/local/tmp/mgr-fd-{v}'], check=True, capture_output=True)
        out = subprocess.run([*adb, 'shell', f'chmod 755 /data/local/tmp/mgr-fd-{v} && /data/local/tmp/mgr-fd-{v} '
                              f'/data/local/tmp/mgr-fd-image.bin {a.seeds}'], capture_output=True, text=True)
        open(os.path.join(WORK, f'out-{v}.txt'), 'w').write(out.stdout)
        res[v] = {}
        for line in out.stdout.splitlines():
            p = line.split()
            if len(p) >= 3:
                res[v][(p[0], p[1])] = p[2]
        print(f'{v}: {len(res[v])} results; {out.stderr.strip()[-200:]}', flush=True)
    subprocess.run([*adb, 'shell', 'rm -f /data/local/tmp/mgr-fd-*'], capture_output=True)
    same = diff = skipped = 0
    bad = set()
    for k, rv in res['ref'].items():
        fv = res['fast'].get(k)
        if fv is None or not re.fullmatch(r'[0-9a-f]{16}', rv) or not re.fullmatch(r'[0-9a-f]{16}', fv):
            skipped += 1
            if fv is not None and rv != fv:
                bad.add(k[0])   # one side faulted or timed out, the other not
            continue
        if rv == fv:
            same += 1
        else:
            diff += 1
            bad.add(k[0])
    print(f'same {same}, different {diff}, skipped (timeout/fault on a side) {skipped}')
    print('functions to isolate:', ' '.join(sorted(bad)) or 'none')
    open(os.path.join(WORK, 'result.txt'), 'w').write(
        f'same {same} different {diff} skipped {skipped}\n' + '\n'.join(sorted(bad)) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
