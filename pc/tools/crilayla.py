#!/usr/bin/env python3
"""CRILAYLA decompressor (reference for checking the lifted CRI FS output).
Algorithm as in the public CriPakTools DecompressCRILAYLA (MIT). Usage: crilayla.py <file> <offset_hex> [out]"""
import struct, sys

def decompress(buf, off=0):
    if buf[off:off + 8] != b'CRILAYLA':
        raise ValueError('no CRILAYLA magic')
    usize, hoff = struct.unpack_from('<II', buf, off + 8)
    chunk = buf[off:off + 0x10 + hoff + 0x100]
    out = bytearray(usize + 0x100)
    out[0:0x100] = chunk[0x10 + hoff:0x10 + hoff + 0x100]
    pos = len(chunk) - 0x100 - 1
    end = 0x100 + usize - 1
    pool = left = done = 0
    def bits(n):
        nonlocal pool, left, pos
        v = got = 0
        while got < n:
            if left == 0:
                pool = chunk[pos]; left = 8; pos -= 1
            take = min(left, n - got)
            v = (v << take) | ((pool >> (left - take)) & ((1 << take) - 1))
            left -= take; got += take
        return v
    lens = (2, 3, 5, 8)
    while done < usize:
        if bits(1):
            ref = end - done + bits(13) + 3
            length = 3
            for level, w in enumerate(lens):
                x = bits(w); length += x
                if x != (1 << w) - 1:
                    break
            else:
                while True:
                    x = bits(8); length += x
                    if x != 255:
                        break
            for _ in range(length):
                out[end - done] = out[ref]; ref -= 1; done += 1
        else:
            out[end - done] = bits(8); done += 1
    return bytes(out)

if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    res = decompress(data, int(sys.argv[2], 16))
    if len(sys.argv) > 3:
        open(sys.argv[3], 'wb').write(res)
    print(len(res), res[0x100:0x100 + 16])
