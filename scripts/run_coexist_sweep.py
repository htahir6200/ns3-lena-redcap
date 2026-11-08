#!/usr/bin/env python3
"""M2: RedCap + eMBB coexistence sweep (IoTJ plan)."""

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


def run_campaign(
    campaign: str,
    configs: list[int],
    ues: int,
    embb_ues: int,
    interval: float,
    seeds: int,
    workers: int,
) -> int:
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
        "--embb-ues",
        str(embb_ues),
        "--embb-packet-size",
        "1500",
        "--embb-interval",
        "0.1",
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
        "--skip-existing",
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
    seeds = int(os.environ.get("SEEDS", "5"))
    workers = int(os.environ.get("WORKERS", "2"))
    RESULTS.mkdir(parents=True, exist_ok=True)
    status = RESULTS / "coexist_status.txt"
    status.write_text(f"started {time.strftime('%F %T')}\n")

    # Hold RedCap load moderate; sweep eMBB background
    redcap_ues = 40
    interval = 0.5
    for embb in (0, 2, 5, 10):
        rc = run_campaign(
            f"ul_coexist_e{embb}",
            [1, 2, 3],
            redcap_ues,
            embb,
            interval,
            seeds,
            workers,
        )
        status.write_text(f"embb={embb} rc={rc} {time.strftime('%F %T')}\n")

    combine_and_parse(["ul_coexist_e*_manifest.csv"], "ul_coexist_sweep")
    status.write_text(f"finished {time.strftime('%F %T')}\n")
    print("Coexistence sweep finished", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
