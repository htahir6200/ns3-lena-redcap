#!/usr/bin/env python3
"""Aggregate ul_en_e5_* / ul_en_e5b_* summaries into axis tables (Abbas-style OFAT)."""

from __future__ import annotations

import argparse
import csv
import math
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"


def mean_std(xs: list[float]) -> tuple[float, float]:
    xs = [x for x in xs if x == x]
    if not xs:
        return float("nan"), float("nan")
    m = sum(xs) / len(xs)
    if len(xs) < 2:
        return m, 0.0
    v = sum((x - m) ** 2 for x in xs) / (len(xs) - 1)
    return m, math.sqrt(v)


def f(row: dict, k: str) -> float:
    try:
        return float(row.get(k, "nan"))
    except ValueError:
        return float("nan")


def fmt(xs: list[float], nd: int = 3) -> str:
    m, s = mean_std(xs)
    if m != m:
        return "—"
    if s < 1e-12:
        return f"{m:.{nd}f}"
    return f"{m:.{nd}f}±{s:.{nd}f}"


def factor_sort_key(factor: str):
    try:
        return (0, float(factor))
    except ValueError:
        return (1, factor)


def parse_meta(camp: str, tag: str) -> dict:
    meta = {
        "campaign": camp,
        "axis": "?",
        "ptw_ms": None,
        "edrx_frames": None,
        "dit_ms": None,
        "rai": None,
        "interval_s": None,
    }
    # Match axis after ul_en_e5_ or ul_en_e5b_
    if re.search(r"ul_en_e5b?_ptw_", camp):
        meta["axis"] = "PTW"
        m = re.search(r"_ptw_p(\d+)_", camp)
        if m:
            meta["ptw_ms"] = int(m.group(1))
    elif re.search(r"ul_en_e5b?_edrx_", camp):
        meta["axis"] = "eDRX"
        m = re.search(r"_edrx_e(\d+)_", camp)
        if m:
            meta["edrx_frames"] = int(m.group(1))
    elif re.search(r"ul_en_e5b?_dit_", camp):
        meta["axis"] = "DIT"
        m = re.search(r"_dit_d(\d+)_", camp)
        if m:
            meta["dit_ms"] = int(m.group(1))
    elif re.search(r"ul_en_e5b?_rai_", camp):
        meta["axis"] = "RAI"
        meta["rai"] = 1 if "_rai_rai1_" in camp else 0
        m = re.search(r"_dit(\d+)_", camp)
        if m:
            meta["dit_ms"] = int(m.group(1))
    elif re.search(r"ul_en_e5b?_iat_", camp):
        meta["axis"] = "IAT"
        m = re.search(r"_iat_i(\d+p?\d*)_", camp)
        if m:
            meta["interval_s"] = float(m.group(1).replace("p", "."))

    m = re.search(r"_ptw(\d+)", tag)
    if m and meta["ptw_ms"] is None:
        meta["ptw_ms"] = int(m.group(1))
    m = re.search(r"_edrx(\d+)", tag)
    if m and meta["edrx_frames"] is None:
        meta["edrx_frames"] = int(m.group(1))
    if meta["rai"] is None:
        meta["rai"] = 1 if "_rai1" in tag else 0
    return meta


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--prefix",
        default="ul_en_e5",
        choices=["ul_en_e5", "ul_en_e5b"],
        help="Campaign prefix to aggregate",
    )
    args = ap.parse_args()
    prefix = args.prefix
    out_csv = RESULTS / (
        "ul_energy_e5b_sparse_agg.csv" if prefix == "ul_en_e5b" else "ul_energy_e5_param_agg.csv"
    )
    out_md = ROOT / "docs" / (
        "ENERGY_CAMPAIGN_E5B_SPARSE.md"
        if prefix == "ul_en_e5b"
        else "ENERGY_CAMPAIGN_E5_PARAM.md"
    )

    BASE = ROOT / "ns-3.38" / "Results" / "Cni-Szenario"
    PARSE = ROOT / "scripts" / "parse_results.py"
    # e5b must be matched before bare e5 when scanning all dirs; here we filter by prefix.
    pat = re.compile(
        rf"^(?P<camp>{re.escape(prefix)}_.+)_ul_c(?P<cfg>\d+)_ues(?P<ues>\d+)_ps(?P<ps>\d+)"
        r"_err(?P<err>\d+)_harq(?P<harq>\d+)(?P<mode>_t5|_cap)?"
        r"(?:_ptw(?P<ptw>\d+))?(?:_rai1)?(?:_edrx(?P<edrx>\d+))?"
        r"(?:_g(?P<gnb>\d+))?_s(?P<seed>\d+)$"
    )
    by: dict[str, list] = defaultdict(list)
    for d in sorted(BASE.iterdir()):
        if not d.is_dir() or not d.name.startswith(prefix + "_"):
            continue
        # Avoid e5 matching e5b dirs when prefix is ul_en_e5
        if prefix == "ul_en_e5" and d.name.startswith("ul_en_e5b_"):
            continue
        m = pat.match(d.name)
        if not m or not list(d.glob("Ausgaben/**/accessStats_Seed*.txt")):
            continue
        by[m.group("camp")].append(m)

    HEADER = [
        "sim_tag",
        "seed",
        "red_cap_config",
        "config_name",
        "downlink",
        "num_ues",
        "embb_ues",
        "packet_size",
        "embb_packet_size",
        "interval_s",
        "embb_interval_s",
        "sim_time",
        "inactivity_timer_ms",
        "error_model",
        "enable_harq",
        "true_5mhz",
        "gnb_num",
        "gnb_isd",
        "enable_edrx_ptw",
        "enable_rai",
        "ptw_length_ms",
        "edrx_cycle_frames",
    ]
    for camp, ms in sorted(by.items()):
        rows = []
        for m in sorted(ms, key=lambda x: int(x.group("seed"))):
            meta = parse_meta(camp, m.group(0))
            iv = meta["interval_s"]
            if iv is None:
                if prefix == "ul_en_e5b":
                    iv = 10.0
                else:
                    im = re.search(r"_iat_i(\d+p?\d*)", camp)
                    iv = float(im.group(1).replace("p", ".")) if im else 1.0
            dit = meta["dit_ms"] or 1000
            dm = re.search(r"dit(\d+)", camp)
            if dm:
                dit = int(dm.group(1))
            ptw = int(m.group("ptw") or meta["ptw_ms"] or 0)
            edrx = int(m.group("edrx") or meta["edrx_frames"] or 256)
            st = 120.0 if (iv and iv >= 5.0) or prefix == "ul_en_e5b" else 60.0
            rows.append(
                {
                    "sim_tag": m.group(0),
                    "seed": int(m.group("seed")),
                    "red_cap_config": m.group("cfg"),
                    "config_name": "eredcap",
                    "downlink": False,
                    "num_ues": m.group("ues"),
                    "embb_ues": 0,
                    "packet_size": m.group("ps"),
                    "embb_packet_size": 1500,
                    "interval_s": iv,
                    "embb_interval_s": 0.1,
                    "sim_time": st,
                    "inactivity_timer_ms": dit,
                    "error_model": False,
                    "enable_harq": False,
                    "true_5mhz": 1,
                    "gnb_num": 1,
                    "gnb_isd": 500.0,
                    "enable_edrx_ptw": 1 if ptw else 0,
                    "enable_rai": 1 if "_rai1" in m.group(0) else 0,
                    "ptw_length_ms": ptw,
                    "edrx_cycle_frames": edrx,
                }
            )
        man = RESULTS / f"{camp}_manifest.csv"
        with man.open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=HEADER)
            w.writeheader()
            w.writerows(rows)
        out = RESULTS / f"{camp}_summary.csv"
        subprocess.call(
            [sys.executable, str(PARSE), "--manifest", str(man), "--out", str(out)],
            cwd=ROOT,
        )

    all_rows: list[dict] = []
    for p in sorted(RESULTS.glob(f"{prefix}_*_summary.csv")):
        if prefix == "ul_en_e5" and p.name.startswith("ul_en_e5b_"):
            continue
        with p.open() as fh:
            for row in csv.DictReader(fh):
                camp = re.sub(r"_ul_c.*", "", row.get("sim_tag", ""))
                row["_camp"] = camp
                row.update({f"_{k}": v for k, v in parse_meta(camp, row.get("sim_tag", "")).items()})
                all_rows.append(row)

    if not all_rows:
        print(f"No {prefix} summaries yet")
        return 1

    groups: dict[tuple, list] = defaultdict(list)
    for r in all_rows:
        axis = r.get("_axis", "?")
        if axis == "PTW":
            key = (axis, str(r.get("_ptw_ms")))
        elif axis == "eDRX":
            key = (axis, str(r.get("_edrx_frames")))
        elif axis == "DIT":
            key = (axis, str(r.get("_dit_ms")))
        elif axis == "RAI":
            key = (axis, f"rai={r.get('_rai')}/dit={r.get('_dit_ms')}")
        elif axis == "IAT":
            key = (axis, str(r.get("_interval_s")))
        else:
            key = (axis, r.get("_campaign", "?"))
        groups[key].append(r)

    title = (
        "# Energy Phase C / E5b — sparse OFAT (IAT = 10 s)"
        if prefix == "ul_en_e5b"
        else "# Energy Phase C / E5 — parameter sweep (OFAT)"
    )
    blurb = (
        "Sleep-dominated companion to E5: fixed **IAT = 10 s**, sim **120 s**, so PTW / eDRX / "
        "DIT / RAI effects are not drowned by release/reconnect thrashing at 1 s IAT."
        if prefix == "ul_en_e5b"
        else "One-factor-at-a-time sweeps inspired by the Guidelines / IEEE energy papers: "
        "vary **PTW**, **eDRX cycle**, **DIT**, **RAI**, and **packet interval** around a "
        "sleep-facing true-5-MHz eRedCap baseline (20 UE, 5000 B, sim 60 s, IAT 1 s)."
    )

    agg_rows = []
    lines = [
        title,
        "",
        blurb,
        "",
        f"Aggregate CSV: `{out_csv.relative_to(ROOT)}`",
        "",
    ]

    for axis in ("PTW", "eDRX", "DIT", "RAI", "IAT"):
        axis_keys = sorted([k for k in groups if k[0] == axis], key=lambda x: factor_sort_key(x[1]))
        if not axis_keys:
            continue
        lines.append(f"## Axis: {axis}")
        lines.append("")
        lines.append(
            "| Factor | n | Mean E (J) | Conn ms | Deep eDRX ms | PTW ms | TX E (J) | PDR % |"
        )
        lines.append("|--------|---|------------|---------|--------------|--------|----------|-------|")
        for key in axis_keys:
            rs = groups[key]
            e = [f(r, "mean_energy_j") for r in rs]
            c = [f(r, "mean_rrc_connected_ms") for r in rs]
            d = [f(r, "mean_rrc_edrx_deep_ms") for r in rs]
            p = [f(r, "mean_rrc_edrx_ptw_ms") for r in rs]
            tx = [f(r, "mean_energy_tx_j") for r in rs]
            pd = [f(r, "payload_pdr") for r in rs]
            em, es = mean_std(e)
            lines.append(
                f"| {key[1]} | {len(rs)} | {fmt(e)} | {fmt(c,1)} | {fmt(d,1)} | {fmt(p,1)} | {fmt(tx)} | {fmt(pd,1)} |"
            )
            agg_rows.append(
                {
                    "axis": axis,
                    "factor": key[1],
                    "n": len(rs),
                    "mean_energy_j": em,
                    "mean_energy_j_std": es,
                    "mean_rrc_connected_ms": mean_std(c)[0],
                    "mean_rrc_edrx_deep_ms": mean_std(d)[0],
                    "mean_rrc_edrx_ptw_ms": mean_std(p)[0],
                    "mean_energy_tx_j": mean_std(tx)[0],
                    "payload_pdr": mean_std(pd)[0],
                }
            )
        lines.append("")

    lines.append("## Reading guide")
    lines.append("")
    lines.append("- **PTW ↑** → more listen energy (if listen power ≫ deep sleep).")
    lines.append("- **eDRX ↑** → longer deep periods between PTWs (lower mean power when sleep dominates).")
    lines.append("- **DIT ↑** (RAI off) → longer Connected dwell after each UL.")
    lines.append("- **RAI on** → early release; Connected shrinks vs DIT-only.")
    lines.append("- **IAT ↑** → fewer UL bursts; sleep share grows (energy/packet falls).")
    lines.append("")

    out_md.write_text("\n".join(lines) + "\n")
    with out_csv.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(agg_rows[0].keys()) if agg_rows else ["axis"])
        w.writeheader()
        w.writerows(agg_rows)
    print(f"Wrote {out_md}")
    print(f"Wrote {out_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
