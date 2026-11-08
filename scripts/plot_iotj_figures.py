#!/usr/bin/env python3
"""IoTJ paper figures — clearer graphical style for IEEE two-column."""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
FIG = ROOT / "results" / "figures"
FIG.mkdir(parents=True, exist_ok=True)

# Colorblind-friendly (Okabe–Ito inspired)
COLORS = {
    "redcap-min": "#0072B2",
    "redcap-max": "#009E73",
    "eredcap": "#D55E00",
}
HATCH = {
    "redcap-min": "///",
    "redcap-max": "xxx",
    "eredcap": "...",
}
LABELS = {
    "redcap-min": "RedCap-min",
    "redcap-max": "RedCap-max",
    "eredcap": "eRedCap",
}
MARKERS = {
    "redcap-min": "o",
    "redcap-max": "s",
    "eredcap": "^",
}
CFGS = ["redcap-min", "redcap-max", "eredcap"]

mpl.rcParams.update(
    {
        "font.family": "serif",
        "font.size": 9,
        "axes.labelsize": 9,
        "axes.titlesize": 9,
        "legend.fontsize": 8,
        "xtick.labelsize": 8,
        "ytick.labelsize": 8,
        "axes.labelpad": 6,
        "xtick.major.pad": 4,
        "ytick.major.pad": 3,
        "axes.linewidth": 0.8,
        "lines.linewidth": 1.6,
        "lines.markersize": 6,
        "figure.dpi": 150,
        "savefig.dpi": 300,
        "savefig.bbox": "tight",
        "savefig.pad_inches": 0.08,
        "axes.grid": True,
        "grid.alpha": 0.25,
        "grid.linestyle": "--",
        "figure.constrained_layout.hspace": 0.18,
        "figure.constrained_layout.wspace": 0.08,
        "figure.constrained_layout.h_pad": 0.12,
        "figure.constrained_layout.w_pad": 0.05,
    }
)


def style_axes(ax, xlabel: str | None = None, ylabel: str | None = None) -> None:
    """Consistent spacing so tick labels do not collide with axis titles."""
    ax.tick_params(axis="x", which="major", pad=8)
    ax.tick_params(axis="y", which="major", pad=4)
    if ylabel is not None:
        ax.set_ylabel(ylabel, labelpad=8)
    if xlabel is not None:
        ax.set_xlabel(xlabel, labelpad=10)
        # Store for post-layout repositioning (tight_layout can crush labelpad).
        ax._iotj_xlabel = xlabel


def dual_panel_figure(figsize=(3.45, 4.85)):
    """Stacked panels sized for IEEE column without oversized whitespace."""
    fig, axes = plt.subplots(2, 1, figsize=figsize)
    return fig, axes


def finish_figure(fig, out: Path, *, hspace: float | None = 0.42) -> None:
    if fig.get_constrained_layout():
        fig.set_constrained_layout_pads(h_pad=0.10, w_pad=0.03, hspace=0.18, wspace=0.04)
    elif hspace is not None:
        fig.tight_layout(pad=0.40, h_pad=1.1, w_pad=0.25)
        fig.subplots_adjust(hspace=hspace, bottom=0.10, top=0.90)
    # Keep x titles clear of ticks without adding large empty PNG margins.
    for ax in fig.axes:
        xlabel = getattr(ax, "_iotj_xlabel", None)
        if xlabel:
            ax.set_xlabel(xlabel)
            ax.xaxis.set_label_coords(0.5, -0.17)
    fig.savefig(out, bbox_inches="tight", pad_inches=0.05)
    plt.close(fig)
    print(f"Wrote {out}")


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
            if k == "num_ues":
                ok &= abs(f(r["num_ues"]) - v) < 0.1
            elif k == "interval_s":
                ok &= abs(f(r["interval_s"]) - v) < 1e-9
            elif k == "embb_ues":
                ok &= abs(f(r["embb_ues"]) - v) < 0.1
            elif k == "error_model":
                ok &= int(float(r["error_model"])) == int(v)
            elif k == "grid":
                ok &= r["grid"] == v
            elif k == "config_name":
                ok &= r["config_name"] == v
            else:
                ok &= r.get(k) == v
        if ok:
            out.append(r)
    return out


def grouped_bars(
    ax,
    categories,
    series,
    ylabel,
    title,
    ylim=(0, 110),
    *,
    xlabel: str | None = None,
    annotate: bool = True,
    value_fmt: str = "{:.0f}",
    legend_loc: str = "best",
):
    """series: dict cfg -> (means, stds) aligned with categories."""
    x = np.arange(len(categories))
    n = len(CFGS)
    width = 0.78 / n
    y0, y1 = ylim
    yspan = max(y1 - y0, 1e-9)
    for i, cfg in enumerate(CFGS):
        means, stds = series[cfg]
        offset = (i - (n - 1) / 2) * width
        bars = ax.bar(
            x + offset,
            means,
            width,
            yerr=stds,
            label=LABELS[cfg],
            color=COLORS[cfg],
            edgecolor="black",
            linewidth=0.4,
            hatch=HATCH[cfg],
            capsize=2.5,
            error_kw={"elinewidth": 0.8},
            zorder=3,
        )
        if annotate:
            for b, m, s in zip(bars, means, stds):
                if m != m:
                    continue
                # Skip near-ceiling values on 0–100 style axes.
                if y1 >= 100 and m >= 99.5:
                    continue
                tip = m + (s if s == s else 0)
                y_txt = tip + 0.035 * yspan
                # Keep labels inside the axes, above the bar.
                y_txt = min(max(y_txt, y0 + 0.08 * yspan), y1 - 0.04 * yspan)
                ax.text(
                    b.get_x() + b.get_width() / 2,
                    y_txt,
                    value_fmt.format(m),
                    ha="center",
                    va="bottom",
                    fontsize=6.5,
                    color=COLORS[cfg],
                    clip_on=True,
                    zorder=4,
                )
    ax.set_xticks(x, categories)
    style_axes(ax, xlabel=xlabel, ylabel=ylabel)
    ax.set_title(title, pad=8)
    ax.set_ylim(y0, y1)
    if y1 >= 100:
        ax.axhline(100, color="0.5", lw=0.6, ls=":", zorder=1)
    ax.legend(frameon=True, fancybox=False, edgecolor="0.7", loc=legend_loc)


def line_with_band(ax, xs, means, stds, cfg):
    means = np.asarray(means, dtype=float)
    stds = np.asarray(stds, dtype=float)
    ax.plot(
        xs,
        means,
        color=COLORS[cfg],
        marker=MARKERS[cfg],
        label=LABELS[cfg],
        zorder=3,
    )
    ax.fill_between(
        xs,
        means - stds,
        means + stds,
        color=COLORS[cfg],
        alpha=0.18,
        linewidth=0,
        zorder=2,
    )


def fig_system_schematic() -> None:
    """Layered architecture graphic for System Model section."""
    fig, ax = plt.subplots(figsize=(7.6, 4.7))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 7.4)
    ax.axis("off")

    def box(x, y, w, h, text, fc, ec="0.15", lw=1.15, fs=7.0):
        r = plt.Rectangle(
            (x, y), w, h, facecolor=fc, edgecolor=ec, lw=lw, zorder=2, joinstyle="round"
        )
        ax.add_patch(r)
        ax.text(
            x + w / 2,
            y + h / 2,
            text,
            ha="center",
            va="center",
            fontsize=fs,
            zorder=3,
            linespacing=1.35,
        )

    # Left: gNB
    ax.text(2.15, 6.85, "Radio access network (single cell)", ha="center", fontsize=8.5, fontweight="bold")
    box(0.25, 5.25, 3.8, 1.55, "gNB — FR1 20 MHz TDD\nMAC scheduler\nPDCCH / PUSCH", "#DEEBF7", fs=7)
    box(0.25, 3.5, 3.8, 1.5, "NR PHY + spectrum channel\n(error model optional)", "#F0F0F0", fs=7)
    box(0.25, 1.75, 3.8, 1.5, "Access plane\nPRACH · RAR · RRC\nNrAccessStats", "#FEE6CE", fs=7)

    # Middle arrows
    ax.annotate("", xy=(5.15, 5.85), xytext=(4.2, 5.85), arrowprops=dict(arrowstyle="<->", color="#3182BD", lw=1.4))
    ax.text(4.7, 6.15, "data", ha="center", fontsize=7, color="#3182BD")
    ax.annotate("", xy=(5.15, 2.45), xytext=(4.2, 2.45), arrowprops=dict(arrowstyle="<->", color="#E6550D", lw=1.4))
    ax.text(4.7, 2.75, "access", ha="center", fontsize=7, color="#E6550D")

    # Right: UE classes
    ax.text(8.7, 6.85, "Device classes under test", ha="center", fontsize=8.5, fontweight="bold")
    box(5.3, 5.3, 3.15, 1.5, "RedCap-min\n1×1 · MCS 19\nfull BW", "#9ECAE1", fs=7)
    box(8.65, 5.3, 3.15, 1.5, "RedCap-max\n2×2 · MCS 27\nfull BW", "#A1D99B", fs=7)
    box(5.3, 3.5, 6.5, 1.55, "eRedCap (capability-style)\n1×1 · MCS 19 · −6 dB\n≈¼-RB on 20 MHz BWP", "#FDBB84", fs=7)
    box(5.3, 1.75, 6.5, 1.55, "Optional eMBB background\n1500 B every 0.1 s\nclass-split PDR / delay", "#C6DBEF", fs=7)

    # Bottom KPI strip
    box(
        0.25,
        0.15,
        11.55,
        1.4,
        "KPIs: PDR · delay · UL RB util · PDCCH usage\n"
        "attach / RRC / PRACH  |  5 seeds · mean±std",
        "#FFFFF0",
        fs=7,
    )

    ax.set_title("Simulation architecture for the dense-cell RedCap/eRedCap study", fontsize=10.5, pad=6)
    out = FIG / "iotj_system_architecture.png"
    finish_figure(fig, out, hspace=None)



def fig_control_plane_load() -> None:
    """PDCCH usage vs UL RB util at offered-load points (40 UE)."""
    rows = load_csv(RESULTS / "ul_access_knee_agg.csv")
    ivs = (0.2, 0.1, 0.05)
    cats = ["0.20 s", "0.10 s", "0.05 s"]
    fig, axes = plt.subplots(2, 1, figsize=(3.45, 4.85))

    # (a) UL RB util
    series_rb = {}
    for cfg in CFGS:
        means, stds = [], []
        for iv in ivs:
            matches = pick(rows, config_name=cfg, num_ues=40, interval_s=iv)
            matches.sort(key=lambda r: abs(f(r["n_seeds"]) - 5))
            r = matches[0]
            means.append(100.0 * f(r["ul_rb_util_mean"]))
            stds.append(100.0 * f(r["ul_rb_util_std"]))
        series_rb[cfg] = (means, stds)
    grouped_bars(
        axes[0],
        cats,
        series_rb,
        "UL RB utilization [%]",
        "(a) UL data RB utilization",
        ylim=(0, 100),
        legend_loc="upper left",
    )

    # (b) PDCCH used
    series_p = {}
    raw = {}
    for cfg in CFGS:
        means, stds = [], []
        for iv in ivs:
            matches = pick(rows, config_name=cfg, num_ues=40, interval_s=iv)
            matches.sort(key=lambda r: abs(f(r["n_seeds"]) - 5))
            r = matches[0]
            means.append(f(r["pdcch_used_mean"]))
            stds.append(f(r["pdcch_used_std"]))
        raw[cfg] = (means, stds)
    for cfg in CFGS:
        m, s = raw[cfg]
        series_p[cfg] = ([x / 1e6 for x in m], [x / 1e6 for x in s])
    grouped_bars(
        axes[1],
        cats,
        series_p,
        "PDCCH usage [×10⁶]",
        "(b) Cumulative PDCCH usage",
        ylim=(0, 1.9),
        xlabel="Packet interval",
        value_fmt="{:.2f}",
        legend_loc="upper left",
    )
    fig.suptitle("Control vs data-plane pressure @ 40 UEs (5 seeds)", fontsize=9)
    out = FIG / "iotj_m1_control_plane.png"
    finish_figure(fig, out)


def fig_access_load() -> None:
    rows = load_csv(RESULTS / "ul_access_knee_agg.csv")
    cats = ["5 pkt/s", "10 pkt/s", "20 pkt/s"]
    ivs = (0.2, 0.1, 0.05)
    series = {}
    delay_m, delay_s = {}, {}
    for cfg in CFGS:
        means, stds, dm, ds = [], [], [], []
        for iv in ivs:
            matches = pick(rows, config_name=cfg, num_ues=40, interval_s=iv)
            matches.sort(key=lambda r: abs(f(r["n_seeds"]) - 5))
            r = matches[0]
            means.append(f(r["payload_pdr_mean"]))
            stds.append(f(r["payload_pdr_std"]))
            dm.append(f(r["payload_mean_delay_ms_mean"]))
            ds.append(f(r["payload_mean_delay_ms_std"]))
        series[cfg] = (means, stds)
        delay_m[cfg], delay_s[cfg] = dm, ds

    fig, axes = plt.subplots(2, 1, figsize=(3.45, 4.85))
    grouped_bars(
        axes[0],
        cats,
        series,
        "Payload PDR [%]",
        "(a) Packet delivery ratio",
        legend_loc="lower center",
    )
    xs = [5, 10, 20]
    for cfg in CFGS:
        line_with_band(axes[1], xs, delay_m[cfg], delay_s[cfg], cfg)
    axes[1].set_yscale("log")
    style_axes(axes[1], xlabel="Offered rate [packets/s/UE]", ylabel="Mean delay [ms]")
    axes[1].set_title("(b) Mean delay (log scale)", pad=8)
    axes[1].set_xticks(xs)
    axes[1].legend(frameon=True, fancybox=False, edgecolor="0.7", loc="upper left")
    fig.suptitle("UL offered load @ 40 UEs (5 seeds)", fontsize=9)
    out = FIG / "iotj_m1_access_load.png"
    finish_figure(fig, out)


def fig_access_dense02() -> None:
    rows = load_csv(RESULTS / "ul_access_knee_agg.csv")
    cats = ["40 UE", "80 UE", "160 UE"]
    ues_list = (40, 80, 160)
    series = {}
    delay_m, delay_s = {}, {}
    for cfg in CFGS:
        means, stds, dm, ds = [], [], [], []
        for ues in ues_list:
            matches = pick(rows, config_name=cfg, num_ues=ues, interval_s=0.2)
            matches.sort(key=lambda r: f(r["n_seeds"]))
            r = matches[0]
            means.append(f(r["payload_pdr_mean"]))
            stds.append(f(r["payload_pdr_std"]))
            dm.append(f(r["payload_mean_delay_ms_mean"]))
            ds.append(f(r["payload_mean_delay_ms_std"]))
        series[cfg] = (means, stds)
        delay_m[cfg], delay_s[cfg] = dm, ds

    fig, axes = plt.subplots(2, 1, figsize=(3.45, 4.85))
    grouped_bars(
        axes[0],
        cats,
        series,
        "Payload PDR [%]",
        "(a) Packet delivery ratio",
        legend_loc="lower left",
    )
    xs = [40, 80, 160]
    for cfg in CFGS:
        line_with_band(axes[1], xs, delay_m[cfg], delay_s[cfg], cfg)
    axes[1].set_yscale("log")
    style_axes(axes[1], xlabel="Number of UEs", ylabel="Mean delay [ms]")
    axes[1].set_title("(b) Mean delay (log scale)", pad=8)
    axes[1].set_xticks(xs)
    axes[1].legend(frameon=True, fancybox=False, edgecolor="0.7", loc="upper left")
    fig.suptitle("UL densification @ 0.2 s interval (5 seeds)", fontsize=9)
    out = FIG / "iotj_m1_access_dense02.png"
    finish_figure(fig, out)


def fig_coexist_stress() -> None:
    rows = load_csv(RESULTS / "ul_coexist_stress_agg.csv")
    fig, axes = plt.subplots(2, 1, figsize=(3.45, 4.85))
    for ax, grid, title, xlabel in zip(
        axes,
        ("i0p1", "u80"),
        ("(a) 40 RedCap @ 0.1 s", "(b) 80 RedCap @ 0.2 s"),
        (None, "eMBB background UEs"),
    ):
        cats = ["0 eMBB", "5 eMBB", "10 eMBB"]
        series = {}
        for cfg in CFGS:
            means, stds = [], []
            for embb in (0, 5, 10):
                m = pick(rows, grid=grid, config_name=cfg, embb_ues=embb)[0]
                means.append(f(m["redcap_payload_pdr_mean"]))
                stds.append(f(m["redcap_payload_pdr_std"]))
            series[cfg] = (means, stds)
        grouped_bars(
            ax,
            cats,
            series,
            "RedCap PDR [%]",
            title,
            xlabel=xlabel,
            legend_loc="lower right",
        )
    fig.suptitle("Stressed RedCap–eMBB coexistence (5 seeds)", fontsize=9)
    out = FIG / "iotj_m2b_coexist_stress.png"
    finish_figure(fig, out)


def fig_coexist_embb_under_eredcap() -> None:
    rows = load_csv(RESULTS / "ul_coexist_stress_agg.csv")
    fig, ax = plt.subplots(figsize=(3.45, 2.55))
    x = np.array([5, 10])
    width = 0.35
    for i, (grid, label, hatch) in enumerate(
        (("i0p1", "40 UE @ 0.1 s", "//"), ("u80", "80 UE @ 0.2 s", "\\\\"))
    ):
        means, stds = [], []
        for embb in (5, 10):
            m = pick(rows, grid=grid, config_name="eredcap", embb_ues=embb)[0]
            means.append(f(m["embb_payload_pdr_mean"]))
            stds.append(f(m["embb_payload_pdr_std"]))
        offset = (-width / 2 if i == 0 else width / 2)
        ax.bar(
            x + offset,
            means,
            width,
            yerr=stds,
            label=label,
            color=["#FDAE6B", "#E6550D"][i],
            edgecolor="black",
            linewidth=0.4,
            hatch=hatch,
            capsize=2.5,
        )
        for xi, m in zip(x + offset, means):
            ax.text(xi, min(m + 2.0, 106), f"{m:.0f}", ha="center", va="bottom", fontsize=7)
    ax.set_xticks([5, 10], ["5 eMBB", "10 eMBB"])
    ax.set_ylim(0, 112)
    style_axes(ax, xlabel="eMBB background UEs", ylabel="eMBB payload PDR [%]")
    ax.set_title("eMBB PDR when sharing with eRedCap", pad=8)
    ax.axhline(100, color="0.5", lw=0.6, ls=":")
    ax.legend(frameon=True, fancybox=False, edgecolor="0.7", loc="lower center")
    out = FIG / "iotj_m2b_embb_under_eredcap.png"
    finish_figure(fig, out, hspace=None)


def fig_sensitivity() -> None:
    rows = load_csv(RESULTS / "ul_sens_sweep_agg.csv")
    fig, axes = plt.subplots(2, 1, figsize=(3.45, 4.85))
    for ax, iv, title, xlabel, loc in zip(
        axes,
        (0.1, 0.05),
        ("(a) Interval 0.10 s", "(b) Interval 0.05 s"),
        (None, "Data error model"),
        ("center left", "upper center"),
    ):
        cats = ["Error off", "Error on"]
        series = {}
        for cfg in CFGS:
            means, stds = [], []
            for err in (0, 1):
                m = pick(rows, config_name=cfg, interval_s=iv, error_model=err)[0]
                means.append(f(m["payload_pdr_mean"]))
                stds.append(f(m["payload_pdr_std"]))
            series[cfg] = (means, stds)
        grouped_bars(
            ax,
            cats,
            series,
            "Payload PDR [%]",
            title,
            xlabel=xlabel,
            legend_loc=loc,
        )
    fig.suptitle("Error-model sensitivity @ 40 UEs (5 seeds)", fontsize=9)
    out = FIG / "iotj_m3_sensitivity_pdr.png"
    finish_figure(fig, out)


def fig_saturation_summary() -> None:
    """One-glance saturation order graphic."""
    pdr = {"redcap-min": 37.2, "redcap-max": 94.6, "eredcap": 21.8}
    fig, ax = plt.subplots(figsize=(3.45, 2.55))
    ys = np.arange(len(CFGS))
    vals = [pdr[c] for c in CFGS]
    colors = [COLORS[c] for c in CFGS]
    bars = ax.barh(
        ys,
        vals,
        color=colors,
        edgecolor="black",
        height=0.55,
        hatch=[HATCH[c] for c in CFGS],
    )
    ax.set_yticks(ys, [LABELS[c] for c in CFGS])
    ax.set_xlim(0, 115)
    style_axes(ax, xlabel="Payload PDR [%]")
    ax.set_title("Saturation order @ 40 UE / 0.05 s", pad=8)
    for b, v in zip(bars, vals):
        ax.text(v + 1.5, b.get_y() + b.get_height() / 2, f"{v:.0f}%", va="center", fontsize=8)
    ax.axvline(100, color="0.5", lw=0.6, ls=":")
    ax.annotate(
        "eRedCap  →  RedCap-min  →  RedCap-max (most robust)",
        xy=(0.5, -0.28),
        xycoords="axes fraction",
        ha="center",
        va="top",
        fontsize=7,
        color="0.25",
    )
    out = FIG / "iotj_saturation_order.png"
    finish_figure(fig, out, hspace=None)


def main() -> int:
    fig_system_schematic()
    fig_access_load()
    fig_access_dense02()
    fig_control_plane_load()
    fig_coexist_stress()
    fig_coexist_embb_under_eredcap()
    fig_sensitivity()
    fig_saturation_summary()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
