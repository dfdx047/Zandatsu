#!/usr/bin/env python3
"""libmgrpc.so (MGR:R PC port: runtime + lifted game code) -> <out>/libmgrpc.zdat + libmgrpc.json.

Same ZDT2 container as tools/pack-libgame.py (360 port), read by app/.../pc/PcGame.kt:
  header "ZDT2" (zlib, 1 MiB) or "ZDT3" (--xz: xz, 8 MiB), chunk size (u32 LE), .so size (u64 LE), nonce prefix (8 bytes); then per 1 MiB chunk: block size (u32 LE)
  + zlib(level 9) chunk encrypted with AES-256-GCM (nonce = prefix + chunk index u32 BE, AAD = header, 16-byte tag).
Key = SHA-256 of the raw .text section of the player's executable (the code we lifted): only the owner of the game has
it. The json carries the SHA-256 of the key (to say "unsupported executable" before trying), never the key.

Usage: pack-libmgr.py <libmgrpc.so> <llvm-strip> <game exe> <out dir>
"""
import concurrent.futures, hashlib, json, lzma, os, struct, subprocess, sys, tempfile, zlib
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

CHUNK = 1 << 20

def text_key(exe):
    d = open(exe, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]; opt = struct.unpack_from('<H', d, pe + 20)[0]
    sec = pe + 24 + opt
    for i in range(nsec):
        name = d[sec + 40 * i:sec + 40 * i + 8].rstrip(b'\0')
        size, raw = struct.unpack_from('<II', d, sec + 40 * i + 16)
        if name == b'.text':
            return hashlib.sha256(d[raw:raw + size]).digest()
    raise SystemExit('no .text section')

def main():
    # --xz: formato ZDT3 (blocos de 8 MiB em xz, ~93 MB contra ~144 MB do ZDT2/zlib). A app le os dois.
    xz = '--xz' in sys.argv
    so, strip, exe, out = [a for a in sys.argv[1:] if a != '--xz'][:4]
    chunk = 8 << 20 if xz else CHUNK
    key = text_key(exe)
    with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as t:
        stripped = os.path.join(t, 'lib.so')
        subprocess.run([strip, '--strip-unneeded', so, '-o', stripped], check=True)
        plain = open(stripped, 'rb').read()
    prefix = os.urandom(8)
    header = (b'ZDT3' if xz else b'ZDT2') + struct.pack('<IQ', chunk, len(plain)) + prefix
    aes = AESGCM(key)
    os.makedirs(out, exist_ok=True)
    tmp = os.path.join(out, 'libmgrpc.zdat.part')
    with open(tmp, 'wb') as f:
        f.write(header)
        pack = (lambda b: lzma.compress(b, preset=6)) if xz else (lambda b: zlib.compress(b, 9))
        parts = [plain[i:i + chunk] for i in range(0, len(plain), chunk)]
        with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as ex:
            for n, packed in enumerate(ex.map(pack, parts)):
                block = aes.encrypt(prefix + struct.pack('>I', n), packed, header)
                f.write(struct.pack('<I', len(block))); f.write(block)
    os.replace(tmp, os.path.join(out, 'libmgrpc.zdat'))
    meta = {'exe_sha1': hashlib.sha1(open(exe, 'rb').read()).hexdigest(), 'key_sha256': hashlib.sha256(key).hexdigest(),
            'key_from': '.text', 'size': len(plain), 'id': hashlib.sha1(plain).hexdigest()[:12]}
    json.dump(meta, open(os.path.join(out, 'libmgrpc.json'), 'w'), indent=1)
    print(json.dumps(meta), os.path.getsize(os.path.join(out, 'libmgrpc.zdat')))

if __name__ == '__main__':
    main()
