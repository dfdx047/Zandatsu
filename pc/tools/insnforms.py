#!/usr/bin/env python3
"""insnforms: every distinct float/SIMD instruction form the catalogued code uses, normalised for insnfuzz.py.

A memory operand is rewritten to the absolute form [disp32] (mod=00 rm=101), so a test needs no base register and the
eight GPRs stay free inputs. Register operands are kept. Forms are deduplicated by their normalised bytes and counted.

    insnforms.py <exe> <catalog.json> <out.json>

The output holds instruction bytes from the game (one instruction per form): it lives under build/lab/, never in git.
"""
import json, os, re, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM, X86_OP_REG

FLOATREG = re.compile(r'\b(xmm\d|mm\d|st\(\d\)|st\b)')
X87 = re.compile(r'^f')
SKIP = {'fwait', 'wait', 'fnsave', 'fsave', 'frstor', 'fnstenv', 'fstenv', 'fldenv', 'fninit', 'finit', 'fbld',
        'fbstp', 'femms', 'emms', 'ffree', 'ffreep', 'fnclex', 'fnsetpm', 'fdecstp', 'fincstp',
        'stmxcsr', 'ldmxcsr', 'fldcw', 'fnstcw', 'fstcw', 'prefetchnta', 'prefetcht0', 'prefetcht1',
        'prefetcht2', 'prefetchw', 'movntps', 'movnti', 'movntq', 'maskmovq'}
MARK = 0x30000200   # placeholder disp for the operand memory; insnfuzz.py rewrites it


def normalise(insn):
    b = bytes(insn.bytes)
    mems = [o for o in insn.operands if o.type == X86_OP_MEM]
    if not mems:
        return b
    if insn.prefix[1] or insn.prefix[3] or mems[0].mem.segment:   # segment / address-size prefix
        return None
    mo = insn.modrm_offset
    tail = b[insn.imm_offset:] if insn.imm_size else b''
    return b[:mo] + bytes([(insn.modrm & 0x38) | 0x05]) + MARK.to_bytes(4, 'little') + tail


def main():
    exe, catalog, out = sys.argv[1:4]
    data = open(exe, 'rb').read()
    sys.path.insert(0, os.path.join(HERE, '..', '..', 'build', 'lab', 'pc', 'pcrecomp-pinned', 'tools', 'pe'))
    from pe_analyze import analyze_pe
    info = analyze_pe(exe)
    secs = info.sections

    def file_off(va):
        rva = va - info.image_base
        for s in secs:
            if s.virtual_address <= rva < s.virtual_address + min(s.virtual_size, s.raw_size):
                return s.raw_offset + rva - s.virtual_address
        return None

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    cat = json.load(open(catalog))
    forms = {}
    nblk = 0
    for f in cat['functions']:
        for start, end, _succ, _x in f['blocks']:
            off = file_off(start)
            if off is None:
                continue
            nblk += 1
            for insn in md.disasm(data[off:off + (end - start)], start):
                m = insn.mnemonic
                if m in SKIP or m.startswith(('rep', 'lock')):
                    continue
                if not (X87.match(m) or FLOATREG.search(insn.op_str) or m in ('cvttss2si', 'cvtss2si', 'cvttsd2si',
                                                                              'cvtsd2si')):
                    continue
                k = normalise(insn)
                if k is None:
                    continue
                e = forms.get(k)
                if e is None:
                    forms[k] = e = {'bytes': k.hex(), 'mnemonic': m, 'op_str': insn.op_str, 'count': 0,
                                    'va': insn.address, 'x87': bool(X87.match(m)),
                                    'mmx': bool(re.search(r'\bmm\d', insn.op_str))}
                e['count'] += 1
    del cat
    res = sorted(forms.values(), key=lambda e: -e['count'])
    json.dump(res, open(out, 'w'), indent=0)
    by = collections.Counter(e['mnemonic'] for e in res)
    print(f'{nblk} blocks, {len(res)} forms, {len(by)} mnemonics')
    print(' '.join(f'{m}:{n}' for m, n in by.most_common()))


if __name__ == '__main__':
    main()
