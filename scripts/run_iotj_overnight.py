#!/usr/bin/env python3
"""Overnight IoTJ pipeline: M1 access knees → M2 coexistence sweep."""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"


def main() -> int:
    seeds = os.environ.get("SEEDS", "5")
    workers = os.environ.get("WORKERS", "2")
    env = dict(os.environ)
    env["SEEDS"] = seeds
    env["WORKERS"] = workers
    RESULTS.mkdir(parents=True, exist_ok=True)
    log = RESULTS / "iotj_overnight_status.txt"
    log.write_text(f"started {time.strftime('%F %T')} SEEDS={seeds} WORKERS={workers}\n")

    steps = [
        ("M1 access", [sys.executable, str(ROOT / "scripts" / "run_access_knee.py")]),
        ("M2 coexist", [sys.executable, str(ROOT / "scripts" / "run_coexist_sweep.py")]),
    ]
    rc_all = 0
    for name, cmd in steps:
        log.write_text(log.read_text() + f"begin {name} {time.strftime('%F %T')}\n")
        print(f"==== {name} ====", flush=True)
        rc = subprocess.call(cmd, cwd=ROOT, env=env)
        log.write_text(log.read_text() + f"end {name} rc={rc} {time.strftime('%F %T')}\n")
        if rc != 0:
            rc_all = rc
            print(f"{name} failed rc={rc}", flush=True)
            # continue to next milestone so overnight still produces something
    log.write_text(log.read_text() + f"finished {time.strftime('%F %T')} rc={rc_all}\n")
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
