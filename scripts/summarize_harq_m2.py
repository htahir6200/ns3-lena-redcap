#!/usr/bin/env python3
"""Aggregate ul_harq_m2_* results → mean±std tables for Elsevier IoT M2."""

from __future__ import annotations

import csv
import math
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
OUT_CSV = RESULTS / "ul_harq_m2_agg.csv"
OUT_MD = ROOT / "docs" / "HARQ_CAMPAIGN_M2.md"


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


def harq_flag(row: dict) -> bool:
    h = row.get("enable_harq", "")
    if h != "":
        return str(h).lower() in ("1", "true", "yes")
    tag = row.get("sim_tag", "")
    return "_harq1_" in tag


def main() -> int:
    paths = sorted(RESULTS.glob("ul_harq_m2_*_summary.csv"))
    # Prefer per-campaign summaries; fall back to combined sweep summary.
    paths = [p for p in paths if p.name != "ul_harq_m2_sweep_summary.csv"] or [
        RESULTS / "ul_harq_m2_sweep_summary.csv"
    ]
    paths = [p for p in paths if p.exists()]
    if not paths:
        print("No ul_harq_m2_*_summary.csv yet")
        return 1

    groups: dict[tuple, list[dict]] = defaultdict(list)
    for path in paths:
        with path.open() as fh:
            for row in csv.DictReader(fh):
                key = (
                    row.get("config_name", "?"),
                    int(float(row.get("num_ues", 0))),
                    float(row.get("interval_s", 0)),
                    harq_flag(row),
                )
                groups[key].append(row)

    agg_rows = []
    lines = [
        "# HARQ campaign M2 (Elsevier IoT Option C)",
        "",
        "**Status:** complete when this file is regenerated after `run_harq_m2.py`.",
        "",
        "Error model **on** for all rows. Compare HARQ retx **off** vs **on** at IoTJ knees.",
        "",
        "Source: `results/campaigns/ul_harq_m2_*_summary.csv`. Seeds as mean ± std.",
        "",
        "## Offered load @ 40 UEs",
        "",
        "| Config | Interval | HARQ | PDR % | Delay ms | attach_fail |",
        "|--------|----------|------|-------|----------|-------------|",
    ]

    load_keys = sorted(
        [k for k in groups if k[1] == 40],
        key=lambda k: (k[2], k[3], k[0]),
    )
    for key in load_keys:
        cfg, ues, iv, harq = key
        rows = groups[key]
        pdr = [f(r.get("payload_pdr")) for r in rows]
        dly = [f(r.get("payload_mean_delay_ms")) for r in rows]
        af = [f(r.get("attach_fail")) for r in rows]
        pm, ps = mean_std(pdr)
        dm, ds = mean_std(dly)
        am, _ = mean_std(af)
        agg_rows.append(
            {
                "config_name": cfg,
                "num_ues": ues,
                "interval_s": iv,
                "enable_harq": int(harq),
                "n_seeds": len(rows),
                "payload_pdr_mean": pm,
                "payload_pdr_std": ps,
                "payload_mean_delay_ms_mean": dm,
                "payload_mean_delay_ms_std": ds,
                "attach_fail_mean": am,
            }
        )
        lines.append(
            f"| {cfg} | {iv:.2f} s | {'on' if harq else 'off'} | {fmt(pdr)} | {fmt(dly)} | {fmt(af, 0)} |"
        )

    lines += [
        "",
        "## Densification @ 0.2 s",
        "",
        "| Config | UEs | HARQ | PDR % | Delay ms | attach_fail |",
        "|--------|-----|------|-------|----------|-------------|",
    ]

    dense_keys = sorted(
        [k for k in groups if abs(k[2] - 0.2) < 1e-9 and k[1] != 40],
        key=lambda k: (k[1], k[3], k[0]),
    )
    # Also include 40@0.2 only in load section; densify section for 80/160
    for key in dense_keys:
        cfg, ues, iv, harq = key
        rows = groups[key]
        pdr = [f(r.get("payload_pdr")) for r in rows]
        dly = [f(r.get("payload_mean_delay_ms")) for r in rows]
        af = [f(r.get("attach_fail")) for r in rows]
        pm, ps = mean_std(pdr)
        dm, ds = mean_std(dly)
        am, _ = mean_std(af)
        # Avoid duplicate agg if already added as load (40@0.2)
        if not any(
            a["config_name"] == cfg
            and a["num_ues"] == ues
            and abs(a["interval_s"] - iv) < 1e-9
            and a["enable_harq"] == int(harq)
            for a in agg_rows
        ):
            agg_rows.append(
                {
                    "config_name": cfg,
                    "num_ues": ues,
                    "interval_s": iv,
                    "enable_harq": int(harq),
                    "n_seeds": len(rows),
                    "payload_pdr_mean": pm,
                    "payload_pdr_std": ps,
                    "payload_mean_delay_ms_mean": dm,
                    "payload_mean_delay_ms_std": ds,
                    "attach_fail_mean": am,
                }
            )
        lines.append(
            f"| {cfg} | {ues} | {'on' if harq else 'off'} | {fmt(pdr)} | {fmt(dly)} | {fmt(af, 0)} |"
        )

    # Pairwise delta: harq on − harq off at same (cfg,ues,iv)
    lines += [
        "",
        "## HARQ on − off (PDR pp)",
        "",
        "| Config | UEs | Interval | ΔPDR (pp) | ΔDelay (ms) |",
        "|--------|-----|----------|-----------|-------------|",
    ]
    base: dict[tuple, dict] = {}
    for a in agg_rows:
        key = (a["config_name"], a["num_ues"], a["interval_s"], a["enable_harq"])
        base[key] = a
    for cfg, ues, iv, _ in sorted({(a["config_name"], a["num_ues"], a["interval_s"], 0) for a in agg_rows}):
        off = base.get((cfg, ues, iv, 0))
        on = base.get((cfg, ues, iv, 1))
        if not off or not on:
            continue
        dp = on["payload_pdr_mean"] - off["payload_pdr_mean"]
        dd = on["payload_mean_delay_ms_mean"] - off["payload_mean_delay_ms_mean"]
        lines.append(
            f"| {cfg} | {ues} | {iv:.2f} s | {dp:+.1f} | {dd:+.1f} |"
        )

    lines += [
        "",
        "## Reading note",
        "",
        "- Positive ΔPDR with HARQ on supports the Elsevier IoT claim that IoTJ HARQ-off understates recovery under the error model.",
        "- If ΔPDR ≈ 0 at high SNR knees, say so: HARQ matters mainly where NACKs occur.",
        "- Ranking (eRedCap softest → min → max) may hold or shift — both are publishable if measured.",
        "- Do not claim Rel-18 HARQ timing fidelity from this campaign alone.",
        "",
        f"Agg CSV: `{OUT_CSV.relative_to(ROOT)}`",
        "",
    ]

    if not agg_rows:
        print("No aggregated rows")
        return 1

    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUT_CSV.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(agg_rows[0].keys()))
        w.writeheader()
        w.writerows(agg_rows)

    OUT_MD.write_text("\n".join(lines) + "\n")
    print(f"Wrote {OUT_MD}")
    print(f"Wrote {OUT_CSV}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
