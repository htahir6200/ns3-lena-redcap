#!/usr/bin/env python3
"""Aggregate ul_t5_m3_* → mean±std for Elsevier IoT M3 true-5MHz vs capability-style."""

from __future__ import annotations

import csv
import math
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
OUT_CSV = RESULTS / "ul_t5_m3_agg.csv"
OUT_MD = ROOT / "docs" / "TRUE5MHZ_CAMPAIGN_M3.md"


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


def true5_flag(row: dict) -> str:
    v = row.get("true_5mhz", "")
    if v != "":
        return "true" if str(v).lower() in ("1", "true", "yes") else "cap"
    tag = row.get("sim_tag", "")
    if "_t5_" in tag or tag.endswith("_t5") or "_t5_s" in tag:
        return "true"
    if "_cap_" in tag or "_cap_s" in tag:
        return "cap"
    # rank campaigns force true5 for config 3
    if "ul_t5_m3_rank" in tag:
        return "true"
    return "?"


def main() -> int:
    paths = [p for p in sorted(RESULTS.glob("ul_t5_m3_*_summary.csv")) if "sweep" not in p.name]
    if not paths and (RESULTS / "ul_t5_m3_sweep_summary.csv").exists():
        paths = [RESULTS / "ul_t5_m3_sweep_summary.csv"]
    if not paths:
        print("No ul_t5_m3_*_summary.csv yet")
        return 1

    groups: dict[tuple, list[dict]] = defaultdict(list)
    for path in paths:
        with path.open() as fh:
            for row in csv.DictReader(fh):
                key = (
                    row.get("config_name", "?"),
                    int(float(row.get("num_ues", 0))),
                    float(row.get("interval_s", 0)),
                    true5_flag(row),
                )
                groups[key].append(row)

    agg_rows = []
    lines = [
        "# True 5 MHz campaign M3 (Elsevier IoT Option C)",
        "",
        "**Status: complete** when regenerated after `run_true5mhz_m3.py`.",
        "",
        "Compare **capability-style** eRedCap (20 MHz grid + ¼-RB, IoTJ) vs **true 5 MHz BWP** (12 RB @ μ=1, no ¼-RB).",
        "",
        "## eRedCap only — capability vs true 5 MHz",
        "",
        "| Mode | UEs | Interval | PDR % | Delay ms | attach_fail |",
        "|------|-----|----------|-------|----------|-------------|",
    ]

    for key in sorted(groups, key=lambda k: (k[3], k[1], k[2], k[0])):
        cfg, ues, iv, mode = key
        if cfg != "eredcap":
            continue
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
                "mode": mode,
                "n_seeds": len(rows),
                "payload_pdr_mean": pm,
                "payload_pdr_std": ps,
                "payload_mean_delay_ms_mean": dm,
                "payload_mean_delay_ms_std": ds,
                "attach_fail_mean": am,
            }
        )
        lines.append(
            f"| {mode} | {ues} | {iv:.2f} s | {fmt(pdr)} | {fmt(dly)} | {fmt(af, 0)} |"
        )

    lines += [
        "",
        "## Ranking @ 40 UEs (true 5 MHz for eRedCap)",
        "",
        "| Config | Interval | PDR % | Delay ms |",
        "|--------|----------|-------|----------|",
    ]
    for key in sorted(groups, key=lambda k: (k[2], k[0])):
        cfg, ues, iv, mode = key
        if ues != 40 or "rank" not in "".join(r.get("sim_tag", "") for r in groups[key][:1]):
            # include rank campaign rows: mode true for all, or any with configs 1/2
            pass
        rows = groups[key]
        if not any("rank" in r.get("sim_tag", "") for r in rows):
            continue
        pdr = [f(r.get("payload_pdr")) for r in rows]
        dly = [f(r.get("payload_mean_delay_ms")) for r in rows]
        lines.append(f"| {cfg} | {iv:.2f} s | {fmt(pdr)} | {fmt(dly)} |")
        if not any(
            a["config_name"] == cfg and a["num_ues"] == ues and abs(a["interval_s"] - iv) < 1e-9 and a["mode"] == mode
            for a in agg_rows
        ):
            pm, ps = mean_std(pdr)
            dm, ds = mean_std(dly)
            agg_rows.append(
                {
                    "config_name": cfg,
                    "num_ues": ues,
                    "interval_s": iv,
                    "mode": mode,
                    "n_seeds": len(rows),
                    "payload_pdr_mean": pm,
                    "payload_pdr_std": ps,
                    "payload_mean_delay_ms_mean": dm,
                    "payload_mean_delay_ms_std": ds,
                    "attach_fail_mean": mean_std([f(r.get("attach_fail")) for r in rows])[0],
                }
            )

    # Delta true − capability for eRedCap
    lines += [
        "",
        "## True − capability (eRedCap ΔPDR pp)",
        "",
        "| UEs | Interval | ΔPDR (pp) | ΔDelay (ms) |",
        "|-----|----------|-----------|-------------|",
    ]
    by = {(a["num_ues"], a["interval_s"], a["mode"]): a for a in agg_rows if a["config_name"] == "eredcap"}
    for ues, iv in sorted({(a["num_ues"], a["interval_s"]) for a in agg_rows if a["config_name"] == "eredcap"}):
        cap = by.get((ues, iv, "cap"))
        tru = by.get((ues, iv, "true"))
        if not cap or not tru:
            continue
        lines.append(
            f"| {ues} | {iv:.2f} s | {tru['payload_pdr_mean'] - cap['payload_pdr_mean']:+.1f} | "
            f"{tru['payload_mean_delay_ms_mean'] - cap['payload_mean_delay_ms_mean']:+.1f} |"
        )

    lines += [
        "",
        "## Notes",
        "",
        "- True 5 MHz: band/BWP = 5 MHz, ResManager `rbPerBwp=12`, `use5MHz=0` (no ¼-RB).",
        "- Capability-style: 20 MHz / 51 RB + `use5MHz=1` (IoTJ).",
        "- μ=1, RbOverhead=0.08 → 12 RB (38.104 Table 5.3.2-1 lists 11 for 5 MHz / 30 kHz).",
        "",
        f"Agg CSV: `{OUT_CSV.relative_to(ROOT)}`",
        "",
    ]

    if not agg_rows:
        print("No aggregated rows")
        return 1

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
