#!/usr/bin/env python3
"""MGR:R PC lift driver (pcrecomp lift32, global-register model).

Adapted from codepdbh/nfsu2-recompiled scripts/run_lift.py (MIT); see CREDITS.md.

  run_lift.py catalog  <analysis.exe> <seeds.json> <catalog.json>
      Recursive descent (pcrecomp disasm32) to a catalog that keeps every
      function's BASIC BLOCK ranges, so lifting can re-decode without paying
      for the whole discovery again (about 4 minutes on the MGR:R exe).

  run_lift.py lift  <analysis.exe> <catalog.json> <outdir> [--split N] [--names names.json] [--prefix P]
      Lift every catalogued function with lift32.Lifter and write:
        recomp_NNNN.c          the functions, N per file
        recomp_funcs.h         declarations
        recomp_dispatch.c      sorted VA -> function table (binary search)
        recomp_imports.c       IAT slot VA -> "dll!name" (for crash trails)
        lift_stats.json        counts for docs/LIFT_REPORT.md
      --prefix P names a second module's output (P + every file and table
      above, e.g. server_recomp_dispatch_table for server.dll) so it links
      next to the main exe's.

Everything written here is derived from the game binary and is gitignored.
"""
import json, os, sys, time, collections

HERE = os.path.dirname(os.path.abspath(__file__))
PCRECOMP = os.environ.get("PCRECOMP", os.path.normpath(os.path.join(HERE, "..", "..", "build", "lab", "pc", "pcrecomp-pinned")))
for sub in ("pe", "disasm", "lift"):
    sys.path.insert(0, os.path.join(PCRECOMP, "tools", sub))

from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402
from pe_analyze import analyze_pe, build_iat_map   # noqa: E402
import disasm32                                     # noqa: E402
from disasm32 import Disassembler, Function, BasicBlock, Instruction  # noqa: E402


def cmd_catalog(exe, seeds_path, out):
    info = analyze_pe(exe)
    iat = build_iat_map(info)
    data = open(exe, "rb").read()
    seeds = {info.image_base + info.entry_point_rva}
    for e in json.load(open(seeds_path)):
        a = e.get("address") if isinstance(e, dict) else e
        seeds.add(int(a, 0) if isinstance(a, str) else a)
    dis = Disassembler(data, info.image_base, info.sections)
    t = time.time()
    funcs = dis.find_functions(info.code_start, info.code_end, iat, seeds=seeds,
                               release_operands=True)
    print(f"[*] {len(funcs)} functions in {time.time() - t:.0f}s")
    doc = {"exe": os.path.basename(exe), "image_base": info.image_base,
           "code_start": info.code_start, "code_end": info.code_end,
           "functions": []}
    for f in sorted(funcs.values(), key=lambda f: f.address):
        doc["functions"].append({
            "address": f.address, "end": f.end, "size": f.size,
            "entry_kind": f.entry_kind, "is_thunk": f.is_thunk,
            "jump_targets": sorted(f.jump_targets),
            "calls_to": sorted(f.calls_to),
            "blocks": [[b.start, b.end, sorted(set(b.successors)), int(b.is_exit)]
                       for b in sorted(f.blocks.values(), key=lambda b: b.start)],
        })
    json.dump(doc, open(out, "w"))
    print(f"[*] wrote {out}")


def rebuild(md, img, base, fd):
    """A disasm32.Function with full capstone operands, from a catalog entry."""
    f = Function(address=fd["address"], end=fd["end"], size=fd["size"],
                 name=f"sub_{fd['address']:08X}", is_thunk=fd["is_thunk"],
                 entry_kind=fd["entry_kind"])
    f.jump_targets = set(fd["jump_targets"])
    f.calls_to = set(fd["calls_to"])
    for s, e, succ, ex in fd["blocks"]:
        b = BasicBlock(start=s, end=e, successors=list(succ), is_exit=bool(ex))
        for ins in md.disasm(img[s - base:e - base], s):
            b.instructions.append(Instruction(ins.address, ins.size, ins.mnemonic,
                                              ins.op_str, bytes(ins.bytes),
                                              list(ins.operands)))
        f.blocks[s] = b
    return f


def read_va_list(path):
    out = {}
    if path and os.path.exists(path):
        for line in open(path):
            line = line.split("#", 1)[0].strip()
            if line:
                parts = line.split(None, 1)
                out[int(parts[0], 16)] = parts[1] if len(parts) > 1 else ""
    return out


ALIAS_LIFT_LIMIT = int(os.environ.get("ALIAS_LIFT_LIMIT", "1024"))


def cmd_lift(exe, catalog, outdir, split=400, names_path=None, prefix="", shard=None):
    """shard=(i, n): lift only the i-th of n contiguous slices into recomp_<i>_NNNN.c; `index` writes the tables."""
    import pefile
    from lift32 import Lifter
    info = analyze_pe(exe)
    iat = build_iat_map(info)
    module = os.path.basename(exe)
    pe = pefile.PE(exe, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    cat = json.load(open(catalog))
    fds = cat["functions"]
    lifted = {fd["address"] for fd in fds}
    names = {}
    if names_path and os.path.exists(names_path):
        names = {int(k, 16): v for k, v in json.load(open(names_path)).items()}
    cfg = os.path.join(HERE, "..", "config")
    overrides = read_va_list(os.path.join(cfg, "overrides.txt"))
    excluded = read_va_list(os.path.join(cfg, "exclude.txt"))
    # Each exe build has its own hook sites (config/hooks-<build>.txt);
    # MGR_EXE_BUILD selects one, the documented build uses hooks.txt.
    build = os.environ.get("MGR_EXE_BUILD", "")
    hooks = read_va_list(os.path.join(cfg, f"hooks-{build}.txt" if build else "hooks.txt"))
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    lifter = Lifter(iat_map=iat, func_names={}, lifted=lifted, precise_carry=True,
                    hook_sites=set(hooks))
    os.makedirs(outdir, exist_ok=True)
    tag = f"{shard[0]}_" if shard else ""
    for fn in os.listdir(outdir):
        if fn.startswith(prefix + "recomp_" + tag) and fn.endswith((".c", ".h")) and (shard or True):
            if shard and not fn[len(prefix + "recomp_" + tag):][:4].isdigit():
                continue
            os.remove(os.path.join(outdir, fn))
    if shard:
        i, n = shard
        per = (len(fds) + n - 1) // n
        fds = fds[i * per:(i + 1) * per]

    stats = collections.Counter()
    unimpl = collections.Counter()
    unimpl_sites = collections.defaultdict(list)
    failed = []
    entries = []
    t0 = time.time()
    chunk, idx, lines_total = [], 0, 0

    def flush():
        nonlocal chunk, idx, lines_total
        if not chunk:
            return
        p = os.path.join(outdir, f"{prefix}recomp_{tag}{idx:04d}.c")
        with open(p, "w", newline="\n") as fh:
            fh.write(f"/* AUTO-GENERATED from {module} by run_lift.py (pcrecomp lift32). DO NOT EDIT, DO NOT COMMIT. */\n")
            fh.write(f'#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n#include "{prefix}recomp_funcs.h"\n#include <math.h>\n#include <string.h>\n\n')
            body = "\n\n".join(chunk)
            fh.write(body)
            lines_total += body.count("\n")
        idx += 1
        chunk = []

    for n, fd in enumerate(fds):
        a = fd["address"]
        try:
            if a in overrides:
                code = (f"/* OVERRIDE: {overrides[a]} */\n"
                        f"void sub_{a:08X}(void) {{ recomp_func_t _f = recomp_lookup_manual(0x{a:08X}u);"
                        f" if (_f) {{ _f(); return; }} RECOMP_UNIMPL(0x{a:08X}u, \"override missing\"); }}")
                stats["overridden"] += 1
            elif fd.get("entry_kind") == "alias" and sum(e - b for b, e, *_ in fd["blocks"]) > ALIAS_LIFT_LIMIT:
                # A label inside another function's body, found through a data pointer: lifting it lifts the rest
                # of the parent again (MGR: 25,221 aliases = 60% of the lifted bytes). Loud stub until a run proves
                # one is a real entry (PCPorts ports/mgr-pc/PROBLEMAS.md).
                code = (f"/* ALIAS of a larger body, not lifted ({sum(e - b for b, e, *_ in fd['blocks'])} bytes of blocks) */\n"
                        f"void sub_{a:08X}(void) {{ RECOMP_UNIMPL(0x{a:08X}u, \"alias entry not lifted\"); }}")
                stats["alias_stubs"] += 1
            elif a in excluded:
                code = (f"/* EXCLUDED: {excluded[a]} */\n"
                        f"void sub_{a:08X}(void) {{ RECOMP_UNIMPL(0x{a:08X}u, \"excluded: not code\"); }}")
                stats["excluded"] += 1
            else:
                f = rebuild(md, img, base, fd)
                code = lifter.lift_function(f)
        except Exception as ex:  # noqa: BLE001
            failed.append((a, repr(ex)))
            code = (f"/* LIFT FAILED: {ex!r} */\n"
                    f"void sub_{a:08X}(void) {{ RECOMP_UNIMPL(0x{a:08X}u, \"lift failed\"); }}")
        for line in code.splitlines():
            if "RECOMP_UNIMPL(" in line:
                key = line.split('RECOMP_UNIMPL(', 1)[1].split('"')[1].split()[0] if '"' in line else "?"
                unimpl[key] += 1
                if len(unimpl_sites[key]) < 10:
                    unimpl_sites[key].append(f"{a:08X}")
            if "RECOMP_ICALL(" in line: stats["icall_sites"] += 1
            if "RECOMP_ITAIL(" in line: stats["itail_sites"] += 1
        if names.get(a):
            code = f"/* {names[a]} */\n" + code
        chunk.append(code)
        entries.append(a)
        if len(chunk) >= split:
            flush()
            el = time.time() - t0
            print(f"[*] {n + 1}/{len(fds)} lifted ({el:.0f}s)", flush=True)
    flush()
    if shard:
        out = {"functions": len(entries), "files": idx, "c_lines": lines_total, "lift_failures": len(failed),
               "failures": failed[:200], "unimplemented_total": sum(unimpl.values()), "unimplemented": dict(unimpl.most_common()),
               "unimplemented_sites": dict(unimpl_sites), "overridden": stats["overridden"], "excluded": stats["excluded"], "alias_stubs": stats["alias_stubs"],
               "icall_sites": stats["icall_sites"], "itail_sites": stats["itail_sites"], "seconds": round(time.time() - t0)}
        json.dump(out, open(os.path.join(outdir, f"{prefix}lift_stats_{shard[0]}.json"), "w"), indent=1)
        print(json.dumps({k: v for k, v in out.items() if k not in ("failures", "unimplemented_sites", "unimplemented")}))
        return
    write_tables(outdir, prefix, entries, iat)


def write_tables(outdir, prefix, entries, iat):
    with open(os.path.join(outdir, f"{prefix}recomp_funcs.h"), "w", newline="\n") as fh:
        fh.write("/* AUTO-GENERATED. DO NOT EDIT. */\n#pragma once\n#include <stdint.h>\n")
        for a in entries:
            fh.write(f"void sub_{a:08X}(void);\n")
    with open(os.path.join(outdir, f"{prefix}recomp_dispatch.c"), "w", newline="\n") as fh:
        fh.write(f'/* AUTO-GENERATED. DO NOT EDIT. */\n#include "recomp_types.h"\n#include "{prefix}recomp_funcs.h"\n\n')
        fh.write(f"const recomp_dispatch_entry_t {prefix}recomp_dispatch_table[] = {{\n")
        for a in sorted(entries):
            fh.write(f"    {{ 0x{a:08X}u, sub_{a:08X} }},\n")
        fh.write("};\n")
        fh.write(f"const uint32_t {prefix}recomp_dispatch_count = {len(entries)};\n")
    with open(os.path.join(outdir, f"{prefix}recomp_imports.c"), "w", newline="\n") as fh:
        fh.write("/* AUTO-GENERATED: IAT slot -> import name, for diagnostics. */\n#include <stdint.h>\n")
        fh.write("typedef struct { uint32_t slot; const char* name; } pc_import_name_t;\n")
        fh.write(f"const pc_import_name_t {prefix}pc_import_names[] = {{\n")
        for slot, (dll, nm) in sorted(iat.items()):
            fh.write(f'    {{ 0x{slot:08X}u, "{dll}!{nm}" }},\n')
        fh.write("    { 0, 0 } };\n")


def cmd_index(exe, catalog, outdir, prefix=""):
    info = analyze_pe(exe)
    iat = build_iat_map(info)
    entries = [fd["address"] for fd in json.load(open(catalog))["functions"]]
    write_tables(outdir, prefix, entries, iat)
    print(f"[*] tables for {len(entries)} functions")


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "catalog":
        cmd_catalog(a[1], a[2], a[3])
    elif a and a[0] == "lift":
        split = int(a[a.index("--split") + 1]) if "--split" in a else 400
        names = a[a.index("--names") + 1] if "--names" in a else None
        prefix = a[a.index("--prefix") + 1] if "--prefix" in a else ""
        shard = tuple(int(x) for x in a[a.index("--shard") + 1].split("/")) if "--shard" in a else None
        cmd_lift(a[1], a[2], a[3], split, names, prefix, shard)
    elif a and a[0] == "index":
        cmd_index(a[1], a[2], a[3], a[a.index("--prefix") + 1] if "--prefix" in a else "")
    else:
        sys.exit(__doc__)
