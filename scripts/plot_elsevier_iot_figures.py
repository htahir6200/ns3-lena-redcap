#!/usr/bin/env python3
"""Elsevier IoT Option C figures from verified M3/M5 campaign aggregates."""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
FIG = ROOT / "paper" / "elsevier_iot" / "figures"
FIG.mkdir(parents=True, exist_ok=True)

COLORS = {
    "cap": "#0072B2",
    "true": "#D55E00",
    "redcap-max": "#009E73",
    "redcap-min": "#0072B2",
    "eredcap": "#D55E00",
    "1cell": "#0072B2",
    "2cell": "#D55E00",
}

mpl.rcParams.update(
    {
        "font.family": "serif",
        "font.size": 9,
        "axes.labelsize": 9,
        "axes.titlesize": 9,
        "legend.fontsize": 8,
        "xtick.labelsize": 8,
        "ytick.labelsize": 8,
        "axes.linewidth": 0.8,
        "figure.dpi": 150,
        "savefig.dpi": 300,
        "savefig.bbox": "tight",
        "savefig.pad_inches": 0.06,
        "axes.grid": True,
        "grid.alpha": 0.25,
        "grid.linestyle": "--",
    }
)


def load_csv(path: Path) -> list[dict]:
    with path.open() as f:
        return list(csv.DictReader(f))


def f(x) -> float:
    try:
        return float(x)
    except (TypeError, ValueError):
        return float("nan")


def pick(rows, **kw):
    out = []
    for r in rows:
        ok = True
        for k, v in kw.items():
            if str(r.get(k)) != str(v):
                ok = False
                break
        if ok:
            out.append(r)
    return out


def one(rows, **kw):
    hits = pick(rows, **kw)
    if not hits:
        raise KeyError(f"no row matching {kw}")
    return hits[0]


def finish(fig, out: Path) -> None:
    fig.tight_layout(pad=0.35)
    fig.savefig(out)
    plt.close(fig)
    print(f"Wrote {out}")


def fig_true5_pdr() -> None:
    rows = load_csv(RESULTS / "ul_t5_m3_agg.csv")
    intervals = [0.2, 0.1, 0.05]
    labels = ["0.20 s", "0.10 s", "0.05 s"]
    x = np.arange(len(intervals))
    w = 0.36

    cap_m, cap_s, true_m, true_s = [], [], [], []
    for iv in intervals:
        c = one(rows, config_name="eredcap", num_ues="40", interval_s=str(iv), mode="cap")
        t = one(rows, config_name="eredcap", num_ues="40", interval_s=str(iv), mode="true")
        cap_m.append(f(c["payload_pdr_mean"]))
        cap_s.append(f(c["payload_pdr_std"]))
        true_m.append(f(t["payload_pdr_mean"]))
        true_s.append(f(t["payload_pdr_std"]))

    fig, ax = plt.subplots(figsize=(3.45, 2.55))
    ax.bar(
        x - w / 2,
        cap_m,
        w,
        yerr=cap_s,
        capsize=2.5,
        color=COLORS["cap"],
        edgecolor="black",
        linewidth=0.5,
        label="Capability-style",
        error_kw={"elinewidth": 0.8},
    )
    ax.bar(
        x + w / 2,
        true_m,
        w,
        yerr=true_s,
        capsize=2.5,
        color=COLORS["true"],
        edgecolor="black",
        linewidth=0.5,
        hatch="///",
        label="True 5 MHz",
        error_kw={"elinewidth": 0.8},
    )
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylim(0, 110)
    ax.set_ylabel("Payload PDR (%)")
    ax.set_xlabel("Packet interval (40 UEs)")
    ax.legend(loc="upper right", framealpha=0.95)
    finish(fig, FIG / "iot_true5_pdr.pdf")


def fig_rank_pdr() -> None:
    rows = load_csv(RESULTS / "ul_t5_m3_agg.csv")
    intervals = [0.1, 0.05]
    cfgs = ["redcap-max", "redcap-min", "eredcap"]
    labels = ["RedCap-max", "RedCap-min", "eRedCap (true)"]
    x = np.arange(len(cfgs))
    w = 0.36

    means = {iv: [] for iv in intervals}
    stds = {iv: [] for iv in intervals}
    for iv in intervals:
        for cfg in cfgs:
            r = one(rows, config_name=cfg, num_ues="40", interval_s=str(iv), mode="true")
            means[iv].append(f(r["payload_pdr_mean"]))
            stds[iv].append(f(r["payload_pdr_std"]))

    fig, ax = plt.subplots(figsize=(3.45, 2.55))
    ax.bar(
        x - w / 2,
        means[0.1],
        w,
        yerr=stds[0.1],
        capsize=2.5,
        color="#56B4E9",
        edgecolor="black",
        linewidth=0.5,
        label="0.10 s",
        error_kw={"elinewidth": 0.8},
    )
    ax.bar(
        x + w / 2,
        means[0.05],
        w,
        yerr=stds[0.05],
        capsize=2.5,
        color=COLORS["eredcap"],
        edgecolor="black",
        linewidth=0.5,
        hatch="...",
        label="0.05 s",
        error_kw={"elinewidth": 0.8},
    )
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=12, ha="right")
    ax.set_ylim(0, 110)
    ax.set_ylabel("Payload PDR (%)")
    ax.set_xlabel("UE class (true 5 MHz eRedCap)")
    ax.legend(loc="upper right", framealpha=0.95)
    finish(fig, FIG / "iot_rank_pdr.pdf")


def fig_multicell_delay() -> None:
    rows = load_csv(RESULTS / "ul_m5_agg.csv")
    # Same total 40 UEs: 1x40 vs 2x20 at 0.2 and 0.1 s
    intervals = [0.2, 0.1]
    labels = ["0.20 s", "0.10 s"]
    x = np.arange(len(intervals))
    w = 0.36

    d1, s1, d2, s2 = [], [], [], []
    p1, ps1, p2, ps2 = [], [], [], []
    for iv in intervals:
        a = one(rows, config="eredcap", gnb_num="1", ues_per_gnb="40", interval_s=str(iv))
        b = one(rows, config="eredcap", gnb_num="2", ues_per_gnb="20", interval_s=str(iv))
        d1.append(f(a["delay_mean"]))
        s1.append(f(a["delay_std"]))
        d2.append(f(b["delay_mean"]))
        s2.append(f(b["delay_std"]))
        p1.append(f(a["pdr_mean"]))
        ps1.append(f(a["pdr_std"]))
        p2.append(f(b["pdr_mean"]))
        ps2.append(f(b["pdr_std"]))

    fig, axes = plt.subplots(1, 2, figsize=(7.0, 2.6))

    ax = axes[0]
    ax.bar(
        x - w / 2,
        p1,
        w,
        yerr=ps1,
        capsize=2.5,
        color=COLORS["1cell"],
        edgecolor="black",
        linewidth=0.5,
        label="1×40",
        error_kw={"elinewidth": 0.8},
    )
    ax.bar(
        x + w / 2,
        p2,
        w,
        yerr=ps2,
        capsize=2.5,
        color=COLORS["2cell"],
        edgecolor="black",
        linewidth=0.5,
        hatch="///",
        label="2×20",
        error_kw={"elinewidth": 0.8},
    )
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylim(0, 110)
    ax.set_ylabel("Payload PDR (%)")
    ax.set_xlabel("Packet interval")
    ax.legend(loc="upper right", framealpha=0.95)
    ax.set_title("(a) PDR")

    ax = axes[1]
    ax.bar(
        x - w / 2,
        d1,
        w,
        yerr=s1,
        capsize=2.5,
        color=COLORS["1cell"],
        edgecolor="black",
        linewidth=0.5,
        label="1×40",
        error_kw={"elinewidth": 0.8},
    )
    ax.bar(
        x + w / 2,
        d2,
        w,
        yerr=s2,
        capsize=2.5,
        color=COLORS["2cell"],
        edgecolor="black",
        linewidth=0.5,
        hatch="///",
        label="2×20",
        error_kw={"elinewidth": 0.8},
    )
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel("Mean delay (ms)")
    ax.set_xlabel("Packet interval")
    ax.set_yscale("log")
    ax.legend(loc="upper right", framealpha=0.95)
    ax.set_title("(b) Delay (log)")

    finish(fig, FIG / "iot_multicell_split.pdf")


def fig_m5b_percell() -> None:
    """Error-model-on per-cell PDR: 1×40 vs 2×40."""
    rows = load_csv(RESULTS / "ul_m5b_agg.csv")
    intervals = [0.2, 0.1]
    labels = ["0.20 s", "0.10 s"]
    x = np.arange(len(intervals))
    w = 0.36

    p1, s1, p2, s2 = [], [], [], []
    for iv in intervals:
        a = one(rows, config="eredcap", gnb_num="1", ues_per_gnb="40", interval_s=str(iv))
        b = one(rows, config="eredcap", gnb_num="2", ues_per_gnb="40", interval_s=str(iv))
        p1.append(f(a["pdr_mean"]))
        s1.append(f(a["pdr_std"]))
        p2.append(f(b["pdr_mean"]))
        s2.append(f(b["pdr_std"]))

    fig, ax = plt.subplots(figsize=(3.45, 2.55))
    ax.bar(
        x - w / 2,
        p1,
        w,
        yerr=s1,
        capsize=2.5,
        color=COLORS["1cell"],
        edgecolor="black",
        linewidth=0.5,
        label="1-cell (40 UEs)",
        error_kw={"elinewidth": 0.8},
    )
    ax.bar(
        x + w / 2,
        p2,
        w,
        yerr=s2,
        capsize=2.5,
        color=COLORS["2cell"],
        edgecolor="black",
        linewidth=0.5,
        hatch="///",
        label="2-cell (40 UEs/gNB)",
        error_kw={"elinewidth": 0.8},
    )
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylim(0, 110)
    ax.set_ylabel("Payload PDR (%)")
    ax.set_xlabel("Packet interval (error model on)")
    ax.legend(loc="upper right", framealpha=0.95)
    finish(fig, FIG / "iot_m5b_percell.pdf")


def _box(ax, x, y, w, h, text, fc, ec="0.2", fs=7.2, lw=1.0):
    r = plt.Rectangle((x, y), w, h, facecolor=fc, edgecolor=ec, lw=lw, zorder=2)
    ax.add_patch(r)
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs, zorder=3, linespacing=1.3)


def fig_system_architecture() -> None:
    """Simulator / evaluation-stack graphic for the System Model section."""
    fig, ax = plt.subplots(figsize=(7.2, 4.9))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 8.0)
    ax.axis("off")

    ax.text(6.0, 7.65, "ns-3.38 + LENA-RedCap evaluation stack (this paper)", ha="center", fontsize=10, fontweight="bold")

    # Scenario row
    _box(ax, 0.3, 6.15, 5.5, 1.2, "Scenario: FR1 TDD · μ=1\n1 or 2 gNBs (ISD 500 m) · AttachToClosestEnb\nMid-tier UL: 5000 B UDP · 36 s · DIT 60 s", "#DEEBF7", fs=7.0)
    _box(ax, 6.1, 6.15, 5.6, 1.2, "Device classes\nRedCap-min · RedCap-max · eRedCap\nFixed MCS (19 / 27)", "#E5F5E0", fs=7.0)

    # Stack
    _box(ax, 0.3, 4.55, 3.6, 1.35, "Apps / UDP + IP\nNrAccessStats · flow KPIs\nPDR · delay · attach", "#FFFFF0", fs=7.0)
    _box(ax, 4.15, 4.55, 3.7, 1.35, "RRC / MAC\nResManager / gNB\nCLI: true5MHz · HARQ · gNbNum", "#FEE6CE", fs=7.0)
    _box(ax, 8.1, 4.55, 3.6, 1.35, "NR PHY + spectrum\nError model on/off\nCo-channel when gNbNum=2", "#F0F0F0", fs=7.0)

    # Three upgrades
    ax.text(6.0, 4.15, "Three modelling upgrades vs prior capability-style baseline", ha="center", fontsize=8.5, fontweight="bold")
    _box(ax, 0.3, 2.55, 3.6, 1.4, "(1) True 5 MHz eRedCap BWP\nband/BWP = 5 MHz\nrbPerBwp = 12 · use5MHz = 0", "#FDBB84", fs=7.0)
    _box(ax, 4.15, 2.55, 3.7, 1.4, "(2) HARQ on ResManager path\nfeedback · insert · retx occupancy\n(classic HARQ-RR; not Rel-18 timing)", "#9ECAE1", fs=7.0)
    _box(ax, 8.1, 2.55, 3.6, 1.4, "(3) Static multi-cell\n1 ResManager / gNB\nerror model off and on @40 UE", "#A1D99B", fs=7.0)

    _box(
        ax,
        0.3,
        0.35,
        11.4,
        1.85,
        "Capability-style baseline (companion IoTJ): 20 MHz BWP + ≈¼-RB · HARQ off · 1 gNB\n"
        "This paper: true RF 5 MHz BWP · HARQ wire · 1–2 cells · fidelity box (NRB 12 vs 38.104’s 11; PBCH 4 RB)\n"
        "KPIs: payload PDR · delay of delivered packets · attach fails  |  ≥5 seeds · mean±std",
        "#FAFAFA",
        fs=7.0,
    )
    finish(fig, FIG / "iot_system_architecture.pdf")


def fig_bwp_modes() -> None:
    """Side-by-side bandwidth-part schematic: capability-style vs true 5 MHz."""
    fig, axes = plt.subplots(1, 2, figsize=(7.0, 2.7))

    # Capability-style
    ax = axes[0]
    ax.set_xlim(0, 51)
    ax.set_ylim(0, 3)
    ax.add_patch(plt.Rectangle((0, 1), 51, 1.2, facecolor="#DEEBF7", edgecolor="0.2", lw=1.0))
    ax.add_patch(plt.Rectangle((0, 1), 13, 1.2, facecolor="#D55E00", edgecolor="0.2", lw=1.0, alpha=0.85))
    ax.text(25.5, 2.55, "Capability-style eRedCap", ha="center", fontsize=9, fontweight="bold")
    ax.text(25.5, 0.35, "20 MHz BWP (51 PRB) · grants ≈¼ grid\n(IoTJ baseline; not a 5 MHz carrier)", ha="center", fontsize=7.5)
    ax.set_yticks([])
    ax.set_xlabel("PRB index")
    ax.set_title("(a)", fontsize=9)

    # True 5 MHz
    ax = axes[1]
    ax.set_xlim(0, 51)
    ax.set_ylim(0, 3)
    ax.add_patch(plt.Rectangle((0, 1), 12, 1.2, facecolor="#D55E00", edgecolor="0.2", lw=1.0))
    ax.add_patch(plt.Rectangle((0, 1), 4, 1.2, facecolor="#666666", edgecolor="0.2", lw=1.0, alpha=0.55))
    ax.text(25.5, 2.55, "True 5 MHz eRedCap BWP", ha="center", fontsize=9, fontweight="bold")
    ax.text(
        25.5,
        0.35,
        "Band/BWP = 5 MHz · 12 PRB (μ=1, oh=0.08)\nGrey: 4-PRB control reserve · no ¼-RB double penalty",
        ha="center",
        fontsize=7.5,
    )
    ax.set_yticks([])
    ax.set_xlabel("PRB index (same axis scale)")
    ax.set_title("(b)", fontsize=9)

    for ax in axes:
        ax.grid(False)
        for spine in ("top", "right", "left"):
            ax.spines[spine].set_visible(False)
    finish(fig, FIG / "iot_bwp_modes.pdf")


def fig_delta_pdr() -> None:
    """ΔPDR (true − capability) at 40 UE knees."""
    rows = load_csv(RESULTS / "ul_t5_m3_agg.csv")
    intervals = [0.2, 0.1, 0.05]
    labels = ["0.20 s", "0.10 s", "0.05 s"]
    deltas = []
    for iv in intervals:
        c = one(rows, config_name="eredcap", num_ues="40", interval_s=str(iv), mode="cap")
        t = one(rows, config_name="eredcap", num_ues="40", interval_s=str(iv), mode="true")
        deltas.append(f(t["payload_pdr_mean"]) - f(c["payload_pdr_mean"]))

    fig, ax = plt.subplots(figsize=(3.45, 2.55))
    colors = ["#D55E00" if d < 0 else "#009E73" for d in deltas]
    bars = ax.bar(labels, deltas, color=colors, edgecolor="black", linewidth=0.5, width=0.55)
    ax.axhline(0, color="0.3", lw=0.8)
    ax.set_ylabel("ΔPDR (true − capability) [pp]")
    ax.set_xlabel("Packet interval (40 UEs)")
    ax.set_ylim(min(deltas) - 8, 5)
    for b, d in zip(bars, deltas):
        ax.text(b.get_x() + b.get_width() / 2, d - 3.5, f"{d:.0f}", ha="center", va="top", fontsize=8)
    finish(fig, FIG / "iot_delta_pdr.pdf")


def fig_harq_softknee() -> None:
    """HARQ off vs on delay at a soft knee (classes @ 40 UE, 0.05 s)."""
    rows = load_csv(RESULTS / "ul_harq_m2_agg.csv")
    cfgs = [("redcap-min", "RedCap-min"), ("redcap-max", "RedCap-max"), ("eredcap", "eRedCap")]
    x = np.arange(len(cfgs))
    w = 0.36
    off_m, off_s, on_m, on_s = [], [], [], []
    for cfg, _ in cfgs:
        off = one(rows, config_name=cfg, num_ues="40", interval_s="0.05", enable_harq="0")
        on = one(rows, config_name=cfg, num_ues="40", interval_s="0.05", enable_harq="1")
        off_m.append(f(off["payload_mean_delay_ms_mean"]))
        off_s.append(f(off["payload_mean_delay_ms_std"]))
        on_m.append(f(on["payload_mean_delay_ms_mean"]))
        on_s.append(f(on["payload_mean_delay_ms_std"]))

    fig, ax = plt.subplots(figsize=(3.45, 2.55))
    ax.bar(
        x - w / 2,
        off_m,
        w,
        yerr=off_s,
        capsize=2.5,
        color="#0072B2",
        edgecolor="black",
        linewidth=0.5,
        label="HARQ off",
        error_kw={"elinewidth": 0.8},
    )
    ax.bar(
        x + w / 2,
        on_m,
        w,
        yerr=on_s,
        capsize=2.5,
        color="#D55E00",
        edgecolor="black",
        linewidth=0.5,
        hatch="///",
        label="HARQ on",
        error_kw={"elinewidth": 0.8},
    )
    ax.set_xticks(x)
    ax.set_xticklabels([lab for _, lab in cfgs], rotation=12, ha="right")
    ax.set_ylabel("Mean delay (ms)")
    ax.set_xlabel("Class @ 40 UEs, 0.05 s (error model on)")
    ax.set_yscale("log")
    ax.legend(loc="upper right", framealpha=0.95)
    finish(fig, FIG / "iot_harq_delay.pdf")


def main() -> None:
    fig_system_architecture()
    fig_bwp_modes()
    fig_true5_pdr()
    fig_delta_pdr()
    fig_rank_pdr()
    fig_harq_softknee()
    fig_multicell_delay()
    fig_m5b_percell()


if __name__ == "__main__":
    main()
