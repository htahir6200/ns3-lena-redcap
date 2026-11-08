#!/usr/bin/env python3
"""Re-run only failed energy E2 seeds (after stale-RNTI map::at fix)."""

from __future__ import annotations

import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "campaigns"
RUNNER = ROOT / "scripts" / "runner_redcap.py"
PARSE = ROOT / "scripts" / "parse_results.py"
CONSOLE = RESULTS / "energy_e2_console.log"

# sim_tag pattern from runner:
# ul_en_e2_load_i0p2_cap_dit1000_ul_c3_ues40_ps5000_err0_harq0_cap_s1
TAG_RE = re.compile(
    r"^(?P<campaign>.+)_ul_c(?P<cfg>\d+)_ues(?P<ues>\d+)_ps(?P<ps>\d+)"
    r"_err(?P<err>\d+)_harq(?P<harq>\d+)"
    r"(?:_t5|_cap)?"
    r"(?:_g(?P<gnb>\d+))?"
    r"_s(?P<seed>\d+)$"
)


def failed_tags() -> list[str]:
    text = CONSOLE.read_text(errors="ignore")
    tags = sorted({m.group(1) for m in re.finditer(r"FAIL (\S+)", text)})
    return tags


def parse_tag(tag: str) -> dict | None:
    m = TAG_RE.match(tag)
    if not m:
        return None
    d = m.groupdict()
    campaign = d["campaign"]
    true5 = None
    if "_t5_" in campaign or campaign.endswith("_t5"):
        true5 = True
    elif "_cap_" in campaign or campaign.endswith("_cap"):
        true5 = False
    # dit from campaign name
    dit = 1000
    dm = re.search(r"dit(\d+)", campaign)
    if dm:
        dit = int(dm.group(1))
    # interval from campaign
    iv = 0.2
    im = re.search(r"_i(\d+p?\d*)_", campaign)
    if im:
        iv = float(im.group(1).replace("p", "."))
    return {
        "campaign": campaign,
        "config": int(d["cfg"]),
        "ues": int(d["ues"]),
        "seed": int(d["seed"]),
        "interval": iv,
        "dit": dit,
        "true5": true5,
    }


def main() -> int:
    workers = int(os.environ.get("WORKERS", "4"))
    tags = failed_tags()
    if not tags:
        print("No FAIL lines in energy_e2_console.log")
        return 0

    status = RESULTS / "energy_e2_rerun_status.txt"
    status.write_text(f"started {time.strftime('%F %T')} n_fail={len(tags)}\n")
    print(f"Re-running {len(tags)} failed tags with {workers} workers", flush=True)

    # Group by campaign settings so runner gets one campaign + one seed list
    groups: dict[tuple, list[int]] = {}
    meta: dict[tuple, dict] = {}
    for tag in tags:
        info = parse_tag(tag)
        if not info:
            print("SKIP unparsed", tag)
            continue
        key = (
            info["campaign"],
            info["config"],
            info["ues"],
            info["interval"],
            info["dit"],
            info["true5"],
        )
        groups.setdefault(key, []).append(info["seed"])
        meta[key] = info

    rc_all = 0
    for key, seeds in sorted(groups.items()):
        info = meta[key]
        # Remove incomplete result dirs so skip-existing does not skip ghosts
        for seed in seeds:
            for p in RESULTS.glob(f"{info['campaign']}_ul_c{info['config']}_ues{info['ues']}_*_s{seed}"):
                # keep logs; only force re-run by deleting access/flow markers in ns-3 Results
                pass
            # Delete access/flow markers under ns-3 Results if present for this seed
            base = (
                ROOT
                / "ns-3.38"
                / "Results"
                / "Cni-Szenario"
            )
            for marker in base.glob(
                f"{info['campaign']}_ul_c{info['config']}_ues{info['ues']}_*_s{seed}/Ausgaben/**/accessStats_Seed{seed}.txt"
            ):
                marker.unlink(missing_ok=True)
            for marker in base.glob(
                f"{info['campaign']}_ul_c{info['config']}_ues{info['ues']}_*_s{seed}/Ausgaben/**/flowOutput_Seed{seed}.txt"
            ):
                marker.unlink(missing_ok=True)

        # runner --seeds is a count (1..N), not a list. Re-run up to max
        # failed seed and skip seeds that already finished cleanly.
        seed_count = max(sorted(set(seeds)))
        cmd = [
            sys.executable,
            str(RUNNER),
            "--campaign",
            info["campaign"],
            "--configs",
            str(info["config"]),
            "--seeds",
            str(seed_count),
            "--ues",
            str(info["ues"]),
            "--packet-size",
            "5000",
            "--interval",
            str(info["interval"]),
            "--sim-time",
            "36",
            "--inactivity-timer",
            str(info["dit"]),
            "--workers",
            str(workers),
            "--skip-existing",
        ]
        if info["true5"] is True:
            cmd.extend(["--true-5mhz", "true"])
        elif info["true5"] is False:
            cmd.extend(["--true-5mhz", "false"])
        print(f"\n=== {time.strftime('%F %T')} {' '.join(cmd[2:])}", flush=True)
        print(f"    (failed seeds were {sorted(set(seeds))})", flush=True)
        rc = subprocess.call(cmd, cwd=ROOT)
        status.write_text(
            status.read_text()
            + f"{info['campaign']} c{info['config']} failed_seeds={sorted(set(seeds))} rc={rc}\n"
        )
        if rc != 0:
            rc_all = rc

    # Refresh sweep parse
    manifests = sorted(RESULTS.glob("ul_en_e2_*_manifest.csv"))
    # re-parse each campaign summary that exists
    for m in manifests:
        if "sweep" in m.name:
            continue
        out = RESULTS / m.name.replace("_manifest.csv", "_summary.csv")
        subprocess.call(
            [sys.executable, str(PARSE), "--manifest", str(m), "--out", str(out)],
            cwd=ROOT,
        )

    subprocess.call(
        [
            sys.executable,
            str(PARSE),
            "--manifest",
            str(RESULTS / "ul_en_e2_sweep_manifest.csv"),
            "--out",
            str(RESULTS / "ul_en_e2_sweep_summary.csv"),
        ],
        cwd=ROOT,
    ) if (RESULTS / "ul_en_e2_sweep_manifest.csv").exists() else None

    status.write_text(status.read_text() + f"finished rc={rc_all} {time.strftime('%F %T')}\n")
    print("Rerun finished", flush=True)
    return rc_all


if __name__ == "__main__":
    raise SystemExit(main())
