#!/usr/bin/env python3
"""exe-check.py <exe> [libmgrpc.json] — identify a player's MGR:R PC executable. Identification only: it reads, never
changes or unpacks anything.

Prints: SHA-1/SHA-256 of the file, the sections (a SteamStub-wrapped exe has a `.bind` section and its entry point
inside it, and its `.text` is encrypted on disk), whether steam_api.dll is imported and which Steamworks entry points
(SteamAPI_Init, SteamAPI_RestartAppIfNecessary), and whether the libgame key (SHA-256 of `.text`, pc/tools/pack-libmgr.py)
matches the one in libmgrpc.json, i.e. whether the shipped encrypted libgame would open with this exe.
"""
import hashlib, json, struct, sys


def main():
    exe = sys.argv[1]
    meta = json.load(open(sys.argv[2])) if len(sys.argv) > 2 else None
    d = open(exe, 'rb').read()
    print(f'file     {exe}\nsize     {len(d)}\nsha1     {hashlib.sha1(d).hexdigest()}\nsha256   {hashlib.sha256(d).hexdigest()}')
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = pe + 24
    entry = struct.unpack_from('<I', d, opt + 16)[0]
    tab = opt + struct.unpack_from('<H', d, pe + 20)[0]
    secs = []
    for i in range(nsec):
        name = d[tab + 40 * i: tab + 40 * i + 8].rstrip(b'\0').decode('latin1')
        vsize, rva, rsize, raw = struct.unpack_from('<IIII', d, tab + 40 * i + 8)
        secs.append((name, rva, vsize, raw, rsize))
        print(f'section  {name:8} rva {rva:08X} vsize {vsize:08X} raw {raw:08X} rsize {rsize:08X}')
    in_sec = next((s[0] for s in secs if s[1] <= entry < s[1] + max(s[2], s[4])), '?')
    print(f'entry    rva {entry:08X} in {in_sec}')
    print('steamstub', 'YES (.bind section)' if any(s[0] == '.bind' for s in secs) else 'no .bind section')
    for name in (b'steam_api.dll', b'SteamAPI_Init', b'SteamAPI_RestartAppIfNecessary', b'SteamAPI_Shutdown'):
        print(f'import   {name.decode():32} {"present" if name in d else "absent"}')
    text = next((s for s in secs if s[0] == '.text'), None)
    if text:
        key = hashlib.sha256(d[text[3]:text[3] + text[4]]).digest()
        ks = hashlib.sha256(key).hexdigest()
        print(f'libkey   sha256(key) {ks}')
        if meta:
            print('libgame ', 'OPENS with this exe' if ks == meta['key_sha256'] else
                  f'does NOT open (built from exe sha1 {meta["exe_sha1"][:12]})')


if __name__ == '__main__':
    main()
