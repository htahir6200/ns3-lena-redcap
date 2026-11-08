#!/usr/bin/env python3
"""M2b: stressed RedCap + eMBB coexistence near M1 capacity knees."""

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
    fieldnames: list[str] = []
    seen = set()
    for r in rows:
        for k in r:
            if k not in seen:
                seen.add(k)
                fieldnames.append(k)
    with out.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
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
    status = RESULTS / "coexist_stress_status.txt"
    status.write_text(f"started {time.strftime('%F %T')}\n")

    # Near M1 eRedCap soft knees
    grids = [
        # (tag_prefix, ues, interval)
        ("ul_coexist_stress_i0p1", 40, 0.1),
        ("ul_coexist_stress_u80", 80, 0.2),
    ]
    rc_all = 0
    for prefix, ues, interval in grids:
        for embb in (0, 5, 10):
            campaign = f"{prefix}_e{embb}"
            rc = run_campaign(
                campaign, [1, 2, 3], ues, embb, interval, seeds, workers
            )
            status.write_text(
                status.read_text()
                + f"{campaign} rc={rc} {time.strftime('%F %T')}\n"
            )
            if rc != 0:
                rc_all = rc

    combine_and_parse(["ul_coexist_stress_*_manifest.csv"], "ul_coexist_stress")
    status.write_text(
        status.read_text() + f"finished rc={rc_all} {time.strftime('%F %T')}\n"
    )
    print("Stressed coexistence sweep finished", flush=True)
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
