#!/usr/bin/env python3
"""Elsevier IoT M3: true 5 MHz eRedCap BWP vs IoTJ capability-style at knees."""

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
    true_5mhz: bool | None,
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
    ]
    if true_5mhz is True:
        cmd.extend(["--true-5mhz", "true"])
    elif true_5mhz is False:
        cmd.extend(["--true-5mhz", "false"])
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
    RESULTS.mkdir(parents=True, exist_ok=True)
    status = RESULTS / "true5mhz_m3_status.txt"
    status.write_text(
        f"started {time.strftime('%F %T')} seeds={seeds} workers={workers}\n"
    )

    rc_all = 0
    # Phase A: eRedCap only — capability vs true 5 MHz at load knees
    for true5 in (False, True):
        mode = "t5" if true5 else "cap"
        for iv in (0.2, 0.1, 0.05):
            tag = str(iv).replace(".", "p")
            campaign = f"ul_t5_m3_load_i{tag}_{mode}"
            rc = run_campaign(campaign, [3], 40, iv, seeds, workers, true5)
            append_status(status, f"{campaign} rc={rc} {time.strftime('%F %T')}")
            if rc != 0:
                rc_all = rc

        for ues in (80, 160):
            campaign = f"ul_t5_m3_dense02_u{ues}_{mode}"
            rc = run_campaign(campaign, [3], ues, 0.2, seeds, workers, true5)
            append_status(status, f"{campaign} rc={rc} {time.strftime('%F %T')}")
            if rc != 0:
                rc_all = rc

    # Phase B: ranking check — configs 1/2/3 with true5 default on for config 3
    for iv in (0.1, 0.05):
        tag = str(iv).replace(".", "p")
        campaign = f"ul_t5_m3_rank_i{tag}"
        rc = run_campaign(campaign, [1, 2, 3], 40, iv, seeds, workers, True)
        append_status(status, f"{campaign} rc={rc} {time.strftime('%F %T')}")
        if rc != 0:
            rc_all = rc

    combine_and_parse(["ul_t5_m3_*_manifest.csv"], "ul_t5_m3_sweep")
    append_status(status, f"finished rc={rc_all} {time.strftime('%F %T')}")
    print("True-5MHz M3 sweep finished", flush=True)
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
