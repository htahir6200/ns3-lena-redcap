#!/usr/bin/env python3
"""Abbas-style one-factor-at-a-time energy parameter sweep (Phase C / E5).

Mirrors the Guidelines / IEEE energy papers: vary PTW, eDRX cycle, DIT, RAI,
and packet interval around a sleep-facing baseline, rather than only binary
on/off flags.

Baseline (true 5 MHz eRedCap):
  20 UE, 5000 B UL, sim 60 s, interval 1.0 s, DIT 1 s,
  PTW 1280 ms, eDRX 1024 frames (10.24 s), RAI on, PTW on.

Axes (one factor at a time; 3 seeds by default):
  A PTW     ∈ {256, 1280, 2560} ms
  B eDRX    ∈ {256, 512, 1024, 2048} frames  (2.56 … 20.48 s)
  C DIT     ∈ {1000, 5000, 15000, 20000} ms   (RAI off to isolate DIT)
  D RAI     ∈ {off, on} × DIT ∈ {1000, 5000}
  E IAT     ∈ {0.2, 1.0, 5.0, 10.0} s
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
CONSOLE = RESULTS / "energy_e5_param_console.log"


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
    ues: int,
    interval: float,
    sim_time: float,
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
        str(ues),
        "--packet-size",
        "5000",
        "--interval",
        str(interval),
        "--sim-time",
        str(sim_time),
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
    ues = int(os.environ.get("UES", "20"))
    sim_time = float(os.environ.get("SIM_TIME", "60"))
    RESULTS.mkdir(parents=True, exist_ok=True)
    CONSOLE.write_text(f"# E5 param sweep started {time.strftime('%F %T')}\n")

    # Baseline knobs
    base_iv = 1.0
    base_dit = 1000
    base_ptw_ms = 1280
    base_edrx = 1024

    jobs: list[list[str]] = []

    # A — PTW length (RAI on, eDRX 10.24 s)
    for ptw_ms in (256, 1280, 2560):
        camp = f"ul_en_e5_ptw_p{ptw_ms}_edrx{base_edrx}_dit{base_dit}_i1"
        jobs.append(
            base_cmd(
                camp,
                seeds=seeds,
                workers=workers,
                ues=ues,
                interval=base_iv,
                sim_time=sim_time,
                dit=base_dit,
                ptw=True,
                ptw_ms=ptw_ms,
                rai=True,
                edrx_fr=base_edrx,
            )
        )

    # B — eDRX cycle (PTW 1.28 s, RAI on)
    for edrx in (256, 512, 1024, 2048):
        camp = f"ul_en_e5_edrx_e{edrx}_ptw{base_ptw_ms}_dit{base_dit}_i1"
        jobs.append(
            base_cmd(
                camp,
                seeds=seeds,
                workers=workers,
                ues=ues,
                interval=base_iv,
                sim_time=sim_time,
                dit=base_dit,
                ptw=True,
                ptw_ms=base_ptw_ms,
                rai=True,
                edrx_fr=edrx,
            )
        )

    # C — DIT (RAI off so DIT is the release trigger; paper-like 15–20 s)
    for dit in (1000, 5000, 15000, 20000):
        camp = f"ul_en_e5_dit_d{dit}_ptw{base_ptw_ms}_edrx{base_edrx}_i1"
        jobs.append(
            base_cmd(
                camp,
                seeds=seeds,
                workers=workers,
                ues=ues,
                interval=base_iv,
                sim_time=sim_time,
                dit=dit,
                ptw=True,
                ptw_ms=base_ptw_ms,
                rai=False,
                edrx_fr=base_edrx,
            )
        )

    # D — RAI on/off × short/mid DIT
    for dit in (1000, 5000):
        for rai in (False, True):
            rtag = "rai1" if rai else "rai0"
            camp = f"ul_en_e5_rai_{rtag}_dit{dit}_ptw{base_ptw_ms}_edrx{base_edrx}_i1"
            jobs.append(
                base_cmd(
                    camp,
                    seeds=seeds,
                    workers=workers,
                    ues=ues,
                    interval=base_iv,
                    sim_time=sim_time,
                    dit=dit,
                    ptw=True,
                    ptw_ms=base_ptw_ms,
                    rai=rai,
                    edrx_fr=base_edrx,
                )
            )

    # E — packet interval / IAT (sleep vs airtime)
    for iv in (0.2, 1.0, 5.0, 10.0):
        iv_tag = str(iv).replace(".", "p")
        # Longer wall for sparse IAT so several packets occur
        st = max(sim_time, 60.0 if iv <= 1.0 else 120.0)
        camp = f"ul_en_e5_iat_i{iv_tag}_ptw{base_ptw_ms}_edrx{base_edrx}_dit{base_dit}"
        jobs.append(
            base_cmd(
                camp,
                seeds=seeds,
                workers=workers,
                ues=ues,
                interval=iv,
                sim_time=st,
                dit=base_dit,
                ptw=True,
                ptw_ms=base_ptw_ms,
                rai=True,
                edrx_fr=base_edrx,
            )
        )

    print(f"Queued {len(jobs)} campaign groups (seeds={seeds}, ues={ues})", flush=True)
    rc_all = 0
    for cmd in jobs:
        rc = run(cmd)
        if rc != 0:
            rc_all = rc
            print(f"FAIL rc={rc}", flush=True)

    # Aggregate
    agg = ROOT / "scripts" / "summarize_energy_param_sweep.py"
    if agg.exists():
        subprocess.call([sys.executable, str(agg)], cwd=ROOT)

    print(f"E5 param sweep finished rc={rc_all}", flush=True)
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
