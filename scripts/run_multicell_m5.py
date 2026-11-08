#!/usr/bin/env python3
"""Elsevier IoT M5: static 2-cell densify/load vs 1-cell baseline (true 5 MHz eRedCap)."""

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
    interval: float,
    seeds: int,
    workers: int,
    gnb_num: int,
    gnb_isd: float,
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
        "--true-5mhz",
        "true",
        "--gnb-num",
        str(gnb_num),
        "--gnb-isd",
        str(gnb_isd),
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
    fieldnames: list[str] = []
    seen: set[str] = set()
    for r in rows:
        for k in r:
            if k not in seen:
                seen.add(k)
                fieldnames.append(k)
    out = RESULTS / f"{out_name}_manifest.csv"
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


def append_status(path: Path, line: str) -> None:
    prev = path.read_text() if path.exists() else ""
    path.write_text(prev + line + "\n")


def main() -> int:
    seeds = int(os.environ.get("SEEDS", "5"))
    workers = int(os.environ.get("WORKERS", "6"))
    isd = float(os.environ.get("GNB_ISD", "500"))
    RESULTS.mkdir(parents=True, exist_ok=True)
    status = RESULTS / "multicell_m5_status.txt"
    status.write_text(
        f"started {time.strftime('%F %T')} seeds={seeds} workers={workers} isd={isd}\n"
    )

    rc_all = 0
    # A) Same per-cell load: 1-cell vs 2-cell at UEs/gNB ∈ {20,40} × intervals
    for gnb in (1, 2):
        for ues in (20, 40):
            for iv in (0.2, 0.1, 0.05):
                tag = str(iv).replace(".", "p")
                campaign = f"ul_m5_percell_g{gnb}_u{ues}_i{tag}"
                rc = run_campaign(campaign, [3], ues, iv, seeds, workers, gnb, isd)
                append_status(status, f"{campaign} rc={rc} {time.strftime('%F %T')}")
                if rc != 0:
                    rc_all = rc

    # B) Same total load: 1×40 vs 2×20 @ 0.2 / 0.1 (total 40 UEs)
    for iv in (0.2, 0.1):
        tag = str(iv).replace(".", "p")
        for gnb, ues in ((1, 40), (2, 20)):
            campaign = f"ul_m5_tot40_g{gnb}_u{ues}_i{tag}"
            rc = run_campaign(campaign, [3], ues, iv, seeds, workers, gnb, isd)
            append_status(status, f"{campaign} rc={rc} {time.strftime('%F %T')}")
            if rc != 0:
                rc_all = rc

    # C) Ranking check @ 2-cell, 20 UE/gNB, soft knees
    for iv in (0.1, 0.05):
        tag = str(iv).replace(".", "p")
        campaign = f"ul_m5_rank_g2_u20_i{tag}"
        rc = run_campaign(campaign, [1, 2, 3], 20, iv, seeds, workers, 2, isd)
        append_status(status, f"{campaign} rc={rc} {time.strftime('%F %T')}")
        if rc != 0:
            rc_all = rc

    combine_and_parse(["ul_m5_*_manifest.csv"], "ul_m5_sweep")
    append_status(status, f"finished rc={rc_all} {time.strftime('%F %T')}")
    print("Multi-cell M5 sweep finished", flush=True)
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
