#!/usr/bin/env python3
"""Port de PC: confere que o APK e o repositório não têm o código do jogo em claro.

Mesmo método do tools/check-clean.py (360): pedaços aleatórios de W bytes tirados (a) da .text da lib do jogo em claro
(libmgrpc.so: runtime + código liftado) e (b) da .text do executável do jogador, procurados com grep em todos os
ficheiros do APK (extraído) e do git (pc/, app/, tools/). >= LIMIT pedaços num ficheiro = código do jogo em claro.

Uso: check-clean-pc.py <apk> <libmgrpc.so em claro> <exe do jogo>
"""
import importlib.util, os, struct, subprocess, sys, tempfile, zipfile

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
spec = importlib.util.spec_from_file_location('cc', os.path.join(ROOT, 'tools/check-clean.py'))
cc = importlib.util.module_from_spec(spec); spec.loader.exec_module(cc)

def pe_text(exe):
    d = open(exe, 'rb').read(); pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]; sec = pe + 24 + struct.unpack_from('<H', d, pe + 20)[0]
    for i in range(n):
        if d[sec + 40 * i:sec + 40 * i + 8].rstrip(b'\0') == b'.text':
            size, raw = struct.unpack_from('<II', d, sec + 40 * i + 16); return d[raw:raw + size]
    sys.exit('exe sem .text')

def elf_text(so):
    with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as t:
        out = os.path.join(t, 'text.bin')
        subprocess.run([cc.ndk_tool('llvm-objcopy'), '-O', 'binary', '--only-section=.text', so, out], check=True)
        return open(out, 'rb').read()

def main():
    apk, so, exe = sys.argv[1:4]
    pats = cc.windows(elf_text(so), 4000, 1) + cc.windows(pe_text(exe), 4000, 2)
    ok = True
    with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as t:
        zipfile.ZipFile(apk).extractall(t)
        files = [os.path.join(r, f) for r, _, fs in os.walk(t) for f in fs]
        ok &= cc.report(f'APK ({len(files)} ficheiros)', cc.grep_hits(pats, files), lambda p: os.path.relpath(p, t))
    tracked = [os.path.join(ROOT, p) for p in subprocess.run(['git', '-C', ROOT, 'ls-files', 'pc', 'app', 'tools'],
               capture_output=True, text=True, check=True).stdout.split('\n') if p and os.path.isfile(os.path.join(ROOT, p))]
    ok &= cc.report(f'git ({len(tracked)} ficheiros)', cc.grep_hits(pats, tracked), lambda p: os.path.relpath(p, ROOT))
    print('LIMPO' if ok else 'NAO LIMPO'); sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()
