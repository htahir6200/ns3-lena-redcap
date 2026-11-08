#!/usr/bin/env python3
"""Publication-style OFAT figures for Energy E5 / E5b parameter sweeps."""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
FIG = ROOT / "paper" / "elsevier_iot" / "figures" / "energy_e5"
FIG.mkdir(parents=True, exist_ok=True)

C_MAIN = "#0072B2"
C_ALT = "#D55E00"
C_OK = "#009E73"
C_MUTE = "#666666"

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


def load(path: Path) -> list[dict]:
    with path.open() as fh:
        return list(csv.DictReader(fh))


def f(x) -> float:
    try:
        return float(x)
    except (TypeError, ValueError):
        return float("nan")


def axis_rows(rows: list[dict], axis: str) -> list[dict]:
    out = [r for r in rows if r.get("axis") == axis]

    def key(r):
        fac = r["factor"]
        try:
            return (0, float(fac))
        except ValueError:
            return (1, fac)

    return sorted(out, key=key)


def save(fig: plt.Figure, stem: str) -> None:
    for ext in ("pdf", "png"):
        p = FIG / f"{stem}.{ext}"
        fig.savefig(p)
        print(f"Wrote {p}")
    plt.close(fig)


def fig_ptw_energy(e5b: list[dict]) -> None:
    rows = axis_rows(e5b, "PTW")
    x = [int(r["factor"]) for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    fig, ax = plt.subplots(figsize=(3.4, 2.4))
    ax.errorbar(x, y, yerr=e, fmt="o-", color=C_MAIN, capsize=3, lw=1.4, ms=5)
    ax.set_xlabel("PTW length (ms)")
    ax.set_ylabel("Mean UE energy (J)")
    ax.set_xticks(x)
    ax.set_title("Energy vs PTW (IAT=10 s, RAI on)")
    save(fig, "e5b_ptw_energy")


def fig_edrx_energy(e5b: list[dict]) -> None:
    rows = axis_rows(e5b, "eDRX")
    frames = [int(r["factor"]) for r in rows]
    seconds = [fr * 0.01 for fr in frames]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    fig, ax = plt.subplots(figsize=(3.4, 2.4))
    ax.errorbar(seconds, y, yerr=e, fmt="s-", color=C_ALT, capsize=3, lw=1.4, ms=5)
    ax.set_xlabel("eDRX cycle (s)")
    ax.set_ylabel("Mean UE energy (J)")
    ax.set_xticks(seconds)
    ax.set_xticklabels([f"{s:g}" for s in seconds])
    ax.set_title("Energy vs eDRX cycle (PTW=1.28 s)")
    save(fig, "e5b_edrx_energy")


def fig_duty_stack(e5b: list[dict]) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(6.8, 2.6), sharey=True)
    sim_ms = 120_000.0
    for ax, axis, xlab, title in (
        (axes[0], "PTW", "PTW (ms)", "Dwell share vs PTW"),
        (axes[1], "eDRX", "eDRX frames", "Dwell share vs eDRX"),
    ):
        rows = axis_rows(e5b, axis)
        labels = [r["factor"] for r in rows]
        conn = np.array([f(r["mean_rrc_connected_ms"]) for r in rows]) / sim_ms
        ptw = np.array([f(r["mean_rrc_edrx_ptw_ms"]) for r in rows]) / sim_ms
        deep = np.array([f(r["mean_rrc_edrx_deep_ms"]) for r in rows]) / sim_ms
        x = np.arange(len(labels))
        ax.bar(x, conn, color=C_ALT, label="Connected", width=0.65)
        ax.bar(x, ptw, bottom=conn, color=C_MAIN, label="eDRX PTW", width=0.65)
        ax.bar(x, deep, bottom=conn + ptw, color=C_OK, label="eDRX deep", width=0.65)
        ax.set_xticks(x)
        ax.set_xticklabels(labels)
        ax.set_xlabel(xlab)
        ax.set_ylim(0, 1.05)
        ax.set_title(title)
    axes[0].set_ylabel("Time share of 120 s")
    axes[1].legend(loc="lower right", frameon=False)
    fig.suptitle("State dwell under sparse UL (IAT=10 s, RAI on)", y=1.02)
    save(fig, "e5b_dwell_stack")


def fig_rai_vs_dit(e5b: list[dict]) -> None:
    rows = axis_rows(e5b, "RAI")
    by: dict[int, dict[int, dict]] = {}
    for r in rows:
        parts = r["factor"].replace("rai=", "").replace("dit=", "").split("/")
        rai, dit = int(parts[0]), int(parts[1])
        by.setdefault(dit, {})[rai] = r
    dits = sorted(by)
    x = np.arange(len(dits))
    w = 0.35
    y0 = [f(by[d][0]["mean_energy_j"]) for d in dits]
    y1 = [f(by[d][1]["mean_energy_j"]) for d in dits]
    e0 = [f(by[d][0]["mean_energy_j_std"]) for d in dits]
    e1 = [f(by[d][1]["mean_energy_j_std"]) for d in dits]
    fig, ax = plt.subplots(figsize=(3.6, 2.5))
    ax.bar(x - w / 2, y0, w, yerr=e0, color=C_MUTE, label="RAI off", capsize=2)
    ax.bar(x + w / 2, y1, w, yerr=e1, color=C_OK, label="RAI on", capsize=2)
    ax.set_xticks(x)
    ax.set_xticklabels([f"{d / 1000:g} s" for d in dits])
    ax.set_xlabel("DIT")
    ax.set_ylabel("Mean UE energy (J)")
    ax.set_title("RAI vs DIT (IAT=10 s)")
    ax.legend(frameon=False)
    save(fig, "e5b_rai_vs_dit")


def fig_dit_energy(e5b: list[dict]) -> None:
    rows = axis_rows(e5b, "DIT")
    x = [int(r["factor"]) / 1000.0 for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    fig, ax = plt.subplots(figsize=(3.4, 2.4))
    ax.errorbar(x, y, yerr=e, fmt="D-", color=C_MUTE, capsize=3, lw=1.4, ms=5)
    ax.set_xlabel("DIT (s), RAI off")
    ax.set_ylabel("Mean UE energy (J)")
    ax.set_title("Energy vs DIT (RAI off)")
    save(fig, "e5b_dit_energy")


def fig_iat_energy_pdr(e5: list[dict]) -> None:
    """Side-by-side IAT panels — short enough for a single column."""
    rows = axis_rows(e5, "IAT")
    x = [f(r["factor"]) for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    pdr = [f(r["payload_pdr"]) for r in rows]
    labels = [f"{v:g}" for v in x]
    xpos = np.arange(len(x))

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(3.4, 2.15))
    ax1.errorbar(
        xpos, y, yerr=e, fmt="o-", color=C_MAIN, capsize=2.5, lw=1.3, ms=4.5, elinewidth=0.8
    )
    ax1.set_ylabel("Mean UE energy (J)")
    ax1.set_xlabel("IAT (s)")
    ax1.set_xticks(xpos)
    ax1.set_xticklabels(labels)
    ax1.set_ylim(0, max(y) + max(e) * 1.1 + 0.8)
    ax1.set_title("(a) Energy")

    ax2.plot(xpos, pdr, "s--", color=C_ALT, ms=4.5, lw=1.2)
    ax2.set_ylabel("Payload PDR (%)")
    ax2.set_xlabel("IAT (s)")
    ax2.set_xticks(xpos)
    ax2.set_xticklabels(labels)
    ax2.set_ylim(0, 105)
    ax2.set_title("(b) PDR")
    fig.tight_layout(pad=0.3)
    save(fig, "e5_iat_energy_pdr")


def fig_energy_combined(e5: list[dict], e5b: list[dict]) -> None:
    """Compact 3x2 panel for one figure* (shorter than stacked OFAT+IAT floats)."""
    fig, axes = plt.subplots(3, 2, figsize=(6.9, 4.35), constrained_layout=True)
    fig.set_constrained_layout_pads(w_pad=0.08, h_pad=0.08, hspace=0.12, wspace=0.08)

    # (a) PTW
    ax = axes[0, 0]
    rows = axis_rows(e5b, "PTW")
    x = [int(r["factor"]) for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    ax.errorbar(x, y, yerr=e, fmt="o-", color=C_MAIN, capsize=2.5, lw=1.2, ms=4)
    ax.set_xlabel("PTW (ms)")
    ax.set_ylabel("Energy (J)")
    ax.set_title("(a) PTW", pad=2)
    ax.set_xticks(x)

    # (b) eDRX
    ax = axes[0, 1]
    rows = axis_rows(e5b, "eDRX")
    sec = [int(r["factor"]) * 0.01 for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    ax.errorbar(sec, y, yerr=e, fmt="s-", color=C_ALT, capsize=2.5, lw=1.2, ms=4)
    ax.set_xlabel("eDRX cycle (s)")
    ax.set_ylabel("Energy (J)")
    ax.set_title("(b) eDRX cycle", pad=2)
    ax.set_xticks(sec)

    # (c) RAI
    ax = axes[1, 0]
    rows = axis_rows(e5b, "RAI")
    by: dict[int, dict[int, dict]] = {}
    for r in rows:
        parts = r["factor"].replace("rai=", "").replace("dit=", "").split("/")
        rai, dit = int(parts[0]), int(parts[1])
        by.setdefault(dit, {})[rai] = r
    dits = sorted(by)
    xx = np.arange(len(dits))
    w = 0.35
    ax.bar(xx - w / 2, [f(by[d][0]["mean_energy_j"]) for d in dits], w, color=C_MUTE, label="RAI off")
    ax.bar(xx + w / 2, [f(by[d][1]["mean_energy_j"]) for d in dits], w, color=C_OK, label="RAI on")
    ax.set_xticks(xx)
    ax.set_xticklabels([f"{d / 1000:g} s" for d in dits])
    ax.set_xlabel("DIT")
    ax.set_ylabel("Energy (J)")
    ax.set_title("(c) RAI vs DIT", pad=2)
    ax.legend(frameon=False, fontsize=7, loc="upper left")

    # (d) DIT
    ax = axes[1, 1]
    rows = axis_rows(e5b, "DIT")
    x = [int(r["factor"]) / 1000.0 for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    ax.errorbar(x, y, yerr=e, fmt="D-", color=C_MUTE, capsize=2.5, lw=1.2, ms=4)
    ax.set_xlabel("DIT (s), RAI off")
    ax.set_ylabel("Energy (J)")
    ax.set_title("(d) DIT only", pad=2)

    # (e)+(f) IAT
    rows = axis_rows(e5, "IAT")
    xv = [f(r["factor"]) for r in rows]
    ye = [f(r["mean_energy_j"]) for r in rows]
    ee = [f(r["mean_energy_j_std"]) for r in rows]
    pdr = [f(r["payload_pdr"]) for r in rows]
    labels = [f"{v:g}" for v in xv]
    xpos = np.arange(len(xv))

    ax = axes[2, 0]
    ax.errorbar(xpos, ye, yerr=ee, fmt="o-", color=C_MAIN, capsize=2.5, lw=1.2, ms=4, elinewidth=0.8)
    ax.set_ylabel("Energy (J)")
    ax.set_xlabel("IAT (s)")
    ax.set_xticks(xpos)
    ax.set_xticklabels(labels)
    ax.set_ylim(0, max(ye) + max(ee) * 1.1 + 0.5)
    ax.set_title("(e) Energy vs IAT", pad=2)

    ax = axes[2, 1]
    ax.plot(xpos, pdr, "s--", color=C_ALT, ms=4, lw=1.2)
    ax.set_ylabel("PDR (%)")
    ax.set_xlabel("IAT (s)")
    ax.set_xticks(xpos)
    ax.set_xticklabels(labels)
    ax.set_ylim(0, 105)
    ax.set_title("(f) PDR vs IAT", pad=2)

    save(fig, "e5_energy_combined")

def fig_panel_summary(e5b: list[dict]) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(6.8, 4.0))

    rows = axis_rows(e5b, "PTW")
    ax = axes[0, 0]
    x = [int(r["factor"]) for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    ax.errorbar(x, y, yerr=e, fmt="o-", color=C_MAIN, capsize=3, lw=1.3, ms=5)
    ax.set_xlabel("PTW (ms)")
    ax.set_ylabel("Energy (J)")
    ax.set_title("(a) PTW")
    ax.set_xticks(x)

    rows = axis_rows(e5b, "eDRX")
    ax = axes[0, 1]
    sec = [int(r["factor"]) * 0.01 for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    ax.errorbar(sec, y, yerr=e, fmt="s-", color=C_ALT, capsize=3, lw=1.3, ms=5)
    ax.set_xlabel("eDRX cycle (s)")
    ax.set_ylabel("Energy (J)")
    ax.set_title("(b) eDRX cycle")
    ax.set_xticks(sec)

    rows = axis_rows(e5b, "RAI")
    by: dict[int, dict[int, dict]] = {}
    for r in rows:
        parts = r["factor"].replace("rai=", "").replace("dit=", "").split("/")
        rai, dit = int(parts[0]), int(parts[1])
        by.setdefault(dit, {})[rai] = r
    dits = sorted(by)
    xx = np.arange(len(dits))
    w = 0.35
    ax = axes[1, 0]
    ax.bar(xx - w / 2, [f(by[d][0]["mean_energy_j"]) for d in dits], w, color=C_MUTE, label="RAI off")
    ax.bar(xx + w / 2, [f(by[d][1]["mean_energy_j"]) for d in dits], w, color=C_OK, label="RAI on")
    ax.set_xticks(xx)
    ax.set_xticklabels([f"{d / 1000:g} s" for d in dits])
    ax.set_xlabel("DIT")
    ax.set_ylabel("Energy (J)")
    ax.set_title("(c) RAI vs DIT")
    ax.legend(frameon=False, fontsize=7)

    rows = axis_rows(e5b, "DIT")
    ax = axes[1, 1]
    x = [int(r["factor"]) / 1000.0 for r in rows]
    y = [f(r["mean_energy_j"]) for r in rows]
    e = [f(r["mean_energy_j_std"]) for r in rows]
    ax.errorbar(x, y, yerr=e, fmt="D-", color=C_MUTE, capsize=3, lw=1.3, ms=5)
    ax.set_xlabel("DIT (s), RAI off")
    ax.set_ylabel("Energy (J)")
    ax.set_title("(d) DIT only")

    fig.suptitle(
        "Sparse OFAT under true 5 MHz eRedCap (IAT=10 s, 20 UE, 120 s)",
        y=1.01,
    )
    fig.tight_layout()
    save(fig, "e5b_panel_summary")


def main() -> int:
    e5 = load(RESULTS / "ul_energy_e5_param_agg.csv")
    e5b = load(RESULTS / "ul_energy_e5b_sparse_agg.csv")
    fig_ptw_energy(e5b)
    fig_edrx_energy(e5b)
    fig_duty_stack(e5b)
    fig_rai_vs_dit(e5b)
    fig_dit_energy(e5b)
    fig_iat_energy_pdr(e5)
    fig_panel_summary(e5b)
    fig_energy_combined(e5, e5b)
    print(f"Figures in {FIG}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
