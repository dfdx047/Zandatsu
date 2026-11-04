#!/usr/bin/env python3
"""Lê o perfil de CPU de um zip do teste de GPU (logs/prof_<variante>/, profiler.cpp) e diz para onde vai o tempo
da thread principal do jogo: a correr, na fila à espera de núcleo, ou a dormir, e onde (pilhas simbolizadas).

    python3 tools/diag-profile.py zandatsu-diag_X.zip [--symbols DIR ...] [--threads N] [--stacks N]

Os símbolos: .so sem strip com o mesmo build-id (por omissão as pastas do build e out/release-*/symbols).
Só biblioteca padrão + o llvm-symbolizer do NDK. O zip não é confiável: lido em memória, nada é extraído."""
import argparse, csv, io, json, os, re, struct, subprocess, sys, zipfile
from collections import Counter, defaultdict
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
MAX_MEMBER = 20 * 1024 * 1024
TICK = 100  # USER_HZ


def member(zf, name):
    info = zf.getinfo(name)
    if info.file_size > MAX_MEMBER:
        raise ValueError(f"{name} grande demais")
    with zf.open(info) as f:
        return f.read(MAX_MEMBER + 1).decode("utf-8", "replace")


def build_id(path):
    """NT_GNU_BUILD_ID de um ELF64 little-endian (sem dependências)."""
    try:
        with open(path, "rb") as f:
            h = f.read(64)
            if h[:4] != b"\x7fELF" or h[4] != 2:
                return None
            shoff, = struct.unpack_from("<Q", h, 0x28)
            shentsize, shnum = struct.unpack_from("<HH", h, 0x3A)
            f.seek(shoff)
            sh = f.read(shentsize * shnum)
            for i in range(shnum):
                stype, = struct.unpack_from("<I", sh, i * shentsize + 4)
                if stype != 7:  # SHT_NOTE
                    continue
                off, size = struct.unpack_from("<QQ", sh, i * shentsize + 0x18)
                f.seek(off)
                d = f.read(size)
                p = 0
                while p + 12 <= len(d):
                    nsz, dsz, typ = struct.unpack_from("<III", d, p)
                    name = d[p + 12:p + 12 + nsz]
                    desc_at = p + 12 + ((nsz + 3) & ~3)
                    if typ == 3 and name[:3] == b"GNU":
                        return d[desc_at:desc_at + dsz].hex()
                    p = desc_at + ((dsz + 3) & ~3)
    except OSError:
        return None
    return None


def find_symbolizer():
    for base in [os.environ.get("ANDROID_NDK_HOME"), *sorted(Path.home().glob("Android/Sdk/ndk/*"), reverse=True)]:
        if base:
            p = Path(base) / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-symbolizer"
            if p.is_file():
                return str(p)
    return None


class Symbols:
    def __init__(self, dirs):
        self.by_id = {}
        for d in dirs:
            for f in Path(d).glob("*.so") if Path(d).is_dir() else []:
                if f.stat().st_size < 1_000_000:
                    continue
                bid = build_id(f)
                if bid:
                    self.by_id.setdefault(bid, f)
        self.tool = find_symbolizer()
        self.mods = {}  # nome -> caminho local (pelo build-id do zip)
        self.cache = {}

    def bind(self, modules):
        for name, bid in modules.items():
            if bid in self.by_id:
                self.mods[name] = self.by_id[bid]

    def resolve(self, addrs):
        """addrs: conjunto de "mod+0xoff" -> nome curto da função."""
        per = defaultdict(list)
        for a in addrs:
            if a in self.cache or "+0x" not in a:
                continue
            m, off = a.split("+0x")
            if m in self.mods and self.tool:
                per[m].append((a, int(off, 16)))
        for m, items in per.items():
            # pc - 1 nos endereços de retorno: a chamada, não a instrução seguinte.
            inp = "\n".join(hex(max(o - 1, 0)) for _, o in items)
            try:
                out = subprocess.run([self.tool, "--obj", str(self.mods[m]), "--functions=linkage", "--no-inlines",
                                      "--output-style=JSON", "--demangle"], input=inp, capture_output=True,
                                     text=True, timeout=600).stdout
            except Exception as e:
                print(f"aviso: symbolizer {m}: {e}", file=sys.stderr)
                continue
            lines = [l for l in out.splitlines() if l.strip()]
            for (a, _), l in zip(items, lines):
                try:
                    sym = json.loads(l)["Symbol"][0]
                    fn = sym.get("FunctionName") or "?"
                except Exception:
                    fn = "?"
                n = short(fn) if fn not in ("?", "??") else ""
                self.cache[a] = n if n.strip() else a  # sem nome no release (inline/local): fica modulo+offset
        for a in addrs:
            self.cache.setdefault(a, a)

    def name(self, a):
        return self.cache.get(a, a)


def short(fn):
    fn = re.sub(r"std::__ndk1::", "std::", fn)
    fn = re.sub(r"\(.*\)( const)?$", "", fn) if "(" in fn else fn
    # tira argumentos de template longos
    while True:
        n = re.sub(r"<[^<>]{0,400}>", "", fn)
        if n == fn:
            break
        fn = n
    fn = fn.replace("(anonymous namespace)::", "")
    return fn[-80:]


NOISE = re.compile(r"^(libc\.so|libc\+\+_shared\.so|libm\.so)\+|^(std::|__|syscall$|futex|pthread_|nanosleep|clock_nanosleep|usleep)")


def condense(frames, syms, n=7):
    names = [syms.name(f) for f in frames]
    out, i = [], 0
    # a folha (a chamada ao sistema) e depois só o que é do jogo/runtime
    leaf = names[0] if names else "?"
    rest = [x for x in names[1:] if not NOISE.search(x)]
    out = [leaf.split("+")[0] if leaf.startswith(("libc.so", "libc++")) else leaf] + rest[:n]
    dedup = []
    for x in out:
        if not dedup or dedup[-1] != x:
            dedup.append(x)
    return " < ".join(dedup)


def parse_csv(text):
    return list(csv.DictReader(io.StringIO(text)))


def placement(log_text):
    """tids da thread principal e da de render, das linhas do Renut."""
    main = render = None
    for l in log_text.splitlines():
        m = re.search(r"\[t(\d+)\] Renut: thread principal do jogo nos n", l)
        if m:
            main = int(m.group(1))
        m = re.search(r"\[t(\d+)\] Renut: thread de render nos n", l)
        if m:
            render = int(m.group(1))
    return main, render


def analyze_variant(zf, names, v, summary, syms, nthreads, nstacks):
    base = f"logs/prof_{v}/"
    if base + "threads.csv" not in names:
        print(f"\n## {v}: sem threads.csv")
        return None
    rows = parse_csv(member(zf, base + "threads.csv"))
    freq = parse_csv(member(zf, base + "cpufreq.csv")) if base + "cpufreq.csv" in names else []
    stacks = member(zf, base + "stacks.txt") if base + "stacks.txt" in names else ""
    go_txt = member(zf, base + "process_go.txt") if base + "process_go.txt" in names else ""
    log = member(zf, f"logs/perf_{v}.log") if f"logs/perf_{v}.log" in names else ""
    go = int(re.search(r"t_ms (\d+)", go_txt).group(1)) if "t_ms" in go_txt else int(rows[0]["t_ms"])
    end = max(int(r["t_ms"]) for r in rows)
    wall_ns = (end - go) * 1e6
    perf = summary.get("perf") or {}
    vinfo = perf if perf.get("variant", "base") == v else next(
        (x for x in perf.get("variants") or [] if x.get("variant") == v), {})
    print(f"\n## {v}  ({vinfo.get('message', '?')})")
    for k in ("cpuset", "Cpus_allowed_list", "sampling"):
        m = re.search(rf"^{k}:?\s+(.*)$", go_txt, re.M)
        if m:
            print(f"   {k}: {m.group(1)}")
    if wall_ns <= 0:
        print("   janela vazia")
        return None
    # por thread: o primeiro registo >= go e o ultimo
    first, last = {}, {}
    for r in rows:
        tid, t = int(r["tid"]), int(r["t_ms"])
        if t < go:
            first[tid] = r  # o ultimo antes do go serve de inicio
            continue
        first.setdefault(tid, r)
        last[tid] = r
    stats = []
    for tid, b in last.items():
        a = first.get(tid, b)
        d = lambda k: float(b[k]) - float(a[k])
        span = (int(b["t_ms"]) - int(a["t_ms"])) * 1e6 or wall_ns
        stats.append(dict(tid=tid, comm=b["comm"], run=d("run_ns") / span * 100, wait=d("wait_ns") / span * 100,
                          vcsw=d("vcsw") / (span / 1e9), nvcsw=d("nvcsw") / (span / 1e9), majflt=d("majflt"),
                          minflt=d("minflt") / (span / 1e9), slices=d("slices") / (span / 1e9),
                          cpu=b["cpu"], allowed=b["allowed"], prio=b["prio"]))
    stats.sort(key=lambda s: -s["run"])
    main, render = placement(log)
    if main is None:
        mains = [s for s in stats if s["comm"].startswith("Main XThread")]
        main = mains[0]["tid"] if mains else None
    print(f"   janela {(end - go) / 1000:.0f} s; thread principal t{main}, render t{render}")
    print(f"   {'thread':<24} {'corre%':>6} {'fila%':>6} {'dorme%':>6} {'vol/s':>6} {'forç/s':>6} {'fatias/s':>8} "
          f"{'minflt/s':>8} {'majflt':>6} {'núcleos':<10} cpu")
    show = stats[:nthreads] + [s for s in stats[nthreads:] if s["tid"] in (main, render)]
    for s in show:
        tag = " ◀ principal" if s["tid"] == main else " ◀ render" if s["tid"] == render else ""
        sleep = max(0.0, 100 - s["run"] - s["wait"])
        print(f"   {s['comm'][:16] + ' ' + str(s['tid']):<24} {s['run']:6.1f} {s['wait']:6.1f} {sleep:6.1f} "
              f"{s['vcsw']:6.0f} {s['nvcsw']:6.0f} {s['slices']:8.0f} {s['minflt']:8.0f} {s['majflt']:6.0f} "
              f"{s['allowed'].strip(chr(34))[:10]:<10} {s['cpu']}{tag}")
    tot = sum(s["run"] for s in stats)
    print(f"   soma das threads: {tot / 100:.2f} núcleos; na fila: {sum(s['wait'] for s in stats) / 100:.2f}")
    # frequencias
    cur = [r for r in freq if r.get("kind") == "cur" and int(r["t_ms"]) >= go]
    cap = [r for r in freq if r.get("kind") == "max" and int(r["t_ms"]) >= go]
    if cur:
        cpus = [k for k in cur[0] if k.startswith("cpu")]
        print("   frequência MHz (média / mín / máx; limite mín):", end="")
        for c in cpus:
            vals = [int(r[c]) / 1000 for r in cur if r[c].lstrip("-").isdigit()]
            caps = [int(r[c]) / 1000 for r in cap if r[c].lstrip("-").isdigit()]
            if vals:
                print(f"\n     {c}: {sum(vals) / len(vals):5.0f} / {min(vals):5.0f} / {max(vals):5.0f}"
                      f"{f'; limite {min(caps):.0f}' if caps else ''}", end="")
        print()
    # pilhas
    blocks = re.split(r"^thread ", stacks, flags=re.M)
    head = blocks[0]
    modules = dict(re.findall(r"^module (\S+) (\S+)", head, re.M))
    syms.bind(modules)
    m = re.search(r"amostras (\d+) nao_entregues (\d+)", head)
    if m:
        print(f"   pilhas: {m.group(1)} amostras, {m.group(2)} sinais não entregues em 3 ms")
    missing = [n for n in ("libgame.so", "librexruntime.so") if n in modules and n not in syms.mods]
    if missing:
        print(f"   sem símbolos para: {', '.join(f'{n} {modules[n]}' for n in missing)}")
    parsed = []
    for blk in blocks[1:]:
        lines = blk.splitlines()
        hm = re.match(r"(\d+) (.*?) estados (.*?) nucleos(.*)$", lines[0])
        if not hm:
            continue
        tid = int(hm.group(1))
        entries = []
        for l in lines[1:]:
            em = re.match(r"\s+(\d+) (\S) (.*)$", l)
            if em:
                key = em.group(3)
                fr, _, scan = key.partition(" |scan;")
                entries.append((int(em.group(1)), em.group(2), fr.split(";"), scan.split(";") if scan else []))
        parsed.append((tid, hm.group(2), hm.group(3), hm.group(4).strip(), entries))
    want = {main, render} | {s["tid"] for s in stats[:4]}
    addrs = set()
    for tid, _, _, _, entries in parsed:
        if tid in want:
            for _, _, fr, sc in entries[:nstacks * 4]:
                addrs.update(fr)
                addrs.update(sc)
    syms.resolve(addrs)
    for tid, comm, states, cores, entries in sorted(parsed, key=lambda p: (p[0] != main, p[0] != render, p[0])):
        if tid not in want:
            continue
        total = sum(e[0] for e in entries) or 1
        tag = "principal" if tid == main else "render" if tid == render else "ocupada"
        print(f"\n   pilhas t{tid} {comm} ({tag}); estados {states}; núcleos {cores}")
        agg = Counter()
        for n, st, fr, sc in entries:
            agg[(st, condense(fr, syms) + (" [scan: " + condense(sc, syms, 4) + "]" if sc and len(fr) < 5 else ""))] += n
        for (st, k), n in agg.most_common(nstacks):
            print(f"     {n / total * 100:5.1f}% {st} {k}")
    return dict(variant=v, main=next((s for s in stats if s["tid"] == main), None))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("zip")
    ap.add_argument("--symbols", action="append", default=[])
    ap.add_argument("--threads", type=int, default=10)
    ap.add_argument("--stacks", type=int, default=12)
    a = ap.parse_args()
    dirs = a.symbols + [str(ROOT / "app/build/intermediates/cmake/release/obj/arm64-v8a"),
                        str(ROOT / "app/build/intermediates/cmake/debug/obj/arm64-v8a"),
                        *map(str, sorted(ROOT.glob("out/release-*/symbols")))]
    syms = Symbols(dirs)
    with zipfile.ZipFile(a.zip) as zf:
        names = {n for n in zf.namelist() if not PurePosixPath(n).is_absolute() and ".." not in PurePosixPath(n).parts}
        summary = json.loads(member(zf, "summary.json"))
        dev = summary.get("device") or {}
        print(f"# {dev.get('manufacturer', '')} {dev.get('model', '')} — {(summary.get('gpu') or {}).get('name', '')}"
              f" — {(summary.get('app') or {}).get('versionName', '')}")
        if "tests/cpu_cores.json" in names:
            t = json.loads(member(zf, "tests/cpu_cores.json"))
            det = t.get("details") or {}
            print(f"cpu_cores ({det.get('cpuset', '?')}, {det.get('cpusAllowed', '?')}):")
            for c in det.get("cores") or []:
                print(f"   cpu{c['cpu']}: {c['effectiveGHz']:.2f} GHz efetivos (máx {c['maxKHz'] / 1e6:.2f}, limite "
                      f"{c['capKHz'] / 1e6:.2f}); memória {c['memLatencyNs']:.0f} ns; bswap {c['bswapStreamNsPerWord']:.2f} ns/palavra")
            w = det.get("wakeRoundTripUs") or []
            if w:
                n = max(max(x["a"], x["b"]) for x in w) + 1
                print("   acordar (ida e volta, mediana em µs; linha = quem acorda, coluna = quem é acordado):")
                print("        " + "".join(f"{b:>6}" for b in range(n)))
                m = {(x["a"], x["b"]): x for x in w}
                for a_ in range(n):
                    print(f"   {a_:>4} " + "".join(f"{m[(a_, b)]['p50']:6.0f}" if (a_, b) in m else "     -" for b in range(n)))
                slow = sorted(w, key=lambda x: -x["p95"])[:3]
                print("   p95 piores: " + ", ".join(f"{x['a']}->{x['b']} {x['p95']:.0f} µs" for x in slow))
            sl = det.get("sleep1msOverUs") or []
            if sl:
                print("   sleep de 1 ms, atraso (mediana/p95/máx µs): " +
                      ", ".join(f"cpu{x['cpu']} {x['p50']:.0f}/{x['p95']:.0f}/{x['max']:.0f}" for x in sl))
        variants = sorted({n.split("/")[1][5:] for n in names if n.startswith("logs/prof_") and n.count("/") >= 2},
                          key=lambda v: (v != "base", v))
        res = [analyze_variant(zf, names, v, summary, syms, a.threads, a.stacks) for v in variants]
        perf = summary.get("perf") or {}
        print("\n## resumo das variantes")
        allv = [perf] + list(perf.get("variants") or [])
        for x in allv:
            print(f"   {x.get('variant', 'base'):<9} {x.get('fpsAvg', '?')} fps, principal {x.get('mainThreadMsAvg', '?')} ms, "
                  f"render {x.get('renderThreadMsAvg', '?')} ms, GPU {x.get('gpuMsAvg', '?')} ms")
        for r in res:
            if r and r["main"]:
                s = r["main"]
                print(f"   {r['variant']:<9} principal: corre {s['run']:.0f}%, fila {s['wait']:.0f}%, "
                      f"dorme {max(0, 100 - s['run'] - s['wait']):.0f}%, {s['nvcsw']:.0f} trocas forçadas/s")


if __name__ == "__main__":
    main()
