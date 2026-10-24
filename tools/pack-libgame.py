#!/usr/bin/env python3
"""libgame.so -> assets/libgame.zdat (encriptado) + libgame.json. Chamado pelo CMake depois de ligar o libgame.so.

A chave (AES-256-GCM) é o SHA-256 da imagem PE do default.xex (tools/xex-image, o mesmo src/xex_image.cpp do
aparelho): só quem tem o disco a obtém. O APK leva o .zdat e o json, com o SHA-256 da chave (para dizer "versão não
suportada" antes de tentar), nunca a chave.

Formato do .zdat (lido pelo GameLib.kt):
  cabeçalho de 24 bytes: "ZDT1", tamanho do pedaço (u32 LE), tamanho do .so (u64 LE), prefixo do nonce (8 bytes);
  depois cada pedaço i do .so, encriptado (AES-256-GCM, nonce = prefixo + i em u32 BE, AAD = o cabeçalho) e com a
  etiqueta de 16 bytes no fim. Pedaços de 1 MiB: o aparelho desencripta sem ter o .so inteiro na memória.

Uso: pack-libgame.py <libgame.so> <llvm-strip> <default.xex> <pasta de saída>
"""
import hashlib
import json
import os
import struct
import subprocess
import sys
import tempfile

from cryptography.hazmat.primitives.ciphers.aead import AESGCM

CHUNK = 1 << 20


def main():
    so, strip, xex, out = sys.argv[1:]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    subprocess.run(['bash', os.path.join(root, 'tools/xex-image/build.sh')], check=True)
    with tempfile.TemporaryDirectory() as t:
        pe_path = os.path.join(t, 'pe.bin')
        info = subprocess.run([os.path.join(root, 'out/host/xex-image'), xex, pe_path], check=True,
                              capture_output=True, text=True).stdout.split()
        title_id = info[0]
        with open(pe_path, 'rb') as f:
            key = hashlib.sha256(f.read()).digest()
        stripped = os.path.join(t, 'libgame.so')
        subprocess.run([strip, '--strip-unneeded', so, '-o', stripped], check=True)
        with open(stripped, 'rb') as f:
            plain = f.read()
    with open(xex, 'rb') as f:
        xex_sha1 = hashlib.sha1(f.read()).hexdigest()

    prefix = os.urandom(8)
    header = b'ZDT1' + struct.pack('<IQ', CHUNK, len(plain)) + prefix
    aes = AESGCM(key)
    os.makedirs(out, exist_ok=True)
    tmp = os.path.join(out, 'libgame.zdat.part')
    with open(tmp, 'wb') as f:
        f.write(header)
        for i in range(0, len(plain), CHUNK):
            f.write(aes.encrypt(prefix + struct.pack('>I', i // CHUNK), plain[i:i + CHUNK], header))
    os.replace(tmp, os.path.join(out, 'libgame.zdat'))
    meta = {
        'format': 1,
        'title_id': title_id,
        'xex_sha1': xex_sha1,
        'key_sha256': hashlib.sha256(key).hexdigest(),
        # Identifica este libgame.so (o aparelho refaz a cópia quando o APK traz outro).
        'id': hashlib.sha256(header).hexdigest()[:16],
        'size': len(plain),
    }
    with open(os.path.join(out, 'libgame.json'), 'w') as f:
        json.dump(meta, f, indent=1)
    print(f'libgame.zdat: {len(plain) / 1e6:.1f} MB, title {title_id}, id {meta["id"]}')


if __name__ == '__main__':
    main()
