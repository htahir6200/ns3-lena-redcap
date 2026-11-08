#!/usr/bin/env python3
"""Overnight dense-capacity campaign in the stable operating region."""

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


def combine_and_parse() -> None:
    rows: list[dict] = []
    for pattern in ("ul_dense_cap_v1_*_manifest.csv", "ul_load_cap_v1_*_manifest.csv"):
        for m in sorted(RESULTS.glob(pattern)):
            with m.open() as f:
                rows.extend(csv.DictReader(f))
    if not rows:
        print("No manifests to combine", flush=True)
        return
    out = RESULTS / "ul_dense_overnight_combined_manifest.csv"
    with out.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"Combined {len(rows)} jobs -> {out}", flush=True)
    subprocess.call(
        [
            sys.executable,
            str(PARSE),
            "--manifest",
            str(out),
            "--out",
            str(RESULTS / "ul_dense_overnight_summary.csv"),
        ],
        cwd=ROOT,
    )


def main() -> int:
    seeds = int(os.environ.get("SEEDS", "3"))
    workers = int(os.environ.get("WORKERS", "2"))
    RESULTS.mkdir(parents=True, exist_ok=True)
    log = RESULTS / f"overnight_{time.strftime('%Y%m%d_%H%M%S')}.log"
    # Tee-like: also write a simple status file
    status = RESULTS / "overnight_status.txt"
    status.write_text(f"started {time.strftime('%F %T')} log={log}\n")

    print(f"Overnight start seeds={seeds} workers={workers}", flush=True)

    # Phase A: densification @ 0.5 s
    for ues in (20, 40, 60, 80, 100):
        run_campaign(f"ul_dense_cap_v1_u{ues}", [1, 2, 3], ues, 0.5, seeds, workers)
        status.write_text(f"phaseA done ues={ues} at {time.strftime('%F %T')}\n")

    # Phase B: offered load @ 40 UEs for min/max
    for iv in (1.0, 0.5, 0.2, 0.1):
        tag = str(iv).replace(".", "p")
        run_campaign(f"ul_load_cap_v1_i{tag}", [1, 2], 40, iv, seeds, workers)
        status.write_text(f"phaseB min/max iv={iv} at {time.strftime('%F %T')}\n")

    # Phase B2: eRedCap safer intervals only
    for iv in (1.0, 0.5, 0.3):
        tag = str(iv).replace(".", "p")
        run_campaign(f"ul_load_cap_v1_eredcap_i{tag}", [3], 40, iv, seeds, workers)
        status.write_text(f"phaseB eredcap iv={iv} at {time.strftime('%F %T')}\n")

    combine_and_parse()
    status.write_text(f"finished {time.strftime('%F %T')}\n")
    print("Overnight finished", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
