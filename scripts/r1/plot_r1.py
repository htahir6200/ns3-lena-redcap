#!/usr/bin/env python3
"""IoTJ R1 figures from results/r1/f_*_agg.csv and per-run time series.

  <venv-with-matplotlib>/bin/python scripts/r1/plot_r1.py --out paper/iotj/figures

Writes r1_*.pdf and r1_*.png. Style matches scripts/plot_iotj_figures.py.
"""

from __future__ import annotations

import argparse
import csv
import glob
from pathlib import Path

import matplotlib as mpl

mpl.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
R1 = ROOT / "results" / "r1"
NS3R = ROOT / "ns-3.38" / "Results" / "Cni-Szenario"

COLORS = {"1": "#0072B2", "2": "#009E73", "3": "#D55E00"}
LABELS = {"1": "RedCap-min", "2": "RedCap-max", "3": "eRedCap"}
MARKERS = {"1": "o", "2": "s", "3": "^"}
CFGS = ["1", "2", "3"]

mpl.rcParams.update({
    "font.family": "serif", "font.size": 9, "axes.labelsize": 9, "axes.titlesize": 9,
    "legend.fontsize": 8, "xtick.labelsize": 8, "ytick.labelsize": 8,
    "axes.linewidth": 0.8, "lines.linewidth": 1.6, "lines.markersize": 5,
    "savefig.dpi": 300, "axes.grid": True, "grid.alpha": 0.25, "grid.linestyle": "--",
})


def load(name):
    return list(csv.DictReader((R1 / f"{name}_agg.csv").open()))


def m(r, k):
    try:
        return float(r[f"{k}_mean"])
    except (KeyError, ValueError):
        return float("nan")


def sd(r, k):
    try:
        x = float(r[f"{k}_std"])
        return 0.0 if x != x else x
    except (KeyError, ValueError):
        return 0.0


def nz(x):
    return 0.0 if x != x else x


def save(fig, out, name):
    for ext in ("pdf", "png"):
        fig.savefig(out / f"{name}.{ext}", bbox_inches="tight", pad_inches=0.04)
    plt.close(fig)
    print("wrote", out / name)


def series(rows, cfg, key_x, **filt):
    sel = [r for r in rows if r["red_cap_config"] == cfg and all(r.get(k) == v for k, v in filt.items())]
    return sorted(sel, key=lambda r: float(r[key_x]))


def fig_goodput_vs_offered(out):
    """Goodput vs offered load at 40 UEs: shows each class's ceiling (headline figure)."""
    base = load("f_base")
    fig, ax = plt.subplots(figsize=(3.45, 2.5), constrained_layout=True)
    xs_all = []
    for c in CFGS:
        rs = series(base, c, "interval_s", num_ues="40")
        x = [40 * 5000 * 8 / float(r["interval_s"]) / 1e6 for r in rs]
        y = [m(r, "goodput_2nd_half_mbps") for r in rs]
        e = [sd(r, "goodput_2nd_half_mbps") for r in rs]
        xs_all += x
        ax.errorbar(x, y, yerr=e, color=COLORS[c], marker=MARKERS[c], label=LABELS[c], capsize=2)
    lim = max(xs_all) * 1.05
    ax.plot([0, lim], [0, lim], color="0.5", lw=0.8, ls=":", label="Goodput = offered")
    ax.set_xlabel("Offered UL load [Mb/s] (40 UEs, 5000 B)")
    ax.set_ylabel("Delivered goodput [Mb/s]")
    ax.set_xlim(0, lim)
    ax.set_ylim(0, lim)
    ax.legend(loc="upper left", frameon=False)
    save(fig, out, "r1_goodput_vs_offered")


def fig_pdr_delay(out, axis):
    base = load("f_base")
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(3.45, 3.6), sharex=True, constrained_layout=True)
    for c in CFGS:
        if axis == "load":
            rs = series(base, c, "interval_s", num_ues="40")
            x = [40 * 5000 * 8 / float(r["interval_s"]) / 1e6 for r in rs]
        else:
            rs = series(base, c, "num_ues", interval_s="0.2")
            rs = sorted(rs, key=lambda r: int(r["num_ues"]))
            x = [int(r["num_ues"]) for r in rs]
        a1.errorbar(x, [m(r, "pdr_at_stop") for r in rs], yerr=[sd(r, "pdr_at_stop") for r in rs],
                    color=COLORS[c], marker=MARKERS[c], label=LABELS[c], capsize=2)
        a2.errorbar(x, [m(r, "delay_ms") for r in rs], yerr=[sd(r, "delay_ms") for r in rs],
                    color=COLORS[c], marker=MARKERS[c], capsize=2)
    a1.set_ylabel("Payload PDR at stop [%]")
    a1.set_ylim(40, 102)
    a1.legend(loc="lower left", frameon=False)
    a2.set_yscale("log")
    a2.set_ylabel("Delay of delivered\npackets [ms]")
    a2.set_xlabel("Offered UL load [Mb/s] (40 UEs)" if axis == "load" else "RedCap UEs in the cell (T = 0.2 s)")
    save(fig, out, f"r1_pdr_delay_{axis}")


def fig_bottleneck(out):
    """CORESET utilization, UL attempts blocked by PDCCH, UL RB utilization vs offered load."""
    base = load("f_base")
    fig, axs = plt.subplots(1, 3, figsize=(7.0, 2.2), sharex=True, constrained_layout=True)
    keys = [("ul_rb_util_pct", "UL data-RB utilization [%]"),
            ("coreset_util_pct", "CORESET utilization [%]"),
            ("ul_blocked_pdcch_pct", "UL attempts blocked\nby PDCCH [%]")]
    for ax, (k, lab) in zip(axs, keys):
        for c in CFGS:
            rs = series(base, c, "interval_s", num_ues="40")
            x = [40 * 5000 * 8 / float(r["interval_s"]) / 1e6 for r in rs]
            ax.errorbar(x, [nz(m(r, k)) for r in rs], yerr=[sd(r, k) for r in rs],
                        color=COLORS[c], marker=MARKERS[c], label=LABELS[c], capsize=2)
        ax.set_ylabel(lab)
        ax.set_xlabel("Offered UL load [Mb/s]")
        ax.set_ylim(0, 100)
    axs[0].legend(loc="upper left", frameon=False)
    save(fig, out, "r1_bottleneck")


def ts_rows(pattern):
    files = sorted(glob.glob(str(NS3R / pattern / "timeseries_Seed*.csv")))
    if not files:
        raise SystemExit(f"no time series for {pattern}")
    rows = [r for r in csv.DictReader(open(files[0])) if r["t_s"]]
    t = [float(r["t_s"]) for r in rows]
    tx = [int(r["tx_bytes"]) / 1e6 for r in rows]
    rx = [int(r["rx_bytes"]) / 1e6 for r in rows]
    q = [int(r["ul_backlog_bytes"]) / 1e6 for r in rows]
    return t, tx, rx, q


def fig_backlog(out):
    """Backlog grows linearly at saturation and drains after traffic stops."""
    fig, ax = plt.subplots(figsize=(3.45, 2.4), constrained_layout=True)
    cases = [("1", "f_base_load_c1_n40_t0p03_*_s1", "RedCap-min 40@0.03 s"),
             ("2", "f_base_load_c2_n40_t0p025_*_s1", "RedCap-max 40@0.025 s"),
             ("3", "f_base_load_c3_n40_t0p03_*_s1", "eRedCap 40@0.03 s")]
    stop = None
    for c, pat, lab in cases:
        t, tx, rx, q = ts_rows(pat)
        ax.plot(t, q, color=COLORS[c], label=lab)
        stop = next((t[i] for i in range(1, len(tx)) if tx[i] == tx[-1]), None)
    if stop:
        ax.axvline(stop, color="0.4", lw=0.8, ls="--")
        ax.text(stop + 0.3, ax.get_ylim()[1] * 0.92, "traffic stop", fontsize=7, color="0.3")
    ax.set_xlabel("Simulated time [s]")
    ax.set_ylabel("UL RLC backlog, all UEs [MB]")
    ax.legend(loc="upper left", frameon=False)
    save(fig, out, "r1_backlog_timeseries")


def fig_stall(out):
    """Original scheduler (DL PDCCH pre-filter reserving CCEs) vs fixed scheduler."""
    fig, ax = plt.subplots(figsize=(3.45, 2.4), constrained_layout=True)
    cases = [("r1_base_load_c1_n40_t0p05_drain_s10p0_s2", "original scheduler", "-"),
             ("f_base_load_c1_n40_t0p05_*_s2", "corrected scheduler", "--")]
    for pat, lab, ls in cases:
        t, tx, rx, q = ts_rows(pat)
        ax.plot(t, rx, color=COLORS["1"], ls=ls, label=f"delivered, {lab}")
    ax.plot(t, tx, color="0.5", lw=0.9, ls=":", label="offered")
    ax.set_xlabel("Simulated time [s]")
    ax.set_ylabel("Cumulative UL bytes [MB]")
    ax.set_title("RedCap-min, 40 UEs, T = 0.05 s, seed 2")
    ax.legend(loc="upper left", frameon=False)
    save(fig, out, "r1_stall_original_vs_fixed")


def fig_coex(out):
    coex = load("f_coex")
    fig, axs = plt.subplots(1, 2, figsize=(7.0, 2.3), sharey=True, constrained_layout=True)
    for ax, (n, t, title) in zip(axs, [("40", "0.03", "40 RedCap UEs, T = 0.03 s"),
                                         ("240", "0.2", "240 RedCap UEs, T = 0.2 s")]):
        width = 0.25
        for i, c in enumerate(CFGS):
            ys, es = [], []
            for e in ("5", "10"):
                r = next(r for r in coex if r["red_cap_config"] == c and r["num_ues"] == n
                         and r["interval_s"] == t and r["embb_ues"] == e)
                ys.append(m(r, "embb_pdr_after_drain"))
                es.append(sd(r, "embb_pdr_after_drain"))
            ax.bar([j + (i - 1) * width for j in range(2)], ys, width, yerr=es, capsize=2,
                   color=COLORS[c], label=LABELS[c])
        ax.set_xticks([0, 1])
        ax.set_xticklabels(["5 eMBB UEs", "10 eMBB UEs"])
        ax.set_title(title)
        ax.set_ylim(0, 105)
    axs[0].set_ylabel("eMBB payload PDR [%]")
    axs[0].legend(loc="upper left", frameon=False, ncol=1)
    save(fig, out, "r1_coex_embb")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "paper" / "iotj" / "figures"))
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    fig_goodput_vs_offered(out)
    fig_pdr_delay(out, "load")
    fig_pdr_delay(out, "dense")
    fig_bottleneck(out)
    fig_backlog(out)
    fig_stall(out)
    fig_coex(out)


if __name__ == "__main__":
    main()
