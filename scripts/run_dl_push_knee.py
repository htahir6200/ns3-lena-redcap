#!/usr/bin/env python3
"""DL twin of the UL offered-load knee campaign."""

from __future__ import annotations

import csv
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
RUNNER = ROOT / "scripts" / "runner_redcap.py"
PARSE = ROOT / "scripts" / "parse_results.py"


def run_campaign(campaign: str, configs: list[int], ues: int, interval: float, seeds: int, workers: int) -> int:
    cmd = [
        sys.executable,
        str(RUNNER),
        "--campaign",
        campaign,
        "--configs",
        *[str(c) for c in configs],
        "--seeds",
        str(seeds),
        "--ues",
        str(ues),
        "--packet-size",
        "5000",
        "--interval",
        str(interval),
        "--sim-time",
        "36",
        "--inactivity-timer",
        "60000",
        "--downlink",
        "--workers",
        str(workers),
    ]
    print(f"\n=== {time.strftime('%F %T')} RUN {' '.join(cmd[2:])}", flush=True)
    return subprocess.call(cmd, cwd=ROOT)


def combine_and_parse() -> None:
    rows: list[dict] = []
    for m in sorted(RESULTS.glob("dl_push_load_*_manifest.csv")):
        with m.open() as f:
            rows.extend(csv.DictReader(f))
    if not rows:
        print("No DL manifests", flush=True)
        return
    out = RESULTS / "dl_push_knee_manifest.csv"
    with out.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    subprocess.call(
        [
            sys.executable,
            str(PARSE),
            "--manifest",
            str(out),
            "--out",
            str(RESULTS / "dl_push_knee_summary.csv"),
        ],
        cwd=ROOT,
    )


def main() -> int:
    seeds = int(os.environ.get("SEEDS", "3"))
    workers = int(os.environ.get("WORKERS", "2"))
    RESULTS.mkdir(parents=True, exist_ok=True)
    status = RESULTS / "dl_push_status.txt"
    status.write_text(f"started {time.strftime('%F %T')}\n")

    # Mirror UL offered-load knee @ 40 UEs (where UL showed clear eRedCap collapse)
    for iv in (0.2, 0.15, 0.1, 0.05):
        tag = str(iv).replace(".", "p")
        run_campaign(f"dl_push_load_i{tag}", [1, 2, 3], 40, iv, seeds, workers)
        status.write_text(f"dl load iv={iv} done {time.strftime('%F %T')}\n")

    # One densify check at the UL-hot spot
    run_campaign("dl_push_dense02_u80", [1, 2, 3], 80, 0.2, seeds, workers)
    status.write_text(f"dl dense02 u80 done {time.strftime('%F %T')}\n")

    combine_and_parse()
    status.write_text(f"finished {time.strftime('%F %T')}\n")
    print("DL push campaign finished", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
