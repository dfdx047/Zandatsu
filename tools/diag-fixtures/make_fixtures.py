#!/usr/bin/env python3
"""Gera zips falsos em tools/diag-fixtures/ para o autoteste do diag-analyze.py."""
import json, zipfile
from pathlib import Path

OUT = Path(__file__).resolve().parent
IDS = ["info", "pipelines_serial", "pipelines_parallel", "game_shaders_serial",
       "game_shaders_parallel", "viewport_scissor", "clear_loadop", "bc_decode", "timestamps"]


def make(fname, gpu, family, mfr, model, soc, drv, vkinfo, over, perf):
    tests = [{"id": i, "status": "pass", "durationMs": 100} for i in IDS]
    for t in tests:
        t.update(over.get(t["id"], {}))
    s = {"schema": 1, "createdAt": "2026-11-04T21:10:00+00:00",
         "app": {"versionName": "1.2.1", "versionCode": 4, "build": "Release@20261104_2100"},
         "device": {"manufacturer": mfr, "model": model, "soc": soc, "android": "15", "api": 35},
         "gpu": {"name": gpu, "family": family, "knownProblem": bool(over)},
         "driver": {"active": drv}, "vulkan": {"driverInfo": vkinfo},
         "tests": tests, "perf": perf}
    with zipfile.ZipFile(OUT / fname, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("summary.json", json.dumps(s))
        for t in tests:
            z.writestr(f"tests/{t['id']}.json", json.dumps(t))
        z.writestr("logs/app.log", "log de exemplo\n")


make("zandatsu-diag_powervr-bxm_20261104-2110.zip", "PowerVR BXM-8-256", "powervr-bxm", "Xiaomi", "POCO X", "MT8791",
     "system", "23.3@6603887",
     {"pipelines_parallel": {"status": "crash", "crashedAt": "pipelines_parallel/thread4/create_graphics",
                             "message": "SIGSEGV"}},
     {"renderer": "xenos", "status": "pass", "fpsAvg": 12.3, "gpuMsAvg": 40.1, "mainThreadMsAvg": 9.0,
      "fallbackFrom": "renut"})
make("zandatsu-diag_mali-g720_20261104-2120.zip", "Mali-G720 MC7", "mali-g720", "Xiaomi", "2412DPC0AG", "MT6899",
     "system", "v1.r49p1-03bet0",
     {"viewport_scissor": {"status": "fail", "message": "scissor ignorado em minimap"}},
     {"renderer": "renut", "status": "pass", "fpsAvg": 29.8, "gpuMsAvg": 14.2, "mainThreadMsAvg": 8.1})
make("zandatsu-diag_adreno-650_20261104-2130.zip", "Adreno 650", "adreno-6xx", "Retroid", "Pocket 5", "SD865",
     "turnip", "Mesa 25.2", {},
     {"renderer": "renut", "status": "pass", "fpsAvg": 60.0, "gpuMsAvg": 9.5, "mainThreadMsAvg": 6.0})
(OUT / "zandatsu-diag_quebrado_20261104-2140.zip").write_bytes(b"isto nao e um zip")
