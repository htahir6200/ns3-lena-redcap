#!/usr/bin/env python3
"""Aggregate ul_sens_*_summary.csv → mean±std for IoTJ M3."""

from __future__ import annotations

import csv
import math
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
OUT_CSV = RESULTS / "ul_sens_sweep_agg.csv"
OUT_MD = ROOT / "docs" / "SENSITIVITY_CAMPAIGN_M3.md"


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


def main() -> int:
    paths = sorted(RESULTS.glob("ul_sens_i*_err*_summary.csv"))
    if not paths:
        print("No ul_sens_*_summary.csv yet")
        return 1

    groups: dict[tuple, list[dict]] = defaultdict(list)
    for path in paths:
        with path.open() as fh:
            for row in csv.DictReader(fh):
                err = row.get("error_model", "")
                if err == "":
                    # infer from sim_tag
                    tag = row.get("sim_tag", "")
                    err = "1" if "_err1_" in tag else "0"
                key = (
                    row.get("config_name", "?"),
                    float(row.get("interval_s", 0)),
                    str(err).lower() in ("1", "true", "yes"),
                )
                groups[key].append(row)

    agg_rows = []
    lines = [
        "# Sensitivity campaign M3 (IoTJ)",
        "",
        "**Status: complete** (error-model on/off at M1 knees). HARQ still hardcoded off in scratch.",
        "",
        "Source: `results/campaigns/ul_sens_*_summary.csv`. Seeds as mean ± std.",
        "",
        "## 40 UEs — error model off vs on",
        "",
        "| Config | Interval | err | PDR % | Delay ms | attach_fail | prach_succ |",
        "|--------|----------|-----|-------|----------|-------------|------------|",
    ]

    for key in sorted(groups, key=lambda k: (k[1], k[2], k[0])):
        cfg, iv, err = key
        rows = groups[key]
        pdr = [f(r.get("payload_pdr")) for r in rows]
        dly = [f(r.get("payload_mean_delay_ms")) for r in rows]
        af = [f(r.get("attach_fail")) for r in rows]
        pr = [f(r.get("prach_success_rate")) for r in rows]
        pm, ps = mean_std(pdr)
        dm, ds = mean_std(dly)
        am, _ = mean_std(af)
        prm, prs = mean_std(pr)
        agg_rows.append(
            {
                "config_name": cfg,
                "interval_s": iv,
                "error_model": int(err),
                "n_seeds": len(rows),
                "payload_pdr_mean": pm,
                "payload_pdr_std": ps,
                "payload_mean_delay_ms_mean": dm,
                "payload_mean_delay_ms_std": ds,
                "attach_fail_mean": am,
                "prach_success_rate_mean": prm,
                "prach_success_rate_std": prs,
            }
        )
        lines.append(
            f"| {cfg} | {iv:.2f} s | {'on' if err else 'off'} | {fmt(pdr)} | {fmt(dly)} | {fmt(af, 0)} | {fmt(pr, 2)} |"
        )

    lines += [
        "",
        "## Reading note",
        "",
        "- If ranking (eRedCap softest → min → max most robust) holds with error model on, M1 claims are more IoTJ-credible.",
        "- If error model reverses ranking or collapses all configs equally, disclose and soften claims.",
        "- HARQ not exercised here.",
        "",
        f"Agg CSV: `{OUT_CSV.relative_to(ROOT)}`",
        "",
    ]

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
