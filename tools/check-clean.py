#!/usr/bin/env python3
"""Confere que o APK e o repositório não têm código nem shaders do jogo em claro.

Procura em cada ficheiro do APK (descomprimido), no próprio .apk e em todos os ficheiros do git (HEAD, com o
submódulo) e em todos os blobs do histórico:
  - nomes do código gerado e do cache de shaders (sub_8xxxxxxx, __imp__sub_, PPCFuncMappings, g_shaderCacheEntries...);
  - milhares de pedaços de 32 bytes tirados ao acaso do que é do jogo no libgame.so em claro (o corpo das funções
    sub_*/__imp__sub_* do código gerado e as tabelas do cache de shaders do Renut, pela tabela de símbolos dinâmicos)
    e da imagem PE do default.xex (o jogo original). O resto do libgame.so (libc++, imgui, plume, Renut, runtime) é
    código aberto que também está nas outras .so e no submódulo: não conta.
Com grep -F (Aho-Corasick): pedaços sem \\n nem \\0 e com bytes variados. Pedaços de 32 bytes também aparecem por
coincidência em qualquer binário (epílogos de função do compilador, tabelas de caracteres do CRT): conta-se quantos
pedaços distintos cada ficheiro tem, e só a partir de LIMIT (1%) é código do jogo. O próprio libgame.so dá ~100%.

Uso: check-clean.py <app-release.apk> <libgame.so em claro> <default.xex>
"""
import os
import random
import re
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = [rb'sub_8[0-9A-F]{7}', rb'__imp__sub_', rb'PPCFuncMappings', rb'PPCImageConfig', rb'g_shaderCacheEntries',
         rb'g_compressedSpirvCache', rb'g_compressedDxilCache']
N_WINDOWS, W = 3000, 32
LIMIT = 60  # pedaços distintos (de 6000) num ficheiro


def ndk_tool(name):
    base = os.path.expanduser('~/Android/Sdk/ndk')
    for v in sorted(os.listdir(base), reverse=True):
        p = os.path.join(base, v, 'toolchains/llvm/prebuilt/linux-x86_64/bin', name)
        if os.path.exists(p):
            return p
    sys.exit(f'sem {name} no NDK')


def game_ranges(so):
    """(início, fim) no ficheiro de cada função do código gerado e de cada tabela do cache de shaders."""
    secs = []
    for l in subprocess.run([ndk_tool('llvm-readelf'), '-S', '-W', so], capture_output=True, text=True,
                            check=True).stdout.split('\n'):
        m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+(PROGBITS)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', l)
        if m:
            secs.append((int(m[3], 16), int(m[4], 16), int(m[5], 16)))
    out = []
    for l in subprocess.run([ndk_tool('llvm-nm'), '-D', '-S', '--defined-only', so], capture_output=True, text=True,
                            check=True).stdout.split('\n'):
        f = l.split()
        if len(f) != 4 or not (re.match(r'(__imp__)?sub_8[0-9A-F]{7}$', f[3]) or f[3].startswith('g_shader')
                               or f[3].startswith('g_compressed')):
            continue
        addr, size = int(f[0], 16), int(f[1], 16)
        for a, off, sz in secs:
            if a <= addr < a + sz and size >= W:
                out.append((addr - a + off, addr - a + off + size))
    return out


def windows(data, n, seed, ranges=None):
    rnd, out, tries = random.Random(seed), set(), 0
    weights = [e - s for s, e in ranges] if ranges else None
    while len(out) < n and tries < n * 50:
        tries += 1
        if ranges:
            s, e = rnd.choices(ranges, weights)[0]
            i = rnd.randrange(s, e - W + 1)
        else:
            i = rnd.randrange(0, len(data) - W)
        w = data[i:i + W]
        if b'\n' in w or b'\0' in w or len(set(w)) < 16:
            continue
        # Tabelas genéricas (0xAA, 0xAB, 0xAC..., alfabetos, pares 01 3A 01 3B...: só 2 passos, ou o mesmo passo em >= 70% do pedaço): estão em qualquer biblioteca.
        steps = [[(w[i + k] - w[i]) & 255 for i in range(W - k)] for k in (1, 2, 4)]
        if any(len(set(d)) <= 2 or max(map(d.count, set(d))) >= 0.7 * len(d) for d in steps):
            continue
        out.add(w)
    return sorted(out)


def grep_hits(patterns, paths):
    """{ficheiro: pedaços distintos encontrados} (só os ficheiros com algum)."""
    if not paths:
        return {}
    with tempfile.NamedTemporaryFile('wb', delete=False) as f:
        f.write(b'\n'.join(patterns) + b'\n')
        pat = f.name
    try:
        r = subprocess.run(['grep', '-a', '-o', '-H', '-Z', '-F', '-f', pat, '--'] + paths, capture_output=True,
                           env={**os.environ, 'LC_ALL': 'C'})
        found = {}
        for line in r.stdout.split(b'\n'):
            if b'\0' in line:
                name, match = line.split(b'\0', 1)
                found.setdefault(name.decode(errors='replace'), set()).add(match)
        return {k: len(v) for k, v in found.items()}
    finally:
        os.unlink(pat)


def report(what, hits, rel):
    bad = {k: n for k, n in hits.items() if n >= LIMIT}
    print(f'{what}: {len(bad)} com código/shaders do jogo (>= {LIMIT} pedaços)'
          + (f'; coincidências: ' + ', '.join(f'{rel(k)} {n}' for k, n in sorted(hits.items()) if n < LIMIT)
             if len(hits) > len(bad) else ''))
    for k, n in bad.items():
        print(f'   {rel(k)}: {n} pedaços')
    return not bad


def name_hits(paths):
    out = []
    for p in paths:
        with open(p, 'rb') as f:
            d = f.read()
        found = [n.decode() for n in NAMES if re.search(n, d)]
        if found:
            out.append(f'{p}: {", ".join(found)}')
    return out


def main():
    apk, so, xex = sys.argv[1:]
    with open(so, 'rb') as f:
        game = f.read()
    with tempfile.TemporaryDirectory() as t:
        pe_path = os.path.join(t, 'pe.bin')
        subprocess.run(['bash', os.path.join(ROOT, 'tools/xex-image/build.sh')], check=True)
        subprocess.run([os.path.join(ROOT, 'out/host/xex-image'), xex, pe_path], check=True, capture_output=True)
        with open(pe_path, 'rb') as f:
            pe = f.read()
        ranges = game_ranges(so)
        seed = int(os.environ.get('SEED', '1'))  # outra amostra: SEED=2, 3...
        gw, pw = windows(game, N_WINDOWS, 2 * seed - 1, ranges), windows(pe, N_WINDOWS, 2 * seed)
        pats = gw + pw
        print(f'padrões: {len(gw)} do libgame.so ({len(ranges)} funções/tabelas do jogo, '
              f'{sum(e - s for s, e in ranges) >> 20} MB) + {len(pw)} da imagem PE ({len(pe) >> 20} MB), {W} bytes cada')
        # Prova de que a procura funciona: o próprio libgame.so tem de dar positivo.
        own = grep_hits(pats, [so]).get(so, 0)
        print(f'controlo: o libgame.so em claro tem {own} dos {len(pats)} pedaços')
        # ~95%: o grep -o não conta pedaços sobrepostos a outro já encontrado.
        assert own >= len(gw) * 0.9, 'a procura não encontra os pedaços no próprio libgame.so'

        ok = True
        # 1. APK: cada ficheiro descomprimido e o .apk em si.
        x = os.path.join(t, 'apk')
        with zipfile.ZipFile(apk) as z:
            z.extractall(x)
            names = z.namelist()
        files = [os.path.join(x, n) for n in names if os.path.isfile(os.path.join(x, n))] + [apk]
        ok &= report(f'APK ({len(files) - 1} ficheiros + o .apk)', grep_hits(pats, files),
                     lambda k: k.replace(x + '/', ''))
        names_found = name_hits(files)
        print(f'APK: {len(names_found)} com nomes do código gerado/cache de shaders')
        for h in names_found:
            print('  ', h.replace(x + '/', ''))
        ok &= not names_found
        if not any(n.startswith('lib/') and n.endswith('libgame.so') for n in names):
            print('APK: sem libgame.so')
        else:
            print('APK: TEM libgame.so')
            ok = False

        # 2. Repositório (HEAD + submódulos), ficheiros no disco que o git segue.
        ls = subprocess.run(['git', '-C', ROOT, 'ls-files', '--recurse-submodules', '-z'], capture_output=True,
                            check=True).stdout.split(b'\0')
        repo = [os.path.join(ROOT, p.decode()) for p in ls if p and os.path.isfile(os.path.join(ROOT, p.decode()))]
        ok &= report(f'repositório ({len(repo)} ficheiros)', grep_hits(pats, repo), lambda k: os.path.relpath(k, ROOT))

        # 3. Histórico: todos os blobs de todos os commits (só o repo principal).
        objs = subprocess.run(['git', '-C', ROOT, 'rev-list', '--all', '--objects'], capture_output=True,
                              check=True).stdout.decode().split('\n')
        blobs = subprocess.run(['git', '-C', ROOT, 'cat-file', '--batch-check=%(objecttype) %(objectname) %(objectsize)'],
                               input='\n'.join(o.split(' ')[0] for o in objs if o) + '\n', capture_output=True,
                               text=True, check=True).stdout.split('\n')
        blob_ids = [b.split()[1] for b in blobs if b.startswith('blob ')]
        hd = os.path.join(t, 'blobs')
        os.makedirs(hd)
        paths = []
        for i in range(0, len(blob_ids), 500):
            batch = blob_ids[i:i + 500]
            data = subprocess.run(['git', '-C', ROOT, 'cat-file', '--batch'], input=('\n'.join(batch) + '\n').encode(),
                                  capture_output=True, check=True).stdout
            p = 0
            for bid in batch:
                head_end = data.index(b'\n', p)
                size = int(data[p:head_end].split()[2])
                path = os.path.join(hd, bid)
                with open(path, 'wb') as f:
                    f.write(data[head_end + 1:head_end + 1 + size])
                paths.append(path)
                p = head_end + 1 + size + 1
        ok &= report(f'histórico ({len(paths)} blobs)', grep_hits(pats, paths), os.path.basename)
    print('LIMPO' if ok else 'PROBLEMAS')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
