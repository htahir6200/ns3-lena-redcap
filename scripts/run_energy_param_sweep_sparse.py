#!/usr/bin/env python3
"""E5b: sparse-traffic OFAT (IAT=10 s) — paper-like sleep-dominated regime.

At IAT≈1 s + RAI, release/reconnect thrashing dominates energy/PDR variance.
This companion sweep holds IAT=10 s (sim 120 s) so PTW / eDRX / RAI / DIT
effects are readable, matching the Guidelines / Sensors sparse UL style.
"""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "scripts" / "runner_redcap.py"
RESULTS = ROOT / "results" / "campaigns"
CONSOLE = RESULTS / "energy_e5b_sparse_console.log"


def run(cmd: list[str]) -> int:
    print(f"\n=== {time.strftime('%F %T')} {' '.join(cmd[2:])}", flush=True)
    with CONSOLE.open("a") as log:
        log.write(f"\n# {' '.join(cmd)}\n")
        log.flush()
        return subprocess.call(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)


def base_cmd(
    campaign: str,
    *,
    seeds: int,
    workers: int,
    dit: int,
    ptw: bool,
    ptw_ms: int,
    rai: bool,
    edrx_fr: int,
) -> list[str]:
    cmd = [
        sys.executable,
        str(RUNNER),
        "--campaign",
        campaign,
        "--configs",
        "3",
        "--seeds",
        str(seeds),
        "--ues",
        "20",
        "--packet-size",
        "5000",
        "--interval",
        "10.0",
        "--sim-time",
        "120",
        "--inactivity-timer",
        str(dit),
        "--workers",
        str(workers),
        "--true-5mhz",
        "true",
        "--edrx-cycle-frames",
        str(edrx_fr),
        "--ptw-length-ms",
        str(ptw_ms),
        "--skip-existing",
    ]
    if ptw:
        cmd.append("--enable-edrx-ptw")
    if rai:
        cmd.append("--enable-rai")
    return cmd


def main() -> int:
    workers = int(os.environ.get("WORKERS", "4"))
    seeds = int(os.environ.get("SEEDS", "3"))
    RESULTS.mkdir(parents=True, exist_ok=True)
    CONSOLE.write_text(f"# E5b sparse OFAT started {time.strftime('%F %T')}\n")

    jobs: list[list[str]] = []
    base_dit = 1000
    base_ptw = 1280
    base_edrx = 1024

    for ptw_ms in (256, 1280, 2560):
        jobs.append(
            base_cmd(
                f"ul_en_e5b_ptw_p{ptw_ms}_edrx{base_edrx}_dit{base_dit}",
                seeds=seeds,
                workers=workers,
                dit=base_dit,
                ptw=True,
                ptw_ms=ptw_ms,
                rai=True,
                edrx_fr=base_edrx,
            )
        )
    for edrx in (256, 512, 1024, 2048):
        jobs.append(
            base_cmd(
                f"ul_en_e5b_edrx_e{edrx}_ptw{base_ptw}_dit{base_dit}",
                seeds=seeds,
                workers=workers,
                dit=base_dit,
                ptw=True,
                ptw_ms=base_ptw,
                rai=True,
                edrx_fr=edrx,
            )
        )
    for dit in (1000, 5000, 15000, 20000):
        jobs.append(
            base_cmd(
                f"ul_en_e5b_dit_d{dit}_ptw{base_ptw}_edrx{base_edrx}",
                seeds=seeds,
                workers=workers,
                dit=dit,
                ptw=True,
                ptw_ms=base_ptw,
                rai=False,
                edrx_fr=base_edrx,
            )
        )
    for dit in (1000, 5000):
        for rai in (False, True):
            rtag = "rai1" if rai else "rai0"
            jobs.append(
                base_cmd(
                    f"ul_en_e5b_rai_{rtag}_dit{dit}_ptw{base_ptw}_edrx{base_edrx}",
                    seeds=seeds,
                    workers=workers,
                    dit=dit,
                    ptw=True,
                    ptw_ms=base_ptw,
                    rai=rai,
                    edrx_fr=base_edrx,
                )
            )

    print(f"Queued {len(jobs)} sparse groups (seeds={seeds})", flush=True)
    rc_all = 0
    for cmd in jobs:
        rc = run(cmd)
        if rc != 0:
            rc_all = rc
    # Reuse summarizer after extending pattern — call e5b summarize below
    subprocess.call([sys.executable, str(ROOT / "scripts" / "summarize_energy_param_sweep.py"), "--prefix", "ul_en_e5b"], cwd=ROOT)
    print(f"E5b sparse finished rc={rc_all}", flush=True)
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
