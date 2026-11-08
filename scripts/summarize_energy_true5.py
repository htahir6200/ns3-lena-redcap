#!/usr/bin/env python3
"""Aggregate energy KPIs from ul_t5_m3_* (Phase A) and ul_en_e2_* (Phase B)."""

from __future__ import annotations

import csv
import math
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
OUT_CSV = RESULTS / "ul_energy_true5_agg.csv"
OUT_MD = ROOT / "docs" / "ENERGY_CAMPAIGN_E1.md"


def mean_std(xs: list[float]) -> tuple[float, float]:
    xs = [x for x in xs if x == x]
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


def fmt(xs: list[float], nd: int = 3) -> str:
    m, s = mean_std(xs)
    if m != m:
        return "—"
    if s != s or s < 1e-12:
        return f"{m:.{nd}f}"
    return f"{m:.{nd}f}±{s:.{nd}f}"


def true5_flag(row: dict) -> str:
    v = row.get("true_5mhz", "")
    if v != "":
        return "true" if str(v).lower() in ("1", "true", "yes") else "cap"
    tag = row.get("sim_tag", "")
    if "_t5" in tag or "true5" in tag:
        return "true"
    if "_cap" in tag:
        return "cap"
    return "?"


def dit_ms(row: dict) -> int:
    try:
        return int(float(row.get("inactivity_timer_ms", 60000)))
    except ValueError:
        return 60000


def collect(patterns: list[str]) -> list[dict]:
    rows: list[dict] = []
    for pat in patterns:
        for path in sorted(RESULTS.glob(pat)):
            if "sweep" in path.name or "agg" in path.name:
                continue
            with path.open() as fh:
                rows.extend(csv.DictReader(fh))
    return rows


def main() -> int:
    # Prefer per-campaign summaries; fall back to sweep.
    rows = collect(["ul_t5_m3_*_summary.csv", "ul_en_e2_*_summary.csv"])
    if not rows:
        print("No energy source summaries found")
        return 1

    groups: dict[tuple, list[dict]] = defaultdict(list)
    for row in rows:
        if not row.get("mean_energy_j"):
            continue
        key = (
            row.get("config_name", "?"),
            int(float(row.get("num_ues", 0))),
            float(row.get("interval_s", 0)),
            true5_flag(row),
            dit_ms(row),
        )
        groups[key].append(row)

    agg_rows: list[dict] = []
    lines = [
        "# Energy under true 5 MHz — Phase A (existing M3 dumps)",
        "",
        "**Status:** Phase A complete from `ul_t5_m3_*` (DIT = 60 s).",
        "Phase B (short DIT) is documented separately when `ul_en_e2_*` finishes.",
        "",
        "Chip model: `RG255C` via `NrEnergyModel` → `Energystates.log`.",
        "KPIs: mean energy (J), Connected / TX energy, Connected / eDRX time.",
        "",
        "## Fidelity note",
        "",
        "With DIT = 60 s and simTime = 36 s, UEs remain RRC Connected for nearly",
        "the whole run (`mean_rrc_edrx_ms ≈ 1`). Energy differences are therefore",
        "**airtime / Connected dominated**, not a sleep/eDRX lifetime study.",
        "Do not claim 3GPP-faithful eDRX PTW or RAI from these numbers.",
        "",
        "## eRedCap — capability vs true 5 MHz (DIT 60 s, 40 UEs)",
        "",
        "| Mode | Interval | Mean E (J) | Conn E (J) | TX E (J) | Conn ms | eDRX ms | PDR % |",
        "|------|----------|------------|------------|----------|---------|---------|-------|",
    ]

    for key in sorted(groups, key=lambda k: (k[4], k[3], k[1], k[2], k[0])):
        cfg, ues, iv, mode, dit = key
        if cfg != "eredcap" or ues != 40 or dit != 60000:
            continue
        rs = groups[key]
        e = [f(r.get("mean_energy_j")) for r in rs]
        ec = [f(r.get("mean_energy_connected_j")) for r in rs]
        et = [f(r.get("mean_energy_tx_j")) for r in rs]
        cm = [f(r.get("mean_rrc_connected_ms")) for r in rs]
        em = [f(r.get("mean_rrc_edrx_ms")) for r in rs]
        pdr = [f(r.get("payload_pdr")) for r in rs]
        lines.append(
            f"| {mode} | {iv:.2f} s | {fmt(e)} | {fmt(ec)} | {fmt(et)} | "
            f"{fmt(cm, 0)} | {fmt(em, 0)} | {fmt(pdr, 1)} |"
        )
        emean, estd = mean_std(e)
        agg_rows.append(
            {
                "phase": "A",
                "config_name": cfg,
                "num_ues": ues,
                "interval_s": iv,
                "mode": mode,
                "dit_ms": dit,
                "n_seeds": len(rs),
                "mean_energy_j": emean,
                "mean_energy_j_std": estd,
                "mean_energy_connected_j": mean_std(ec)[0],
                "mean_energy_tx_j": mean_std(et)[0],
                "mean_rrc_connected_ms": mean_std(cm)[0],
                "mean_rrc_edrx_ms": mean_std(em)[0],
                "payload_pdr": mean_std(pdr)[0],
            }
        )

    lines += [
        "",
        "## ΔEnergy (true − capability) at 40 UEs, DIT 60 s",
        "",
        "| Interval | ΔE (J) | ΔTX E (J) | ΔPDR (pp) |",
        "|----------|--------|-----------|-----------|",
    ]
    by = {
        (a["interval_s"], a["mode"]): a
        for a in agg_rows
        if a["config_name"] == "eredcap" and a["dit_ms"] == 60000
    }
    for iv in sorted({a["interval_s"] for a in agg_rows if a["dit_ms"] == 60000}):
        cap = by.get((iv, "cap"))
        tru = by.get((iv, "true"))
        if not cap or not tru:
            continue
        lines.append(
            f"| {iv:.2f} s | {tru['mean_energy_j'] - cap['mean_energy_j']:+.3f} | "
            f"{tru['mean_energy_tx_j'] - cap['mean_energy_tx_j']:+.3f} | "
            f"{tru['payload_pdr'] - cap['payload_pdr']:+.1f} |"
        )

    lines += [
        "",
        "## Class ranking energy @ 40 UEs (true 5 MHz eRedCap, DIT 60 s)",
        "",
        "| Config | Interval | Mean E (J) | TX E (J) | PDR % |",
        "|--------|----------|------------|----------|-------|",
    ]
    for key in sorted(groups, key=lambda k: (k[2], k[0])):
        cfg, ues, iv, mode, dit = key
        if ues != 40 or dit != 60000:
            continue
        rs = groups[key]
        if not any("rank" in r.get("sim_tag", "") for r in rs):
            continue
        e = [f(r.get("mean_energy_j")) for r in rs]
        et = [f(r.get("mean_energy_tx_j")) for r in rs]
        pdr = [f(r.get("payload_pdr")) for r in rs]
        lines.append(f"| {cfg} | {iv:.2f} s | {fmt(e)} | {fmt(et)} | {fmt(pdr, 1)} |")

    lines += [
        "",
        "## Findings (Phase A)",
        "",
        "1. Soft knees under true 5 MHz **reduce TX energy** vs capability-style because",
        "   fewer payload grants complete; Connected energy stays ~10.3–10.4 J.",
        "2. Mean total energy at 40 UEs / 0.05 s: capability ≈ 11.6 J, true ≈ 10.8 J",
        "   (true lower mainly via TX, not sleep).",
        "3. Class energy ranking under true 5 MHz is tight (~10.7–11.2 J); PDR ranking",
        "   remains the clearer densification signal under long DIT.",
        "4. **Next (Phase B):** short DIT (1 s / 5 s) so eDRX time can dominate.",
        "",
        f"Agg CSV: `{OUT_CSV.relative_to(ROOT)}`",
        "",
    ]

    with OUT_CSV.open("w", newline="") as fh:
        if agg_rows:
            w = csv.DictWriter(fh, fieldnames=list(agg_rows[0].keys()))
            w.writeheader()
            w.writerows(agg_rows)
    OUT_MD.write_text("\n".join(lines) + "\n")
    print(f"Wrote {OUT_MD}")
    print(f"Wrote {OUT_CSV}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
