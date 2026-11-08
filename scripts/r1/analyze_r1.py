#!/usr/bin/env python3
"""Aggregate IoTJ R1 campaign summaries.

  python3 -I scripts/r1/analyze_r1.py results/r1/r1_base_summary.csv [more.csv ...] \
      --out results/r1/r1_agg.csv

Per operating point (all Job fields except seed/sim_tag) it reports mean, sample
std, n and a 95 % t-interval for each metric below.

Paired comparison (e.g. error model off vs on, same seeds):
  python3 -I scripts/r1/analyze_r1.py A.csv B.csv --paired error_model \
      --out results/r1/r1_err_paired.csv
pairs rows that differ only in the given field and reports the mean paired
difference with a 95 % t-interval.
"""

from __future__ import annotations

import argparse
import csv
import math
import statistics as st
from collections import defaultdict

# Two-sided 95 % Student-t critical values, df = n-1.
T95 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365, 8: 2.306,
       9: 2.262, 10: 2.228, 11: 2.201, 12: 2.179, 14: 2.145, 19: 2.093, 29: 2.045}

KEY_FIELDS = ["red_cap_config", "config_name", "num_ues", "interval_s", "sim_time", "packet_size",
              "embb_ues", "error_model", "enable_harq", "true_5mhz", "strict_phy", "drain_s",
              "mcs_override", "eredcap_power_offset_db", "dl_cce_reserve", "fix_dl_pdcch_prefilter"]


def t95(df: int) -> float:
    if df in T95:
        return T95[df]
    lower = [k for k in T95 if k <= df]
    return T95[max(lower)] if lower else 12.706


def f(row: dict, key: str) -> float:
    try:
        return float(row.get(key, "nan"))
    except ValueError:
        return float("nan")


def metrics(row: dict) -> dict:
    """Derived per-run metrics."""
    out = {}
    tx_s, rx_s = f(row, "rev.atstop.payload_tx_bytes"), f(row, "rev.atstop.payload_rx_bytes")
    out["pdr_at_stop"] = 100 * rx_s / tx_s if tx_s > 0 else float("nan")
    out["pdr_after_drain"] = f(row, "payload_pdr")
    out["delay_ms"] = f(row, "payload_mean_delay_ms")
    out["redcap_pdr_after_drain"] = f(row, "redcap_payload_pdr")
    out["embb_pdr_after_drain"] = f(row, "embb_payload_pdr")
    out["backlog_at_stop_MB"] = f(row, "rev.atstop.ul_backlog_bytes") / 1e6
    out["backlog_at_end_MB"] = f(row, "rev.queue.ul_backlog_bytes_at_end") / 1e6
    out["ues_backlogged_at_stop"] = f(row, "rev.atstop.ues_with_backlog")
    out["goodput_2nd_half_mbps"] = f(row, "ts.goodput_2nd_half_mbps")
    out["offered_2nd_half_mbps"] = f(row, "ts.offered_rate_2nd_half_mbps")
    out["softfails"] = f(row, "rev.sf.total")
    out["softfail_dropped_data_bytes"] = f(row, "rev.sf.PHY_RX_DATA_DROPPED_CTRL@gnb.bytes")
    sr = f(row, "rev.ue.sr_sent")
    out["sr_sent"] = sr
    out["sr_unanswered_pct"] = 100 * f(row, "rev.ue.sr_unanswered") / sr if sr > 0 else 0.0
    cand = f(row, "rev.sched.ul.ue_slot_candidates")
    out["ul_attempts"] = cand
    out["ul_blocked_pdcch_pct"] = 100 * f(row, "rev.sched.ul.not_scheduled.pdcch") / cand if cand > 0 else 0.0
    out["ul_refused_dl_reserve_pct"] = 100 * f(row, "rev.sched.pdcch_dl_reserve_refusals") / cand if cand > 0 else 0.0
    dlc = f(row, "rev.sched.pdcch_check.dl.ok") + f(row, "rev.sched.pdcch_check.dl.no_free_cce")
    out["dl_blocked_pdcch_pct"] = 100 * f(row, "rev.sched.pdcch_check.dl.no_free_cce") / dlc if dlc > 0 else 0.0
    out["ul_blocked_no_rb_pct"] = 100 * f(row, "rev.sched.ul.not_scheduled.no_rb") / cand if cand > 0 else 0.0
    used, free = f(row, "rev.res.pdcch_coreset_cells_used"), f(row, "rev.res.pdcch_coreset_cells_free")
    out["coreset_util_pct"] = 100 * used / (used + free) if used + free > 0 else float("nan")
    out["ul_rb_util_pct"] = 100 * f(row, "ul_rb_util")
    g = f(row, "rev.sched.ul.grants")
    out["ul_grants"] = g
    out["rb_per_grant"] = f(row, "rev.sched.ul.grant_rb_sum") / g if g > 0 else float("nan")
    out["sym_per_grant"] = f(row, "rev.sched.ul.grant_sym_sum") / g if g > 0 else float("nan")
    out["attach_fail"] = f(row, "attach_fail")
    out["rrc_connected"] = f(row, "rrc_connected")
    out["ok"] = f(row, "ok")
    return out


def load(paths: list[str]) -> list[dict]:
    rows = []
    for p in paths:
        with open(p, newline="") as fh:
            rows.extend(csv.DictReader(fh))
    return rows


def key_of(row: dict, drop: tuple[str, ...] = ()) -> tuple:
    return tuple((k, row.get(k, "")) for k in KEY_FIELDS if k not in drop)


def summarize(vals: list[float]) -> tuple[float, float, int, float]:
    vals = [v for v in vals if not math.isnan(v)]
    n = len(vals)
    if n == 0:
        return float("nan"), float("nan"), 0, float("nan")
    m = st.mean(vals)
    s = st.stdev(vals) if n > 1 else 0.0
    ci = t95(n - 1) * s / math.sqrt(n) if n > 1 else float("nan")
    return m, s, n, ci


def aggregate(rows: list[dict]) -> list[dict]:
    groups: dict[tuple, list[dict]] = defaultdict(list)
    for r in rows:
        groups[key_of(r)].append(r)
    out = []
    for k, rs in sorted(groups.items()):
        rec = dict(k)
        rec["n_runs"] = len(rs)
        ms = [metrics(r) for r in rs]
        for name in ms[0]:
            m, s, n, ci = summarize([x[name] for x in ms])
            rec[f"{name}_mean"] = m
            rec[f"{name}_std"] = s
            rec[f"{name}_ci95"] = ci
        out.append(rec)
    return out


def paired(rows: list[dict], field: str) -> list[dict]:
    by: dict[tuple, dict] = defaultdict(dict)
    for r in rows:
        if r.get("ok") not in ("1", 1):
            continue
        by[key_of(r, drop=(field,)) + (("seed", r["seed"]),)][r.get(field, "")] = r
    groups: dict[tuple, list[tuple[dict, dict]]] = defaultdict(list)
    for k, d in by.items():
        if len(d) != 2:
            continue
        (va, ra), (vb, rb) = sorted(d.items())
        groups[tuple(x for x in k if x[0] != "seed") + ((f"{field}_a", va), (f"{field}_b", vb))].append((ra, rb))
    out = []
    for k, pairs in sorted(groups.items()):
        rec = dict(k)
        rec["n_pairs"] = len(pairs)
        for name in ("pdr_at_stop", "pdr_after_drain", "delay_ms", "softfails", "ul_blocked_pdcch_pct"):
            diffs = [metrics(b)[name] - metrics(a)[name] for a, b in pairs]
            m, s, n, ci = summarize(diffs)
            rec[f"d_{name}_mean"] = m
            rec[f"d_{name}_std"] = s
            rec[f"d_{name}_ci95"] = ci
        out.append(rec)
    return out


def write(rows: list[dict], path: str) -> None:
    keys = []
    for r in rows:
        for k in r:
            if k not in keys:
                keys.append(k)
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {path} ({len(rows)} rows)")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("summaries", nargs="+")
    ap.add_argument("--out", required=True)
    ap.add_argument("--paired", default=None, help="field to pair on (e.g. error_model)")
    a = ap.parse_args()
    rows = load(a.summaries)
    write(paired(rows, a.paired) if a.paired else aggregate(rows), a.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
