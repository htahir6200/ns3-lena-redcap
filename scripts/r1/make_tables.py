#!/usr/bin/env python3
"""Generate LaTeX tables for the IoTJ R1 manuscript from results/r1/f_*_agg.csv.

  python3 -I scripts/r1/make_tables.py --out paper/iotj/revision_r1/tables

Every number in the revised Results section should come from these files.
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
R1 = ROOT / "results" / "r1"
CLS = {"1": "RedCap-min", "2": "RedCap-max", "3": "eRedCap"}


def load(name: str) -> list[dict]:
    return list(csv.DictReader((R1 / f"{name}_agg.csv").open()))


def v(r: dict, k: str) -> float:
    try:
        return float(r[f"{k}_mean"])
    except (KeyError, ValueError):
        return float("nan")


def s(r: dict, k: str) -> float:
    try:
        return float(r[f"{k}_std"])
    except (KeyError, ValueError):
        return float("nan")


def ci(r: dict, k: str) -> float:
    try:
        return float(r[f"{k}_ci95"])
    except (KeyError, ValueError):
        return float("nan")


def pm(r: dict, k: str, d: int = 1) -> str:
    m, e = v(r, k), s(r, k)
    if math.isnan(m):
        return "---"
    if math.isnan(e) or round(e, d) == 0:
        return f"${m:.{d}f}$"
    return f"${m:.{d}f}\\pm{e:.{d}f}$"


def num(x: float, d: int = 1) -> str:
    return "---" if math.isnan(x) else f"${x:.{d}f}$"


def pick(rows, **kw):
    out = []
    for r in rows:
        if all(str(r.get(k)) == str(val) for k, val in kw.items()):
            out.append(r)
    return out


def one(rows, **kw):
    m = pick(rows, **kw)
    if len(m) != 1:
        raise SystemExit(f"expected 1 row for {kw}, got {len(m)}")
    return m[0]


def write(out: Path, name: str, body: str) -> None:
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{name}.tex").write_text(body)
    print(f"wrote {out / name}.tex")


def t_load(out: Path) -> None:
    base = load("f_base")
    Ts = ["0.2", "0.1", "0.05", "0.04", "0.03", "0.025"]
    lines = [
        r"\begin{tabular}{@{}l" + "c" * len(Ts) + "@{}}",
        r"\toprule",
        " & ".join(["Class"] + [f"{t}\\,s" for t in Ts]) + r" \\",
        " & ".join(["Offered [Mb/s]"] + [f"${40 * 5000 * 8 / float(t) / 1e6:.0f}$" for t in Ts]) + r" \\",
        r"\midrule",
    ]
    for c in ("1", "2", "3"):
        cells = []
        for t in Ts:
            r = one(base, red_cap_config=c, num_ues="40", interval_s=t)
            cells.append(pm(r, "pdr_at_stop"))
        lines.append(" & ".join([CLS[c]] + cells) + r" \\")
    lines += [r"\midrule", r"\multicolumn{" + str(len(Ts) + 1) + r"}{@{}l}{\emph{After 10\,s drain}} \\"]
    for c in ("1", "2", "3"):
        cells = [pm(one(base, red_cap_config=c, num_ues="40", interval_s=t), "pdr_after_drain") for t in Ts]
        lines.append(" & ".join([CLS[c]] + cells) + r" \\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    write(out, "tab_load", "\n".join(lines) + "\n")


def t_dense(out: Path) -> None:
    base = load("f_base")
    Ns = ["40", "80", "160", "240", "320"]
    lines = [
        r"\begin{tabular}{@{}l" + "c" * len(Ns) + "@{}}",
        r"\toprule",
        " & ".join(["Class"] + [f"{n}~UE" for n in Ns]) + r" \\",
        " & ".join(["Offered [Mb/s]"] + [f"${int(n) * 5000 * 8 / 0.2 / 1e6:.0f}$" for n in Ns]) + r" \\",
        r"\midrule",
    ]
    lines.append(r"\multicolumn{" + str(len(Ns) + 1) + r"}{@{}l}{\emph{PDR at traffic stop [\%]}} \\")
    for c in ("1", "2", "3"):
        cells = [pm(one(base, red_cap_config=c, num_ues=n, interval_s="0.2"), "pdr_at_stop") for n in Ns]
        lines.append(" & ".join([CLS[c]] + cells) + r" \\")
    lines += [r"\midrule", r"\multicolumn{" + str(len(Ns) + 1) + r"}{@{}l}{\emph{Mean delay of delivered packets [ms]}} \\"]
    for c in ("1", "2", "3"):
        cells = [num(v(one(base, red_cap_config=c, num_ues=n, interval_s="0.2"), "delay_ms"), 0) for n in Ns]
        lines.append(" & ".join([CLS[c]] + cells) + r" \\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    write(out, "tab_dense", "\n".join(lines) + "\n")


def t_bottleneck(out: Path) -> None:
    """Bottleneck counters at representative saturation points (AE item 1)."""
    base = load("f_base")
    pts = [("1", "40", "0.03"), ("2", "40", "0.025"), ("3", "40", "0.03"),
           ("1", "320", "0.2"), ("2", "320", "0.2"), ("3", "240", "0.2")]
    cols = [("goodput_2nd_half_mbps", "Goodput [Mb/s]", 1),
            ("ul_rb_util_pct", "UL RB util. [\\%]", 1),
            ("coreset_util_pct", "CORESET util. [\\%]", 1),
            ("ul_blocked_pdcch_pct", "UL att.\\ blocked by PDCCH [\\%]", 1),
            ("sr_unanswered_pct", "SR unanswered [\\%]", 1),
            ("backlog_at_stop_MB", "Backlog at stop [MB]", 1),
            ("softfails", "PHY/sched.\\ soft-fails", 0)]
    lines = [r"\begin{tabular}{@{}l" + "c" * len(pts) + "@{}}", r"\toprule",
             " & ".join(["Metric"] + [f"{CLS[c]}\\\\{n}@{t}\\,s" for c, n, t in pts]).replace("\\\\", "~") + r" \\",
             r"\midrule"]
    rows = {p: one(base, red_cap_config=p[0], num_ues=p[1], interval_s=p[2]) for p in pts}
    for k, label, d in cols:
        cells = []
        for p in pts:
            x = v(rows[p], k)
            cells.append(num(0.0 if math.isnan(x) else x, d))
        lines.append(" & ".join([label] + cells) + r" \\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    write(out, "tab_bottleneck", "\n".join(lines) + "\n")


def t_mcs(out: Path) -> None:
    base, mcs = load("f_base"), load("f_mcs")
    pts = [("40", "0.03"), ("40", "0.025"), ("320", "0.2")]
    lines = [r"\begin{tabular}{@{}llccc@{}}", r"\toprule",
             r"Class & MCS & 40@0.03\,s & 40@0.025\,s & 320@0.2\,s \\",
             r"\midrule"]

    def g(rows, c, n, t, **kw):
        r = one(rows, red_cap_config=c, num_ues=n, interval_s=t, **kw)
        return f"{num(v(r, 'goodput_2nd_half_mbps'))}"

    lines.append(" & ".join(["RedCap-min", "19 (native)"] + [g(base, "1", n, t) for n, t in pts]) + r" \\")
    lines.append(" & ".join(["RedCap-min", "27"] + [g(mcs, "1", n, t, mcs_override="27") for n, t in pts]) + r" \\")
    lines.append(" & ".join(["RedCap-max", "27 (native)"] + [g(base, "2", n, t) for n, t in pts]) + r" \\")
    lines.append(" & ".join(["RedCap-max", "19"] + [g(mcs, "2", n, t, mcs_override="19") for n, t in pts]) + r" \\")
    lines.append(" & ".join(["eRedCap", "19 (native)"] + [g(base, "3", n, t) for n, t in pts]) + r" \\")
    lines.append(" & ".join(["eRedCap", "27"] + [g(mcs, "3", n, t, mcs_override="27") for n, t in pts]) + r" \\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    write(out, "tab_mcs", "\n".join(lines) + "\n")


def t_coex(out: Path) -> None:
    coex = load("f_coex")
    grids = [("40", "0.03"), ("240", "0.2")]
    lines = [r"\begin{tabular}{@{}llccc@{}}", r"\toprule",
             r"Grid & eMBB & RedCap-min & RedCap-max & eRedCap \\", r"\midrule"]
    for n, t in grids:
        for e in ("0", "5", "10"):
            cells = []
            for c in ("1", "2", "3"):
                r = one(coex, red_cap_config=c, num_ues=n, interval_s=t, embb_ues=e)
                rc = v(r, "pdr_at_stop")
                eb = v(r, "embb_pdr_after_drain")
                ed = v(r, "embb_delay_ms") / 1000.0
                if e == "0":
                    cells.append(f"{num(rc)}~/~---~/~---")
                else:
                    cells.append(f"{num(rc)}~/~{num(eb, 0)}~/~{num(ed, 2)}")
            lines.append(" & ".join([f"{n}@{t}\\,s", e] + cells) + r" \\")
        lines.append(r"\midrule" if (n, t) != grids[-1] else "")
    lines += [r"\bottomrule", r"\end{tabular}"]
    write(out, "tab_coex", "\n".join(l for l in lines if l) + "\n")


def t_onefactor(out: Path) -> None:
    """One-factor-at-a-time ladder from RedCap-min (Reviewer 2, comment 1), paired seeds 1-5."""
    import statistics as st
    rows = []
    for f in ("f_base", "f_mcs"):
        rows += list(csv.DictReader((R1 / f"{f}_summary.csv").open()))

    def sel(cfg, mcs="-1", pwr="6"):
        return [r for r in rows if r["red_cap_config"] == cfg and r["mcs_override"] == mcs
                and r["eredcap_power_offset_db"] == pwr and r["num_ues"] == "40" and int(r["seed"]) <= 5]

    def gp(rs, t):
        x = [float(r["ts.goodput_2nd_half_mbps"]) for r in rs if r["interval_s"] == t]
        return st.mean(x) if x else float("nan")

    def blk(rs, t):
        x = []
        for r in rs:
            if r["interval_s"] != t:
                continue
            c = float(r.get("rev.sched.ul.ue_slot_candidates") or 0)
            x.append(100 * float(r.get("rev.sched.ul.not_scheduled.pdcch") or 0) / c if c else 0.0)
        return st.mean(x) if x else float("nan")

    ladder = [
        ("RedCap-min (reference)", "---", sel("1")),
        ("+ $2{\\times}2$ array", "antenna", sel("2", "19")),
        ("+ MCS 27", "MCS", sel("1", "27")),
        ("+ array + MCS 27 (RedCap-max)", "antenna, MCS", sel("2")),
        ("+ 12-PRB grant cap", "grant cap", sel("3", "-1", "0")),
        ("+ cap + $-6$\\,dB (eRedCap)", "grant cap, power", sel("3")),
        ("+ cap + $-6$\\,dB + MCS 27", "grant cap, power, MCS", sel("3", "27")),
    ]
    lines = [r"\begin{tabularx}{\columnwidth}{@{}>{\raggedright\arraybackslash}Xccc@{}}", r"\toprule",
             r"Change from RedCap-min & \multicolumn{2}{c}{Goodput [Mb/s]} & Blocked [\%] \\",
             r" & 53\,Mb/s & 64\,Mb/s & 64\,Mb/s \\", r"\midrule"]
    for label, _, rs in ladder:
        if not rs:
            raise SystemExit(f"no rows for {label}")
        lines.append(f"{label} & {num(gp(rs, '0.03'))} & {num(gp(rs, '0.025'))} & {num(blk(rs, '0.025'))} \\\\")
    lines += [r"\bottomrule", r"\end{tabularx}"]
    write(out, "tab_onefactor", "\n".join(lines) + "\n")


def t_mcssweep(out: Path) -> None:
    """Goodput vs MCS at 40 UEs / 0.025 s (64 Mb/s offered); daggers mark stalled runs."""
    rows = load("f_mcssweep")
    mcss = ["5", "10", "15", "19", "23", "27"]

    def cell(c, m, k, d=1):
        r = one(rows, red_cap_config=c, mcs_override=m)
        x = v(r, k)
        dag = "$^{\\dagger}$" if v(r, "stalled") > 0 else ""
        return f"{num(0.0 if math.isnan(x) else x, d)}{dag}"

    lines = [r"\begin{tabular}{@{}l" + "c" * len(mcss) + "@{}}", r"\toprule",
             " & ".join(["MCS"] + mcss) + r" \\", r"\midrule",
             " & ".join(["RedCap-min goodput [Mb/s]"] + [cell("1", m, "goodput_2nd_half_mbps") for m in mcss]) + r" \\",
             " & ".join(["RedCap-max goodput [Mb/s]"] + [cell("2", m, "goodput_2nd_half_mbps") for m in mcss]) + r" \\",
             " & ".join(["eRedCap goodput [Mb/s]"] + [cell("3", m, "goodput_2nd_half_mbps") for m in mcss]) + r" \\",
             r"\midrule",
             " & ".join(["RedCap-min UL PRB util. [\\%]"] + [cell("1", m, "ul_rb_util_pct") for m in mcss]) + r" \\",
             " & ".join(["eRedCap PDCCH-blocked [\\%]"] + [cell("3", m, "ul_blocked_pdcch_pct") for m in mcss]) + r" \\",
             " & ".join(["eRedCap CORESET util. [\\%]"] + [cell("3", m, "coreset_util_pct") for m in mcss]) + r" \\",
             r"\bottomrule", r"\end{tabular}"]
    write(out, "tab_mcssweep", "\n".join(lines) + "\n")


def t_sens(out: Path) -> None:
    """Error model / HARQ sensitivity at 40 UEs, paired seeds (original Table IX layout)."""
    base, err = load("f_base"), load("f_err")
    lines = [r"\begin{tabular}{@{}lcccccc@{}}", r"\toprule",
             r" & \multicolumn{3}{c}{0.05\,s (32\,Mb/s)} & \multicolumn{3}{c}{0.03\,s (53\,Mb/s)} \\",
             r"\cmidrule(lr){2-4}\cmidrule(lr){5-7}",
             r"Class & off & on & on+HARQ & off & on & on+HARQ \\", r"\midrule"]
    for c in ("1", "2", "3"):
        cells = []
        for t in ("0.05", "0.03"):
            cells.append(pm(one(base, red_cap_config=c, num_ues="40", interval_s=t), "pdr_at_stop"))
            cells.append(pm(one(err, red_cap_config=c, num_ues="40", interval_s=t, error_model="True", enable_harq="False"), "pdr_at_stop"))
            cells.append(pm(one(err, red_cap_config=c, num_ues="40", interval_s=t, error_model="True", enable_harq="True"), "pdr_at_stop"))
        lines.append(" & ".join([CLS[c]] + cells) + r" \\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    write(out, "tab_sens", "\n".join(lines) + "\n")


def t_traffic(out: Path) -> None:
    """Traffic-model sensitivity (Reviewer 2, comment 7)."""
    base, tr = load("f_base"), load("f_traffic")
    models = [("Periodic", None), ("Poisson", "0.0"), ("On/off 1/1\\,s", "1.0"), ("On/off 2/6\\,s", "2.0")]
    pts = [("40", "0.05", "pdr_at_stop", 1), ("40", "0.03", "pdr_at_stop", 1), ("240", "0.2", "pdr_at_stop", 1),
           ("40", "0.03", "goodput_2nd_half_mbps", 1), ("40", "0.04", "delay_ms", 0)]

    def row(c, on, n, t):
        if on is None:
            return one(base, red_cap_config=c, num_ues=n, interval_s=t)
        return one(tr, red_cap_config=c, num_ues=n, interval_s=t, on_mean_s=on)

    lines = [r"\begin{tabular}{@{}llccccc@{}}", r"\toprule",
             r" & & \multicolumn{3}{c}{PDR at stop [\%]} & Goodput [Mb/s] & Delay [ms] \\",
             r"\cmidrule(lr){3-5}",
             r"Class & Traffic & 40@0.05 & 40@0.03 & 240@0.2 & 40@0.03 & 40@0.04 \\", r"\midrule"]
    for i, c in enumerate(("1", "2", "3")):
        for j, (lab, on) in enumerate(models):
            cells = [num(v(row(c, on, n, t), k), d) for n, t, k, d in pts]
            lines.append(" & ".join([CLS[c] if j == 0 else "", lab] + cells) + r" \\")
        if i < 2:
            lines.append(r"\midrule")
    lines += [r"\bottomrule", r"\end{tabular}"]
    write(out, "tab_traffic", "\n".join(lines) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "paper" / "iotj" / "revision_r1" / "tables"))
    a = ap.parse_args()
    out = Path(a.out)
    for fn in (t_load, t_dense, t_bottleneck, t_mcs, t_coex, t_onefactor, t_mcssweep, t_sens, t_traffic):
        fn(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
