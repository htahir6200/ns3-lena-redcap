#!/usr/bin/env python3
"""Offline battery lifetime helper from mean energy / sim duration.

Usage:
  python3 scripts/battery_lifetime.py --mean-energy-j 8.5 --sim-time-s 36 --battery-wh 5
  python3 scripts/battery_lifetime.py --summary results/campaigns/ul_en_e4_*_summary.csv --battery-wh 5
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path


def years_from_mean_power(mean_power_w: float, battery_wh: float) -> float:
    if mean_power_w <= 0 or not math.isfinite(mean_power_w):
        return float("nan")
    return battery_wh / mean_power_w / 24.0 / 365.25


def main() -> int:
    ap = argparse.ArgumentParser(description="Wh → years from mean campaign energy")
    ap.add_argument("--mean-energy-j", type=float, default=None)
    ap.add_argument("--sim-time-s", type=float, default=36.0, help="Traffic window (not wall clock)")
    ap.add_argument("--battery-wh", type=float, default=5.0)
    ap.add_argument(
        "--summary",
        type=Path,
        nargs="*",
        default=None,
        help="One or more summary CSVs with mean_energy_j + sim_time columns",
    )
    args = ap.parse_args()

    if args.mean_energy_j is not None:
        p = args.mean_energy_j / args.sim_time_s
        y = years_from_mean_power(p, args.battery_wh)
        print(
            f"mean_energy={args.mean_energy_j:.4f} J over {args.sim_time_s:.1f} s "
            f"=> mean_power={p*1000:.3f} mW => lifetime@{args.battery_wh:g} Wh ≈ {y:.4f} years"
        )
        return 0

    if not args.summary:
        ap.error("provide --mean-energy-j or --summary")

    print(
        f"{'file':40} {'tag/seed':40} {'E_J':>8} {'P_mW':>8} {'years':>8} (battery={args.battery_wh:g} Wh)"
    )
    for path in args.summary:
        with path.open() as fh:
            for row in csv.DictReader(fh):
                try:
                    e = float(row.get("mean_energy_j", "nan"))
                    st = float(row.get("sim_time", row.get("sim_time_s", args.sim_time_s)))
                except ValueError:
                    continue
                p = e / st if st > 0 else float("nan")
                y = years_from_mean_power(p, args.battery_wh)
                tag = row.get("sim_tag", path.name)[:40]
                print(
                    f"{path.name[:40]:40} {tag:40} {e:8.3f} {p*1000:8.3f} {y:8.2f}"
                )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
