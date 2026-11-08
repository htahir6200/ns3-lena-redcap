#!/usr/bin/env python3
"""Phase C / E4 campaign: PTW × RAI on short-DIT mid-tier UL grid.

Grid (plan):
  - 40 UE, packet 5000 B, sim 36 s, DIT 1000 ms
  - intervals {0.05, 0.1}
  - true 5 MHz vs capability-style eRedCap
  - PTW on/off × RAI on/off
  - 5 seeds
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
CONSOLE = RESULTS / "energy_e4_console.log"


def run(cmd: list[str]) -> int:
    print(f"\n=== {time.strftime('%F %T')} {' '.join(cmd[2:])}", flush=True)
    with CONSOLE.open("a") as log:
        log.write(f"\n# {' '.join(cmd)}\n")
        log.flush()
        return subprocess.call(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)


def main() -> int:
    workers = int(os.environ.get("WORKERS", "4"))
    seeds = int(os.environ.get("SEEDS", "5"))
    RESULTS.mkdir(parents=True, exist_ok=True)
    CONSOLE.write_text(f"# E4 started {time.strftime('%F %T')}\n")

    intervals = [0.05, 0.1]
    modes = [("true", True), ("false", False)]  # true5MHz flag
    # (ptw, rai)
    factors = [(False, False), (True, False), (False, True), (True, True)]

    rc_all = 0
    for iv in intervals:
        iv_tag = str(iv).replace(".", "p")
        for mode_name, t5 in modes:
            mode_tag = "t5" if t5 else "cap"
            for ptw, rai in factors:
                camp = f"ul_en_e4_load_i{iv_tag}_{mode_tag}_dit1000"
                cmd = [
                    sys.executable,
                    str(RUNNER),
                    "--campaign",
                    camp,
                    "--configs",
                    "3",
                    "--seeds",
                    str(seeds),
                    "--ues",
                    "40",
                    "--packet-size",
                    "5000",
                    "--interval",
                    str(iv),
                    "--sim-time",
                    "36",
                    "--inactivity-timer",
                    "1000",
                    "--workers",
                    str(workers),
                    "--true-5mhz",
                    "true" if t5 else "false",
                    "--skip-existing",
                ]
                if ptw:
                    cmd.append("--enable-edrx-ptw")
                if rai:
                    cmd.append("--enable-rai")
                # Distinct campaign names when factors differ (tags also encode flags)
                if ptw or rai:
                    camp_full = camp + ("_ptw" if ptw else "") + ("_rai" if rai else "")
                    cmd[cmd.index("--campaign") + 1] = camp_full
                rc = run(cmd)
                if rc != 0:
                    rc_all = rc
                    print(f"FAIL rc={rc} campaign={cmd[cmd.index('--campaign')+1]}", flush=True)

    print(f"E4 finished rc={rc_all}", flush=True)
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
