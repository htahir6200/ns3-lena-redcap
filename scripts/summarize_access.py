#!/usr/bin/env python3
"""Aggregate ul_access_knee_summary.csv → mean±std tables for IoTJ M1."""

from __future__ import annotations

import csv
import math
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IN = ROOT / "results" / "campaigns" / "ul_access_knee_summary.csv"
OUT_MD = ROOT / "docs" / "ACCESS_CAMPAIGN_M1.md"
OUT_CSV = ROOT / "results" / "campaigns" / "ul_access_knee_agg.csv"


def mean_std(xs: list[float]) -> tuple[float, float]:
    if not xs:
        return float("nan"), float("nan")
    m = sum(xs) / len(xs)
    if len(xs) < 2:
        return m, 0.0
    v = sum((x - m) ** 2 for x in xs) / (len(xs) - 1)
    return m, math.sqrt(v)


def main() -> int:
    if not IN.exists():
        print(f"Missing {IN}")
        return 1

    groups: dict[tuple, list[dict]] = defaultdict(list)
    with IN.open() as f:
        for row in csv.DictReader(f):
            key = (
                row.get("config_name", "?"),
                int(float(row.get("num_ues", 0))),
                float(row.get("interval_s", 0)),
            )
            groups[key].append(row)

    metrics = [
        "payload_pdr",
        "payload_mean_delay_ms",
        "preamble_tx",
        "ra_success",
        "ra_failed",
        "rrc_connected",
        "rrc_timeout",
        "prach_success_rate",
        "attach_fail",
        "ul_rb_util",
        "pdcch_used",
    ]

    agg_rows = []
    lines = [
        "# Access campaign M1 (IoTJ)",
        "",
        f"Source: `{IN.relative_to(ROOT)}`. Seeds aggregated as mean ± std.",
        "",
        "## Offered load @ 40 UEs",
        "",
        "| Config | Interval | PDR % | Delay ms | preamble | ra_ok | rrc_conn | attach_fail | prach_succ |",
        "|--------|----------|-------|----------|----------|-------|----------|-------------|------------|",
    ]

    def fmt(m: float, s: float, nd: int = 1) -> str:
        if m != m:
            return "—"
        if s < 1e-12:
            return f"{m:.{nd}f}"
        return f"{m:.{nd}f}±{s:.{nd}f}"

    load_keys = sorted(
        [k for k in groups if k[1] == 40 and k[2] in (0.05, 0.1, 0.2)],
        key=lambda k: (k[2], k[0]),
    )
    for key in load_keys:
        rows = groups[key]
        stats = {}
        for met in metrics:
            vals = []
            for r in rows:
                try:
                    vals.append(float(r.get(met, "nan")))
                except ValueError:
                    pass
            vals = [v for v in vals if v == v]
            stats[met] = mean_std(vals)
        cfg, ues, iv = key
        agg = {
            "config_name": cfg,
            "num_ues": ues,
            "interval_s": iv,
            "n_seeds": len(rows),
        }
        for met in metrics:
            m, s = stats[met]
            agg[f"{met}_mean"] = m
            agg[f"{met}_std"] = s
        agg_rows.append(agg)
        lines.append(
            f"| {cfg} | {iv:.2f} s | {fmt(stats['payload_pdr'][0], stats['payload_pdr'][1])} | "
            f"{fmt(stats['payload_mean_delay_ms'][0], stats['payload_mean_delay_ms'][1])} | "
            f"{fmt(stats['preamble_tx'][0], stats['preamble_tx'][1], 0)} | "
            f"{fmt(stats['ra_success'][0], stats['ra_success'][1], 0)} | "
            f"{fmt(stats['rrc_connected'][0], stats['rrc_connected'][1], 0)} | "
            f"{fmt(stats['attach_fail'][0], stats['attach_fail'][1], 0)} | "
            f"{fmt(stats['prach_success_rate'][0], stats['prach_success_rate'][1], 2)} |"
        )

    lines += [
        "",
        "## Densify @ 0.2 s",
        "",
        "| Config | UEs | PDR % | Delay ms | preamble | ra_ok | rrc_conn | attach_fail | prach_succ |",
        "|--------|-----|-------|----------|----------|-------|----------|-------------|------------|",
    ]
    dense02 = sorted(
        [k for k in groups if abs(k[2] - 0.2) < 1e-9 and k[1] in (40, 80, 160)],
        key=lambda k: (k[1], k[0]),
    )
    for key in dense02:
        rows = groups[key]
        stats = {
            met: mean_std(
                [
                    float(r[met])
                    for r in rows
                    if r.get(met) not in (None, "") and float(r[met]) == float(r[met])
                ]
            )
            for met in metrics
            if any(r.get(met) not in (None, "") for r in rows)
        }
        for met in metrics:
            if met not in stats:
                stats[met] = (float("nan"), float("nan"))
        cfg, ues, iv = key
        # avoid double-counting 40@0.2 already in load table detail — still list densify
        agg = {
            "config_name": cfg,
            "num_ues": ues,
            "interval_s": iv,
            "n_seeds": len(rows),
        }
        for met in metrics:
            m, s = stats[met]
            agg[f"{met}_mean"] = m
            agg[f"{met}_std"] = s
        if not any(
            a["config_name"] == cfg and a["num_ues"] == ues and abs(a["interval_s"] - iv) < 1e-9
            for a in agg_rows
        ):
            agg_rows.append(agg)
        else:
            # refresh existing
            for i, a in enumerate(agg_rows):
                if a["config_name"] == cfg and a["num_ues"] == ues and abs(a["interval_s"] - iv) < 1e-9:
                    agg_rows[i] = agg
        lines.append(
            f"| {cfg} | {ues} | {fmt(stats['payload_pdr'][0], stats['payload_pdr'][1])} | "
            f"{fmt(stats['payload_mean_delay_ms'][0], stats['payload_mean_delay_ms'][1])} | "
            f"{fmt(stats['preamble_tx'][0], stats['preamble_tx'][1], 0)} | "
            f"{fmt(stats['ra_success'][0], stats['ra_success'][1], 0)} | "
            f"{fmt(stats['rrc_connected'][0], stats['rrc_connected'][1], 0)} | "
            f"{fmt(stats['attach_fail'][0], stats['attach_fail'][1], 0)} | "
            f"{fmt(stats['prach_success_rate'][0], stats['prach_success_rate'][1], 2)} |"
        )

    lines += [
        "",
        "## Densify @ 0.5 s",
        "",
        "| Config | UEs | PDR % | Delay ms | preamble | ra_ok | rrc_conn | attach_fail | prach_succ |",
        "|--------|-----|-------|----------|----------|-------|----------|-------------|------------|",
    ]
    dense05 = sorted(
        [k for k in groups if abs(k[2] - 0.5) < 1e-9],
        key=lambda k: (k[1], k[0]),
    )
    for key in dense05:
        rows = groups[key]
        stats = {}
        for met in metrics:
            vals = []
            for r in rows:
                try:
                    v = float(r.get(met, "nan"))
                    if v == v:
                        vals.append(v)
                except ValueError:
                    pass
            stats[met] = mean_std(vals)
        cfg, ues, iv = key
        agg = {
            "config_name": cfg,
            "num_ues": ues,
            "interval_s": iv,
            "n_seeds": len(rows),
        }
        for met in metrics:
            m, s = stats[met]
            agg[f"{met}_mean"] = m
            agg[f"{met}_std"] = s
        agg_rows.append(agg)
        lines.append(
            f"| {cfg} | {ues} | {fmt(stats['payload_pdr'][0], stats['payload_pdr'][1])} | "
            f"{fmt(stats['payload_mean_delay_ms'][0], stats['payload_mean_delay_ms'][1])} | "
            f"{fmt(stats['preamble_tx'][0], stats['preamble_tx'][1], 0)} | "
            f"{fmt(stats['ra_success'][0], stats['ra_success'][1], 0)} | "
            f"{fmt(stats['rrc_connected'][0], stats['rrc_connected'][1], 0)} | "
            f"{fmt(stats['attach_fail'][0], stats['attach_fail'][1], 0)} | "
            f"{fmt(stats['prach_success_rate'][0], stats['prach_success_rate'][1], 2)} |"
        )

    lines += [
        "",
        "## Reading note",
        "",
        "- If `rrc_connected ≈ num_ues` and `attach_fail ≈ 0` while PDR collapses, the knee is **scheduler/data-plane**, not attach failure.",
        "- If `attach_fail` or `rrc_timeout` rises with densify, access contention is part of the IoTJ story.",
        "",
    ]

    OUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")
    if agg_rows:
        with OUT_CSV.open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(agg_rows[0].keys()))
            w.writeheader()
            w.writerows(agg_rows)
    print(f"Wrote {OUT_MD}")
    print(f"Wrote {OUT_CSV}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
