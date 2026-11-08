#!/usr/bin/env python3
"""Push past the prior stability wall to map the capacity knee."""

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
        "--workers",
        str(workers),
    ]
    print(f"\n=== {time.strftime('%F %T')} RUN {' '.join(cmd[2:])}", flush=True)
    return subprocess.call(cmd, cwd=ROOT)


def combine_and_parse(patterns: list[str], out_name: str) -> None:
    rows: list[dict] = []
    for pattern in patterns:
        for m in sorted(RESULTS.glob(pattern)):
            with m.open() as f:
                rows.extend(csv.DictReader(f))
    if not rows:
        print("No manifests", flush=True)
        return
    out = RESULTS / f"{out_name}_manifest.csv"
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
            str(RESULTS / f"{out_name}_summary.csv"),
        ],
        cwd=ROOT,
    )


def main() -> int:
    seeds = int(os.environ.get("SEEDS", "3"))
    workers = int(os.environ.get("WORKERS", "2"))
    RESULTS.mkdir(parents=True, exist_ok=True)
    status = RESULTS / "push_status.txt"
    status.write_text(f"started {time.strftime('%F %T')}\n")

    # Phase 1: offered-load knee map @ 40 UEs (where eRedCap already showed ~82% PDR at 0.1s)
    for iv in (0.2, 0.15, 0.1, 0.05):
        tag = str(iv).replace(".", "p")
        run_campaign(f"ul_push_load_i{tag}", [1, 2, 3], 40, iv, seeds, workers)
        status.write_text(f"load iv={iv} done {time.strftime('%F %T')}\n")

    # Phase 2: densify @ 0.5 s beyond previous wall
    for ues in (100, 120, 160):
        run_campaign(f"ul_push_dense_u{ues}", [1, 2, 3], ues, 0.5, seeds, workers)
        status.write_text(f"dense ues={ues} done {time.strftime('%F %T')}\n")

    # Phase 3: densify at harder interval 0.2 s
    for ues in (40, 60, 80):
        run_campaign(f"ul_push_dense02_u{ues}", [1, 2, 3], ues, 0.2, seeds, workers)
        status.write_text(f"dense02 ues={ues} done {time.strftime('%F %T')}\n")

    combine_and_parse(
        ["ul_push_load_*_manifest.csv", "ul_push_dense*_manifest.csv"],
        "ul_push_knee",
    )
    status.write_text(f"finished {time.strftime('%F %T')}\n")
    print("Push campaign finished", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
