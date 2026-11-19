#!/usr/bin/env python3
"""pcrecomp difftest (lifted C vs Unicorn) built with the Android build's compiler and flags and run on the device.

The upstream tools/lift/difftest.py compiles the lifted cases with the host compiler at -O0, so it proves the lifter's
semantics but not the configuration we ship: NDK clang, aarch64, -O2 with RECOMP_LOCAL_REGS, RECOMP_X87_WINDOW and
non-volatile guest memory (GUEST_FAST in pc/android/CMakeLists.txt). This wrapper reuses its 202 cases and its Unicorn
reference, and only changes how the C is built and where it runs:

    difftest-android.py --flags fast   -s <serial>     # -O2 + GUEST_FAST defines (what libmgrpc.so uses)
    difftest-android.py --flags o1     -s <serial>     # -O1, globals (the old M1 build)
    difftest-android.py --flags host                   # upstream behaviour (host cc -O0)

Needs the pinned pcrecomp checkout (build/lab/pc/pcrecomp-pinned) and its venv (unicorn, capstone).
"""
import argparse, os, subprocess, sys, shutil, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
PCRECOMP = os.path.join(ROOT, 'build', 'lab', 'pc', 'pcrecomp-pinned')
NDK = os.environ.get('ANDROID_NDK', os.path.expanduser('~/Android/Sdk/ndk/28.2.13676358'))
CLANG = os.path.join(NDK, 'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang')
COMMON = ['-w', '-fwrapv', '-fno-strict-aliasing', '-ffp-contract=off', '-static', '-lm']
FLAGS = {
    'fast': ['-O2', '-DRECOMP_LOCAL_REGS', '-DRECOMP_X87_WINDOW', '-DRECOMP_MEM_QUAL=', '-g0'],
    'o1': ['-O1'],
    'o0': ['-O0'],
}

sys.path.insert(0, PCRECOMP)
from tools.lift import difftest as dt  # noqa: E402


def patch_templates(local_regs, x87_window):
    if x87_window:
        # the header owns the (weak) window storage; g_st is a pointer expression, so size it explicitly
        dt.PRELUDE = dt.PRELUDE.replace('double   g_st[8];\n', '')
    if local_regs:
        # every lifted body works on local copies of the registers; the harness sets and reads the globals
        dt.CASE_FN = dt.CASE_FN.replace('    int _fpu_cmp = 0;\n', '    RECOMP_REGS_LOCALS\n    int _fpu_cmp = 0;\n', 1)
        dt.CASE_FN = dt.CASE_FN.replace('    /* The whole lazy model', '    RECOMP_REGS_OUT();\n    /* The whole lazy model', 1)
    orig_build_c = dt.build_c

    def build_c(cases):
        return orig_build_c(cases).replace('memset(g_st, 0, sizeof g_st);', 'memset(g_st, 0, 8 * sizeof(double));')
    dt.build_c = build_c


def make_runner(flags, serial, workroot):
    def run_lifted(cases, workdir, keep):
        src = os.path.join(workdir, 'difftest.c')
        exe = os.path.join(workdir, 'difftest-arm64')
        with open(src, 'w') as f:
            f.write(dt.build_c(cases))
        inc = os.path.join(PCRECOMP, 'runtime', 'recomp32')
        r = subprocess.run([CLANG, *flags, '-I', inc, src, '-o', exe, *COMMON], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(f'clang failed:\n{r.stderr[-4000:]}')
        if keep:
            shutil.copy(src, os.path.join(workroot, 'difftest-last.c'))
        adb = ['adb', '-s', serial]
        subprocess.run([*adb, 'push', exe, '/data/local/tmp/mgr-difftest'], check=True, capture_output=True)
        out = subprocess.run([*adb, 'shell', 'chmod 755 /data/local/tmp/mgr-difftest && /data/local/tmp/mgr-difftest'],
                             capture_output=True, text=True)
        subprocess.run([*adb, 'shell', 'rm -f /data/local/tmp/mgr-difftest'], capture_output=True)
        if out.returncode:
            raise SystemExit(f'device run exited {out.returncode}:\n{out.stdout[-2000:]}{out.stderr[-2000:]}')
        return dt.parse_output(out.stdout)
    return run_lifted


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--flags', choices=['fast', 'o1', 'o0', 'host'], default='fast')
    ap.add_argument('-s', '--serial', default='192.168.1.206:5555')
    ap.add_argument('rest', nargs='*', help='passed to difftest (-k, -v, --keep)')
    a = ap.parse_args()
    if a.flags != 'host':
        flags = FLAGS[a.flags]
        patch_templates('-DRECOMP_LOCAL_REGS' in flags, '-DRECOMP_X87_WINDOW' in flags)
        work = os.path.join(ROOT, 'build', 'lab', 'pc', 'difftest')
        os.makedirs(work, exist_ok=True)
        tempfile.tempdir = work
        dt.run_lifted = make_runner(flags, a.serial, work)
    sys.argv = [sys.argv[0], *a.rest]
    return dt.main()


if __name__ == '__main__':
    sys.exit(main())
