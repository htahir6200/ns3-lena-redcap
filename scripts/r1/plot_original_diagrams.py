#!/usr/bin/env python3
"""Original Fig. 1 (study framing) and Fig. 2 (architecture) with legible text (Reviewer 2, comment 11).

Layout, colours and wording follow the submitted figures:
  - Fig. 2 reuses fig_system_schematic() from scripts/plot_iotj_figures.py with larger fonts;
  - Fig. 1 had no script (drawn by hand); it is reproduced here box by box.
Only values that the revised study changed are updated (offered-load range, UE range, seeds).
Both are drawn close to their printed width so that text is about 8-9 pt in the paper.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib as mpl

mpl.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import FancyBboxPatch  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def fig_system_schematic(out: Path) -> None:
    """Same layout as plot_iotj_figures.fig_system_schematic(); fonts enlarged."""
    mpl.rcParams.update({"font.family": "serif"})
    fig, ax = plt.subplots(figsize=(7.16, 4.4))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 7.4)
    ax.axis("off")
    FS = 7.4

    def box(x, y, w, h, text, fc, ec="0.15", lw=1.15, fs=FS):
        ax.add_patch(plt.Rectangle((x, y), w, h, facecolor=fc, edgecolor=ec, lw=lw, zorder=2,
                                   joinstyle="round"))
        ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs, zorder=3,
                linespacing=1.3)

    ax.text(2.15, 6.95, "Radio access network (single cell)", ha="center", fontsize=8.0, fontweight="bold")
    box(0.25, 5.25, 3.8, 1.55, "gNB — FR1 20 MHz TDD\nMAC scheduler\nPDCCH / PUSCH", "#DEEBF7")
    box(0.25, 3.5, 3.8, 1.5, "NR PHY + spectrum channel\n(error model optional)", "#F0F0F0", fs=7.0)
    box(0.25, 1.75, 3.8, 1.5, "Access plane\nPRACH · RAR · RRC\nNrAccessStats", "#FEE6CE")

    ax.annotate("", xy=(5.15, 5.85), xytext=(4.2, 5.85), arrowprops=dict(arrowstyle="<->", color="#3182BD", lw=1.4))
    ax.text(4.7, 6.15, "data", ha="center", fontsize=7.6, color="#3182BD")
    ax.annotate("", xy=(5.15, 2.45), xytext=(4.2, 2.45), arrowprops=dict(arrowstyle="<->", color="#E6550D", lw=1.4))
    ax.text(4.7, 2.75, "access", ha="center", fontsize=7.6, color="#E6550D")

    ax.text(8.7, 6.95, "Device classes under test", ha="center", fontsize=8.0, fontweight="bold")
    box(5.3, 5.3, 3.15, 1.5, "RedCap-min\n1×1 · MCS 19\nfull BW", "#9ECAE1")
    box(8.65, 5.3, 3.15, 1.5, "RedCap-max\n2×2 · MCS 27\nfull BW", "#A1D99B")
    box(5.3, 3.5, 6.5, 1.55, "eRedCap (capability-style)\n1×1 · MCS 19 · −6 dB\n≈¼-RB on 20 MHz BWP", "#FDBB84")
    box(5.3, 1.75, 6.5, 1.55, "Optional eMBB background\n1500 B every 0.1 s\nclass-split PDR / delay", "#C6DBEF")
    box(0.25, 0.15, 11.55, 1.4,
        "KPIs: PDR · delay · UL RB util · PDCCH usage\n"
        "attach / RRC / PRACH  |  10 seeds · mean±std",
        "#FFFFF0")

    ax.set_title("Simulation architecture for the dense-cell RedCap/eRedCap study", fontsize=9.5, pad=4)
    for ext in ("pdf", "png"):
        fig.savefig(out / f"iotj_system_architecture_r1.{ext}", dpi=300, bbox_inches="tight", pad_inches=0.04)
    plt.close(fig)
    print("wrote", out / "iotj_system_architecture_r1")


def fig_intro_framing(out: Path) -> None:
    """Reproduction of the original hand-made framing figure (sans-serif), legible at column width."""
    mpl.rcParams.update({"font.family": "sans-serif"})
    fig = plt.figure(figsize=(3.5, 4.2))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    FS = 8.2

    def box(x, y, w, h, text, fc, fs=FS):
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.004,rounding_size=0.015",
                                    fc=fc, ec="#333333", lw=1.0))
        ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs, linespacing=1.2)

    ax.text(0.5, 0.998, "Study framing", ha="center", va="top", fontsize=10, fontweight="bold")
    box(0.015, 0.845, 0.97, 0.1, "Mid-tier IoT on FR1 (20 MHz TDD)\nRel-17 RedCap  ·  Rel-18 eRedCap\nns-3.38 + LENA-RedCap", "#DEEBF7")
    w = 0.313
    for i, (t, fc) in enumerate([("RedCap-min\n1×1\nMCS 19", "#9ECAE1"),
                                 ("RedCap-max\n2×2\nMCS 27", "#A1D99B"),
                                 ("eRedCap\n≈¼-RB\ncapability", "#FDBB84")]):
        box(0.015 + i * (w + 0.015), 0.725, w, 0.11, t, fc)
    box(0.015, 0.575, 0.477, 0.13, "Axis A — Offered load\nfix N=40\ndecrease T\n0.2 → 0.025 s", "#FEF5EB")
    box(0.508, 0.575, 0.477, 0.13, "Axis B — Densify\nfix T=0.2 s\nincrease N\n40 → 320 UE", "#F5FAF3")
    box(0.015, 0.385, 0.97, 0.17,
        "Q1  Where do PDR / delay knees appear?\nQ2  Does attach fail, or only the data plane?\n"
        "Q3  How does modest eMBB coexist\nbelow / near the knee?", "#FFFFF0")
    box(0.015, 0.21, 0.97, 0.155, "Headline reading\neRedCap → min → max (most robust)\nattach_fail = 0 at studied knees", "#E5F5E0")
    box(0.015, 0.035, 0.97, 0.155, "Fidelity disclosed\nfixed MCS · HARQ off\neRedCap on 20 MHz (not 5 MHz RF)", "#F0F0F0")
    for ext in ("pdf", "png"):
        fig.savefig(out / f"iotj_intro_framing_r1.{ext}", dpi=300)
    plt.close(fig)
    print("wrote", out / "iotj_intro_framing_r1")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "paper" / "iotj" / "figures"))
    out = Path(ap.parse_args().out)
    out.mkdir(parents=True, exist_ok=True)
    fig_intro_framing(out)
    fig_system_schematic(out)


if __name__ == "__main__":
    main()
