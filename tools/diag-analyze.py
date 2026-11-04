#!/usr/bin/env python3
"""Analisa zips de diagnóstico do Zandatsu (GPU test). Só biblioteca padrão.
Os zips são não confiáveis: nada é extraído para o disco, tudo é lido em memória."""
import argparse, csv, hashlib, json, sys, zipfile
from collections import Counter, defaultdict
from pathlib import Path, PurePosixPath

MAX_MEMBER = 20 * 1024 * 1024
TEST_ORDER = ["info", "pipelines_serial", "pipelines_parallel", "game_shaders_serial",
              "game_shaders_parallel", "viewport_scissor", "clear_loadop", "bc_decode", "timestamps", "cpu_cores"]
ICON = {"pass": "✔", "fail": "✘", "crash": "💥", "timeout": "⏱"}


def err(msg):
    print(f"aviso: {msg}", file=sys.stderr)


def safe_member(name):
    p = PurePosixPath(name.replace("\\", "/"))
    return not p.is_absolute() and ".." not in p.parts


def read_json(zf, name):
    info = zf.getinfo(name)
    if info.file_size > MAX_MEMBER:
        raise ValueError(f"{name}: maior que {MAX_MEMBER // 2**20} MB")
    with zf.open(info) as f:
        data = f.read(MAX_MEMBER + 1)
    if len(data) > MAX_MEMBER:
        raise ValueError(f"{name}: maior que {MAX_MEMBER // 2**20} MB")
    return data


def load_zip(path):
    """Devolve (relatório, sha1 do summary.json) ou levanta exceção."""
    with zipfile.ZipFile(path) as zf:
        names = {n for n in zf.namelist() if safe_member(n)}
        if "summary.json" not in names:
            raise ValueError("sem summary.json")
        raw = read_json(zf, "summary.json")
        s = json.loads(raw.decode("utf-8", "replace"))
        if not isinstance(s, dict):
            raise ValueError("summary.json não é um objeto")
        tests = {}
        for t in s.get("tests") or []:
            if isinstance(t, dict):
                tests[str(t.get("id", "?"))] = t
        for n in sorted(names):  # tests/<id>.json completa o que faltar no summary
            if n.startswith("tests/") and n.endswith(".json"):
                try:
                    t = json.loads(read_json(zf, n).decode("utf-8", "replace"))
                    if isinstance(t, dict):
                        tests.setdefault(str(t.get("id", Path(n).stem)), t)
                except Exception as e:
                    err(f"{path}: {n}: {e}")
        s["_tests"] = tests
        return s, hashlib.sha1(raw).hexdigest()


def g(d, *keys, default=""):
    for k in keys:
        d = d.get(k) if isinstance(d, dict) else None
        if d is None:
            return default
    return d


def ordered_ids(reports):
    seen = {i for r in reports for i in r["_tests"]}
    return TEST_ORDER + sorted(seen - set(TEST_ORDER))


def status(t):
    return str(t.get("status", "?")) if t else "-"


def cell(t, notes):
    st = status(t)
    if st == "crash":
        notes.append(t.get("crashedAt") or "?")
        return f"💥[{len(notes)}]"
    return ICON.get(st, "–" if st in ("-", "skip") else "?")


def fmt_perf(p):
    if not p:
        return "–"
    f = lambda k: "?" if p.get(k) is None else f"{p[k]:.1f}" if isinstance(p[k], (int, float)) else str(p[k])
    s = f"{f('fpsAvg')} / {f('gpuMsAvg')} / {f('mainThreadMsAvg')}, {p.get('renderer', '?')}"
    s += " (fallback)" if p.get("fallbackFrom") else ""
    # O jogador abriu o menu do app durante o teste: as janelas com pausa ficaram de fora (1.2.1+).
    s += f" ⚠ {p['pauses']} pausa(s), {p.get('seconds', '?')} s" if p.get("pauses") else ""
    # Abaixo de 30 fps o teste repete com uma coisa mudada de cada vez (1.2.1+, detalhe: tools/diag-profile.py).
    for v in p.get("variants") or []:
        fps = v.get("fpsAvg")
        s += f"; {v.get('variant')} {fps:.1f}" if isinstance(fps, (int, float)) else f"; {v.get('variant')} {v.get('status')}"
    return s + ("" if p.get("status", "pass") == "pass" else f" [{p['status']}]")


def row_info(name, s):
    return {
        "arquivo": name, "gpu": g(s, "gpu", "name"), "familia": g(s, "gpu", "family", default="desconhecida"),
        "aparelho": f"{g(s, 'device', 'manufacturer')} {g(s, 'device', 'model')}".strip(),
        "soc": g(s, "device", "soc"), "android": g(s, "device", "android"),
        "driver": f"{g(s, 'driver', 'active')} {g(s, 'vulkan', 'driverInfo')}".strip(),
        "app": f"{g(s, 'app', 'versionName')} ({g(s, 'app', 'versionCode')})",
    }


def table(rows, ids):
    notes = []
    head = ["GPU", "aparelho", "SoC", "Android", "driver", "app"] + ids + ["perf (fps / gpu ms / main ms)"]
    out = ["| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for name, s in rows:
        i = row_info(name, s)
        cols = [i["gpu"], i["aparelho"], i["soc"], i["android"], i["driver"], i["app"]]
        cols += [cell(s["_tests"].get(t), notes) for t in ids] + [fmt_perf(s.get("perf"))]
        out.append("| " + " | ".join(str(c).replace("|", "/") for c in cols) + " |")
    if notes:
        out += ["", "Quedas:"] + [f"[{n}] {t}" for n, t in enumerate(notes, 1)]
    return "\n".join(out)


def aggregate(rows, ids):
    by = defaultdict(list)
    for name, s in rows:
        by[row_info(name, s)["familia"]].append(s)
    res = {}
    for fam, reps in by.items():
        tests = {}
        for t in ids:
            c, crash, msg = Counter(), Counter(), Counter()
            for s in reps:
                x = s["_tests"].get(t)
                if not x:
                    continue
                c[status(x)] += 1
                if status(x) == "crash":
                    crash[x.get("crashedAt") or "?"] += 1
                if status(x) in ("fail", "timeout", "crash") and x.get("message"):
                    msg[str(x["message"])] += 1
            if c:
                tests[t] = {"contagem": dict(c), "crashedAt": crash.most_common(3), "mensagens": msg.most_common(3)}
        res[fam] = {"relatorios": len(reps), "testes": tests}
    return res


def print_agg(agg):
    print("\n## Por GPU")
    for fam in sorted(agg):
        a = agg[fam]
        print(f"\n### {fam} ({a['relatorios']} relatório(s))")
        for t, d in a["testes"].items():
            c = d["contagem"]
            print(f"- {t}: ✔{c.get('pass', 0)} ✘{c.get('fail', 0)} 💥{c.get('crash', 0)}"
                  + (f" ⏱{c['timeout']}" if c.get("timeout") else "") + (f" –{c['skip']}" if c.get("skip") else ""))
            for k, n in d["crashedAt"]:
                print(f"    queda em {k}: {n}x")
            for k, n in d["mensagens"]:
                print(f"    msg \"{k[:100]}\": {n}x")


def main():
    ap = argparse.ArgumentParser(description="Analisa zips zandatsu-diag_*.zip e mostra uma tabela por GPU.")
    ap.add_argument("entradas", nargs="+", help="zips ou pastas com zips")
    ap.add_argument("--json", metavar="ARQ", help="grava dados agregados em JSON")
    ap.add_argument("--csv", metavar="ARQ", help="grava uma linha por zip em CSV")
    a = ap.parse_args()

    files = []
    for e in a.entradas:
        p = Path(e)
        files += sorted(p.rglob("*.zip")) if p.is_dir() else [p]
    rows, seen = [], {}
    for f in files:
        try:
            s, sha = load_zip(f)
        except Exception as ex:
            err(f"{f}: {ex}")
            continue
        if sha in seen:
            err(f"{f}: duplicado de {seen[sha]}, ignorado")
            continue
        seen[sha] = f
        rows.append((f.name, s))
    if not rows:
        print("nenhum relatório válido", file=sys.stderr)
        return 1
    rows.sort(key=lambda r: (row_info(*r)["familia"], row_info(*r)["aparelho"], r[0]))
    ids = ordered_ids([s for _, s in rows])
    print(f"{len(rows)} relatório(s) únicos de {len(files)} arquivo(s)\n")
    print(table(rows, ids))
    agg = aggregate(rows, ids)
    print_agg(agg)
    if a.json:
        Path(a.json).write_text(json.dumps(agg, ensure_ascii=False, indent=1), encoding="utf-8")
    if a.csv:
        with open(a.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["arquivo", "familia", "gpu", "aparelho", "soc", "android", "driver", "app"] + ids
                       + ["fpsAvg", "gpuMsAvg", "mainThreadMsAvg", "renderer"])
            for name, s in rows:
                i, p = row_info(name, s), s.get("perf") or {}
                w.writerow([name, i["familia"], i["gpu"], i["aparelho"], i["soc"], i["android"], i["driver"], i["app"]]
                           + [status(s["_tests"].get(t)) for t in ids]
                           + [p.get("fpsAvg", ""), p.get("gpuMsAvg", ""), p.get("mainThreadMsAvg", ""), p.get("renderer", "")])
    return 0


if __name__ == "__main__":
    sys.exit(main())
