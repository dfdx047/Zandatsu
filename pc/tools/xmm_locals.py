#!/usr/bin/env python3
"""xmm_locals.py <gen dir>: keep the guest SSE registers in C locals inside each lifted function (in place, idempotent).

The lifted code reads and writes g_xmm[n], memory in the per-thread cpu struct; with -fno-strict-aliasing every guest
store may alias it, so clang stores and reloads the SSE registers around every guest memory write. GPRs already get
locals (RECOMP_LOCAL_REGS); this does the same for XMM in every function that uses them: g_xmm[i] becomes l_xi (only
the registers it uses), loaded on entry, and RECOMP_REGS_OUT()/RECOMP_REGS_IN() -- which the pinned runtime runs
around every call, tail call, back-edge yield, hook and return -- are redefined around that function to also write
back / reload them (pc/runtime/xmm_locals.h). Functions without SSE are untouched.
"""
import os, re, sys

MARK = '#include "xmm_locals.h"'
HEAD = re.compile(r'^(static )?void sub_[0-9A-F]{8}(_body)?\(void\) \{$')
USE = re.compile(r'g_xmm\[(\d)\]')


def process(text):
    if MARK in text:
        return None
    out, lines, i, changed = [], text.split("\n"), 0, 0
    while i < len(lines):
        line = lines[i]
        if not HEAD.match(line):
            out.append(line); i += 1; continue
        j = i + 1
        while j < len(lines) and lines[j] != "}":
            j += 1
        body = lines[i + 1:j]
        regs = sorted({int(m) for l in body for m in USE.findall(l)})
        if not regs or not any(l.strip() == "RECOMP_REGS_LOCALS" for l in body):
            out.extend(lines[i:j + 1]); i = j + 1; continue
        wb = ", ".join(f"g_xmm[{r}] = l_x{r}" for r in regs)
        rl = ", ".join(f"l_x{r} = g_xmm[{r}]" for r in regs)
        out.append("#undef RECOMP_REGS_OUT\n#undef RECOMP_REGS_IN\n"
                   f"#define RECOMP_REGS_OUT() (RECOMP_GPR_OUT(), {wb})\n"
                   f"#define RECOMP_REGS_IN() (RECOMP_GPR_IN(), {rl})")
        out.append(line)
        for l in body:
            l = USE.sub(lambda m: f"l_x{m.group(1)}", l)
            if l.strip() == "RECOMP_REGS_LOCALS":
                l += "\n    V128 " + ", ".join(f"l_x{r} = g_xmm[{r}]" for r in regs) + ";"
            out.append(l)
        out.append("}")
        out.append("#undef RECOMP_REGS_OUT\n#undef RECOMP_REGS_IN\n"
                   "#define RECOMP_REGS_OUT() RECOMP_GPR_OUT()\n#define RECOMP_REGS_IN() RECOMP_GPR_IN()")
        changed += 1
        i = j + 1
    text = "\n".join(out)
    k = text.index('#include "recomp_types.h"\n') + len('#include "recomp_types.h"\n')
    return text[:k] + MARK + "\n" + text[k:], changed


def main(gen):
    files = funcs = 0
    for fn in sorted(os.listdir(gen)):
        if not (fn.startswith("recomp_") and fn.endswith(".c")):
            continue
        p = os.path.join(gen, fn)
        text = open(p).read()
        if '#include "recomp_types.h"' not in text:
            continue
        r = process(text)
        if r is None:
            continue
        new, n = r
        if n:
            open(p, "w", newline="\n").write(new); files += 1; funcs += n
    print(f"xmm_locals: {funcs} functions in {files} files")


if __name__ == "__main__":
    main(sys.argv[1])
