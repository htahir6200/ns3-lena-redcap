#!/usr/bin/env python3
"""Framing (Fig. 1) and architecture (Fig. 2) diagrams for the IoTJ R1 manuscript.

Drawn at their printed size (IEEE column 3.5 in, text width 7.16 in) so that
all text is 7.5-9 pt on the page (Reviewer 2, comment 11).
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib as mpl

mpl.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
mpl.rcParams.update({"font.family": "serif", "font.size": 8})

C = {"min": "#cfe3f3", "max": "#cdebdc", "ered": "#f7d8c4", "embb": "#e6e6e6",
     "gnb": "#eef3f8", "inst": "#fdf6dc", "q": "#f4f4f4", "edge": "#333333"}


def box(ax, x, y, w, h, text, fc, fs=8, bold_first=False, ha="center"):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.004,rounding_size=0.012",
                                fc=fc, ec=C["edge"], lw=0.7))
    lines = text.split("\n")
    if bold_first:
        ax.text(x + w / 2 if ha == "center" else x + 0.012, y + h - 0.03, lines[0], ha=ha, va="top",
                fontsize=fs, fontweight="bold")
        ax.text(x + w / 2 if ha == "center" else x + 0.012, y + h - 0.03 - fs * 0.0062, "\n".join(lines[1:]),
                ha=ha, va="top", fontsize=fs - 0.5, linespacing=1.15)
    else:
        ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs, linespacing=1.15)


def arrow(ax, x0, y0, x1, y1, color="#555555", style="-|>"):
    ax.add_patch(FancyArrowPatch((x0, y0), (x1, y1), arrowstyle=style, mutation_scale=8,
                                 color=color, lw=0.9))


def framing(out: Path):
    fig = plt.figure(figsize=(3.5, 3.25))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    box(ax, 0.01, 0.885, 0.98, 0.105,
        "Mid-tier uplink IoT, one FR1 cell (20 MHz TDD), 5000 B\nns-3.38 + 5G-LENA / LENA-RedCap, audited simulator",
        C["gnb"], fs=7.8)
    w = 0.32
    for i, (t, fc) in enumerate([("RedCap-min\n1 antenna\nMCS 19, 20 MHz", C["min"]),
                                 ("RedCap-max\n2×2 array, 1 layer\nMCS 27, 20 MHz", C["max"]),
                                 ("eRedCap\nRel-18 BB-reduced:\n≤12 PRBs, MCS 19", C["ered"])]):
        box(ax, 0.01 + i * (w + 0.01), 0.705, w, 0.155, t, fc, fs=7.4, bold_first=True)
    box(ax, 0.01, 0.52, 0.485, 0.16, "Offered-load axis\nN = 40 UEs\nT = 0.2 → 0.025 s\n(8 → 64 Mb/s)",
        "#fbfbfb", fs=7.4, bold_first=True)
    box(ax, 0.505, 0.52, 0.485, 0.16, "Densification axis\nT = 0.2 s\nN = 40 → 320 UEs\n(8 → 64 Mb/s)",
        "#fbfbfb", fs=7.4, bold_first=True)
    box(ax, 0.01, 0.195, 0.98, 0.3,
        "Questions\n"
        "Q1  What uplink ceiling does each class reach?\n"
        "Q2  Which resource binds: data PRBs or PDCCH/CORESET?\n"
        "Q3  Do access and attach fail at the knee?\n"
        "Q4  How does near-knee RedCap traffic affect eMBB?\n"
        "Q5  Do the findings hold for Poisson / on-off traffic?",
        C["q"], fs=7.6, bold_first=True, ha="left")
    box(ax, 0.01, 0.01, 0.98, 0.16,
        "Controls disclosed\nfixed MCS · high SINR (error model / HARQ have no effect)\n"
        "single cell · 10 paired seeds · deterministic runs",
        C["inst"], fs=7.4, bold_first=True)
    for ext in ("pdf", "png"):
        fig.savefig(out / f"r1_framing.{ext}", dpi=300)
    plt.close(fig)
    print("wrote", out / "r1_framing")


def architecture(out: Path):
    fig = plt.figure(figsize=(7.16, 2.55))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    # UEs
    ax.text(0.13, 0.965, "UEs (one cell)", ha="center", va="top", fontsize=8.5, fontweight="bold")
    ys = [0.72, 0.52, 0.32, 0.10]
    labels = [("RedCap-min / RedCap-max\n5000 B UDP, periodic\nor Poisson / on-off", C["min"]),
              ("eRedCap\n≤12-PRB grants, −6 dB", C["ered"]),
              ("eMBB background (opt.)\n1500 B every 0.1 s", C["embb"]),
              ("UE MAC / RLC AM\nSR, BSR, retxBSR-Timer", "#ffffff")]
    for y, (t, fc) in zip(ys, labels):
        box(ax, 0.01, y, 0.24, 0.17, t, fc, fs=7.5)
    # gNB
    ax.text(0.47, 0.965, "gNB (FR1, 20 MHz, TDD DDSUUUUUUU)", ha="center", va="top", fontsize=8.5,
            fontweight="bold")
    box(ax, 0.31, 0.62, 0.32, 0.29,
        "PDCCH / CORESET (DL and S slots)\nOne CCE per DCI in the UE search space;\nUL grant only if a CCE is free",
        C["gnb"], fs=7.5, bold_first=True)
    box(ax, 0.31, 0.32, 0.32, 0.27,
        "Uplink scheduler + resource manager\nOFDMA round robin, ≤1 grant/UE/slot;\nPUSCH symbols 0–12, PUCCH symbol 13",
        C["gnb"], fs=7.5, bold_first=True)
    box(ax, 0.31, 0.06, 0.32, 0.23,
        "Access plane\nPRACH (config 199), RAR, RRC",
        C["gnb"], fs=7.5, bold_first=True)
    arrow(ax, 0.25, 0.80, 0.31, 0.76)
    arrow(ax, 0.31, 0.72, 0.25, 0.62)
    ax.text(0.28, 0.835, "SR/BSR", fontsize=6.8, ha="center")
    ax.text(0.28, 0.645, "DCI", fontsize=6.8, ha="center")
    arrow(ax, 0.25, 0.42, 0.31, 0.45)
    ax.text(0.28, 0.465, "PUSCH", fontsize=6.8, ha="center")
    arrow(ax, 0.25, 0.18, 0.31, 0.17)
    ax.text(0.28, 0.20, "RACH", fontsize=6.8, ha="center")
    # instrumentation
    ax.text(0.83, 0.965, "Instrumentation and KPIs", ha="center", va="top", fontsize=8.5, fontweight="bold")
    box(ax, 0.68, 0.53, 0.31, 0.38,
        "Per-run counters\nCORESET utilization · UL attempts\nblocked by PDCCH / no PRB · grant size\n"
        "SR sent / unanswered · RLC backlog/s\nhardening-site hits · stall detector",
        C["inst"], fs=7.4, bold_first=True)
    box(ax, 0.68, 0.06, 0.31, 0.42,
        "Reported metrics\nPDR at traffic stop and after 10 s drain\nsteady-state goodput · delay\n"
        "attach / RRC / PRACH success\nRedCap vs eMBB delivery\nmean ± std, paired 95 % CIs",
        "#ffffff", fs=7.4, bold_first=True)
    arrow(ax, 0.63, 0.76, 0.68, 0.74)
    arrow(ax, 0.63, 0.45, 0.68, 0.62)
    arrow(ax, 0.63, 0.17, 0.68, 0.25)
    for ext in ("pdf", "png"):
        fig.savefig(out / f"r1_architecture.{ext}", dpi=300)
    plt.close(fig)
    print("wrote", out / "r1_architecture")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "paper" / "iotj" / "figures"))
    out = Path(ap.parse_args().out)
    out.mkdir(parents=True, exist_ok=True)
    framing(out)
    architecture(out)


if __name__ == "__main__":
    main()
