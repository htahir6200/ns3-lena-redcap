#!/usr/bin/env python3
"""Aggregate ul_m5_* → mean±std for Elsevier IoT M5 multi-cell campaign."""

from __future__ import annotations

import csv
import math
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
OUT_CSV = RESULTS / "ul_m5_agg.csv"
OUT_MD = ROOT / "docs" / "MULTICELL_CAMPAIGN_M5.md"


def mean_std(xs: list[float]) -> tuple[float, float]:
    if not xs:
        return float("nan"), float("nan")
    m = sum(xs) / len(xs)
    if len(xs) < 2:
        return m, 0.0
    v = sum((x - m) ** 2 for x in xs) / (len(xs) - 1)
    return m, math.sqrt(v)


def f(x: str | None) -> float:
    try:
        return float(x) if x not in (None, "") else float("nan")
    except ValueError:
        return float("nan")


def fmt(xs: list[float], nd: int = 1) -> str:
    m, s = mean_std(xs)
    if m != m:
        return "—"
    if s != s or s < 1e-9:
        return f"{m:.{nd}f}"
    return f"{m:.{nd}f}±{s:.{nd}f}"


def gnb_of(row: dict) -> int:
    v = row.get("gnb_num", "")
    if v not in (None, ""):
        try:
            return int(float(v))
        except ValueError:
            pass
    tag = row.get("sim_tag", "")
    if "_g2_" in tag or tag.endswith("_g2") or "_g2_s" in tag:
        return 2
    return 1


def main() -> int:
    paths = [p for p in sorted(RESULTS.glob("ul_m5_*_summary.csv")) if "sweep" not in p.name]
    if not paths and (RESULTS / "ul_m5_sweep_summary.csv").exists():
        paths = [RESULTS / "ul_m5_sweep_summary.csv"]
    if not paths:
        print("No ul_m5_*_summary.csv yet")
        return 1

    groups: dict[tuple, list[dict]] = defaultdict(list)
    for path in paths:
        with path.open() as fh:
            for row in csv.DictReader(fh):
                key = (
                    row.get("config_name", "?"),
                    gnb_of(row),
                    int(float(row.get("num_ues", 0))),
                    float(row.get("interval_s", 0)),
                )
                groups[key].append(row)

    agg_rows = []
    lines = [
        "# Multi-cell campaign M5 (Elsevier IoT Option C)",
        "",
        "**Status: complete** when regenerated after `run_multicell_m5.py`.",
        "",
        "True 5 MHz eRedCap; static layout; ISD default 500 m; co-channel spectrum.",
        "",
        "## Per-cell load (1-cell vs 2-cell, same UEs/gNB)",
        "",
        "| gNB | UEs/gNB | Interval | PDR % | Delay ms | attach_fail |",
        "|-----|---------|----------|-------|----------|-------------|",
    ]

    for key in sorted(groups.keys(), key=lambda k: (k[0], k[1], k[2], k[3])):
        cfg, gnb, ues, iv = key
        rows = groups[key]
        pdr = [f(r.get("payload_pdr")) for r in rows]
        delay = [f(r.get("payload_mean_delay_ms")) for r in rows]
        att = [f(r.get("attach_fail")) for r in rows]
        agg_rows.append(
            {
                "config": cfg,
                "gnb_num": gnb,
                "ues_per_gnb": ues,
                "interval_s": iv,
                "n": len(rows),
                "pdr_mean": mean_std(pdr)[0],
                "pdr_std": mean_std(pdr)[1],
                "delay_mean": mean_std(delay)[0],
                "delay_std": mean_std(delay)[1],
                "attach_fail_mean": mean_std(att)[0],
            }
        )
        if cfg == "eredcap":
            lines.append(
                f"| {gnb} | {ues} | {iv:.2f} s | {fmt(pdr)} | {fmt(delay)} | {fmt(att, 0)} |"
            )

    lines += [
        "",
        "## Ranking @ 2-cell, 20 UE/gNB",
        "",
        "| Config | Interval | PDR % | Delay ms |",
        "|--------|----------|-------|----------|",
    ]
    for key in sorted(groups.keys(), key=lambda k: (k[3], k[0])):
        cfg, gnb, ues, iv = key
        if gnb != 2 or ues != 20:
            continue
        rows = groups[key]
        pdr = [f(r.get("payload_pdr")) for r in rows]
        delay = [f(r.get("payload_mean_delay_ms")) for r in rows]
        lines.append(f"| {cfg} | {iv:.2f} s | {fmt(pdr)} | {fmt(delay)} |")

    # Same total load contrasts: 1×40 vs 2×20
    lines += [
        "",
        "## Same total load (40 UEs): 1×40 vs 2×20",
        "",
        "| Layout | Interval | PDR % | Delay ms |",
        "|--------|----------|-------|----------|",
    ]
    for iv in (0.2, 0.1):
        for gnb, ues, label in ((1, 40, "1×40"), (2, 20, "2×20")):
            key = ("eredcap", gnb, ues, iv)
            rows = groups.get(key, [])
            if not rows:
                continue
            pdr = [f(r.get("payload_pdr")) for r in rows]
            delay = [f(r.get("payload_mean_delay_ms")) for r in rows]
            lines.append(f"| {label} | {iv:.2f} s | {fmt(pdr)} | {fmt(delay)} |")

    lines += [
        "",
        "## Notes",
        "",
        "- `--gNbNum` / `--gNbIsd`; one ResManager per gNB; `AttachToClosestEnb`.",
        "- Shared spectrum (co-channel interference intentional).",
        "- `enddevicesNumPergNb` is per cell; total RedCap UEs = UEs/gNB × gNbNum.",
        "",
        f"Agg CSV: `{OUT_CSV.relative_to(ROOT)}`",
        "",
    ]

    OUT_MD.write_text("\n".join(lines) + "\n")
    with OUT_CSV.open("w", newline="") as fh:
        if agg_rows:
            w = csv.DictWriter(fh, fieldnames=list(agg_rows[0].keys()))
            w.writeheader()
            w.writerows(agg_rows)
    print(f"Wrote {OUT_MD}")
    print(f"Wrote {OUT_CSV}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
