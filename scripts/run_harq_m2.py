#!/usr/bin/env python3
"""Elsevier IoT M2: knee subset with DataErrorModel on, HARQ retx off vs on."""

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
    enable_harq: bool,
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
        "--error-model",
    ]
    if enable_harq:
        cmd.append("--enable-harq")
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
    status = RESULTS / "harq_m2_status.txt"
    status.write_text(
        f"started {time.strftime('%F %T')} seeds={seeds} workers={workers}\n"
    )

    # Always error-model on. Compare HARQ off vs on at IoTJ knees.
    # Phase A: offered load @ 40 UEs
    # Phase B: densify @ 0.2 s
    rc_all = 0
    for harq in (False, True):
        for iv in (0.2, 0.1, 0.05):
            tag = str(iv).replace(".", "p")
            campaign = f"ul_harq_m2_load_i{tag}_h{int(harq)}"
            rc = run_campaign(campaign, [1, 2, 3], 40, iv, seeds, workers, harq)
            append_status(status, f"{campaign} rc={rc} {time.strftime('%F %T')}")
            if rc != 0:
                rc_all = rc

        for ues in (80, 160):
            campaign = f"ul_harq_m2_dense02_u{ues}_h{int(harq)}"
            rc = run_campaign(campaign, [1, 2, 3], ues, 0.2, seeds, workers, harq)
            append_status(status, f"{campaign} rc={rc} {time.strftime('%F %T')}")
            if rc != 0:
                rc_all = rc

    combine_and_parse(
        ["ul_harq_m2_*_manifest.csv"],
        "ul_harq_m2_sweep",
    )
    append_status(status, f"finished rc={rc_all} {time.strftime('%F %T')}")
    print("HARQ M2 sweep finished", flush=True)
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
