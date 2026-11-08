#!/usr/bin/env python3
"""IoTJ R1 campaign runner.

Differences from scripts/runner_redcap.py:
  * every model flag is passed explicitly (no reliance on scenario defaults,
    e.g. true5MHz, which silently changed between code versions);
  * runs the built binary directly (no per-job `ns3 run` rebuild checks), so
    parallel workers cannot trigger concurrent builds;
  * records the git commit + dirty flag of the simulator tree in the manifest;
  * parses flow, access, resource, revision (soft-fail / scheduler / queue)
    counters and the per-second time series into one summary CSV.

Grid file (JSON): a list of "arms"; each arm is a dict of Job fields, where any
field may be a list (cartesian product is taken). Example:
  [{"name": "r1base", "red_cap_config": [1,2,3], "num_ues": 40,
    "interval_s": [0.2,0.1,0.05], "seeds": [1,2,3,4,5]}]

Usage:
  python3 -I scripts/r1/run_r1.py --grid grid.json --campaign r1_load --workers 6
"""

from __future__ import annotations

import argparse
import csv
import itertools
import json
import os
import queue
import subprocess
import sys
import threading
import time
from dataclasses import asdict, dataclass, fields
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NS3 = ROOT / "ns-3.38"
BIN = NS3 / "build" / "scratch" / "ns3.38-cni-playground-base-default"
RESULTS = ROOT / "results" / "r1"
NS3_RESULTS = NS3 / "Results" / "Cni-Szenario"

sys.path.insert(0, str(ROOT / "scripts"))
import parse_results as pr  # noqa: E402


@dataclass(frozen=True)
class Job:
    sim_tag: str
    seed: int
    red_cap_config: int
    num_ues: int
    interval_s: float
    sim_time: float = 36.0
    packet_size: int = 5000
    downlink: bool = False
    inactivity_timer_ms: int = 60000
    embb_ues: int = 0
    embb_packet_size: int = 1500
    embb_interval_s: float = 0.1
    error_model: bool = False
    enable_harq: bool = False
    true_5mhz: bool = False  # IoTJ = capability-style eRedCap
    strict_phy: bool = False
    drain_s: float = 0.0
    mcs_override: int = -1
    eredcap_power_offset_db: int = 6

    @property
    def config_name(self) -> str:
        return {1: "redcap-min", 2: "redcap-max", 3: "eredcap"}[self.red_cap_config]

    def argv(self) -> list[str]:
        b = lambda x: "true" if x else "false"  # noqa: E731
        return [
            str(BIN),
            f"--simTime={self.sim_time}",
            f"--enddevicesNumPergNb={self.num_ues}",
            "--gNbNum=1",
            f"--embbUes={self.embb_ues}",
            f"--embbPacketSize={self.embb_packet_size}",
            f"--embbIntervall={self.embb_interval_s}",
            f"--packetSize={self.packet_size}",
            f"--intervall={self.interval_s}",
            f"--redCapConfig={self.red_cap_config}",
            f"--downlink={b(self.downlink)}",
            f"--inactivityTimer={self.inactivity_timer_ms}",
            f"--simTag={self.sim_tag}",
            f"--seed={self.seed}",
            f"--DataErrorModelEnabled={b(self.error_model)}",
            f"--EnableHarqReTx={b(self.enable_harq)}",
            f"--true5MHz={b(self.true_5mhz)}",
            f"--strictPhy={b(self.strict_phy)}",
            f"--drainS={self.drain_s}",
            f"--mcsOverride={self.mcs_override}",
            f"--eredcapPowerOffsetDb={self.eredcap_power_offset_db}",
        ]


JOB_FIELDS = {f.name for f in fields(Job)} - {"sim_tag", "seed"}


def git_rev() -> tuple[str, bool]:
    rev = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"],
                         capture_output=True, text=True).stdout.strip()
    dirty = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain", "--", "ns-3.38"],
                           capture_output=True, text=True).stdout.strip() != ""
    return rev, dirty


def expand_grid(grid: list[dict], campaign: str) -> list[Job]:
    jobs = []
    for arm in grid:
        arm = dict(arm)
        name = arm.pop("name")
        seeds = arm.pop("seeds")
        keys = sorted(arm)
        unknown = set(keys) - JOB_FIELDS
        if unknown:
            raise SystemExit(f"arm {name}: unknown fields {unknown}")
        vals = [arm[k] if isinstance(arm[k], list) else [arm[k]] for k in keys]
        for combo in itertools.product(*vals):
            kw = dict(zip(keys, combo))
            for seed in seeds:
                parts = [campaign, name, f"c{kw['red_cap_config']}", f"n{kw['num_ues']}",
                         f"t{kw['interval_s']}".replace(".", "p")]
                for k in sorted(kw):
                    if k in ("red_cap_config", "num_ues", "interval_s"):
                        continue
                    v = kw[k]
                    v = int(v) if isinstance(v, bool) else v
                    parts.append(f"{k}{v}".replace(".", "p").replace("-", "m"))
                parts.append(f"s{seed}")
                jobs.append(Job(sim_tag="_".join(parts), seed=seed, **kw))
    tags = [j.sim_tag for j in jobs]
    if len(tags) != len(set(tags)):
        raise SystemExit("duplicate sim_tags in grid")
    return jobs


def run_job(job: Job, wid: int, skip_existing: bool) -> int:
    log_dir = RESULTS / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / f"{job.sim_tag}.log"
    rev_file = NS3_RESULTS / job.sim_tag / f"revisionStats_Seed{job.seed}.txt"
    if skip_existing and rev_file.exists():
        print(f"[w{wid}] SKIP {job.sim_tag}", flush=True)
        return 0
    print(f"[w{wid}] START {job.sim_tag}", flush=True)
    t0 = time.time()
    with log_path.open("w") as log:
        log.write("# argv: " + " ".join(job.argv()) + "\n")
        log.flush()
        proc = subprocess.run(job.argv(), cwd=NS3, stdout=log, stderr=subprocess.STDOUT)
    status = "OK" if proc.returncode == 0 else f"FAIL({proc.returncode})"
    print(f"[w{wid}] {status} {job.sim_tag} {time.time() - t0:.0f}s", flush=True)
    return proc.returncode


def parse_revision(path: Path) -> dict:
    out = {}
    if path and path.exists():
        for line in path.read_text().splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                if k.endswith("_us"):
                    continue
                try:
                    out["rev." + k] = int(v)
                except ValueError:
                    out["rev." + k] = v
    return out


def parse_timeseries(path: Path) -> dict:
    """Backlog trend: queued bytes at mid-traffic vs. end, and delivered rate in the
    second half of the traffic window (steady-state goodput)."""
    if not path.exists():
        return {}
    rows = list(csv.DictReader(path.open()))
    rows = [r for r in rows if r.get("t_s")]
    if len(rows) < 4:
        return {}
    t = [float(r["t_s"]) for r in rows]
    tx = [int(r["tx_bytes"]) for r in rows]
    rx = [int(r["rx_bytes"]) for r in rows]
    q = [int(r["ul_backlog_bytes"]) for r in rows]
    # traffic window = from first second with payload growth to last
    start = next((i for i in range(1, len(tx)) if tx[i] - tx[i - 1] > 10000), 1)
    end = len(tx) - 1
    mid = start + (end - start) // 2
    dt = t[end] - t[mid]
    return {
        "ts.traffic_start_s": t[start],
        "ts.backlog_mid_bytes": q[mid],
        "ts.backlog_end_bytes": q[end],
        "ts.offered_rate_2nd_half_mbps": (tx[end] - tx[mid]) * 8 / dt / 1e6 if dt > 0 else 0.0,
        "ts.goodput_2nd_half_mbps": (rx[end] - rx[mid]) * 8 / dt / 1e6 if dt > 0 else 0.0,
    }


def summarize(jobs: list[Job], out_csv: Path, rev: str, dirty: bool) -> None:
    rows = []
    for j in jobs:
        row = {"git_rev": rev, "git_dirty": int(dirty), **asdict(j), "config_name": j.config_name}
        ff = pr.find_flow_file(j.sim_tag, j.seed)
        row["ok"] = int(ff is not None)
        if ff:
            row.update(pr.parse_flows(ff))
        af = pr.find_access_file(j.sim_tag, j.seed)
        if af:
            row.update(pr.parse_access_stats(af))
        rf = pr.find_resource_file(j.sim_tag, j.seed)
        if rf:
            row.update(pr.parse_resources(rf))
        row.update(parse_revision(NS3_RESULTS / j.sim_tag / f"revisionStats_Seed{j.seed}.txt"))
        row.update(parse_timeseries(NS3_RESULTS / j.sim_tag / f"timeseries_Seed{j.seed}.csv"))
        rows.append(row)
    keys = []
    for r in rows:
        for k in r:
            if k not in keys:
                keys.append(k)
    with out_csv.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for r in rows:
            w.writerow(r)
    print(f"summary: {out_csv} ({sum(r['ok'] for r in rows)}/{len(rows)} ok)")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--grid", required=True)
    ap.add_argument("--campaign", required=True)
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--skip-existing", action="store_true")
    ap.add_argument("--summarize-only", action="store_true")
    ap.add_argument("--allow-dirty", action="store_true",
                    help="permit running from a simulator tree with uncommitted changes")
    a = ap.parse_args()

    if not BIN.exists():
        print(f"binary not found: {BIN}", file=sys.stderr)
        return 1
    rev, dirty = git_rev()
    if dirty and not a.allow_dirty and not a.summarize_only:
        print("simulator tree has uncommitted changes; commit first or pass --allow-dirty",
              file=sys.stderr)
        return 1
    jobs = expand_grid(json.loads(Path(a.grid).read_text()), a.campaign)
    RESULTS.mkdir(parents=True, exist_ok=True)
    manifest = RESULTS / f"{a.campaign}_manifest.csv"
    with manifest.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["git_rev", "git_dirty", *[x.name for x in fields(Job)]])
        w.writeheader()
        for j in jobs:
            w.writerow({"git_rev": rev, "git_dirty": int(dirty), **asdict(j)})
    print(f"{len(jobs)} jobs, rev {rev[:10]}{' (dirty)' if dirty else ''}, manifest {manifest}")
    if dirty and not a.summarize_only:
        # Record the exact simulator tree used, so dirty-tree results stay reproducible.
        diff = subprocess.run(["git", "-C", str(ROOT), "diff", "HEAD", "--", "ns-3.38"],
                              capture_output=True, text=True).stdout
        untracked = subprocess.run(
            ["git", "-C", str(ROOT), "ls-files", "--others", "--exclude-standard", "--", "ns-3.38"],
            capture_output=True, text=True).stdout.split()
        with (RESULTS / f"{a.campaign}_tree.patch").open("w") as f:
            f.write(diff)
            for u in untracked:
                f.write(f"\n# untracked: {u}\n")
                f.write((ROOT / u).read_text())

    if not a.summarize_only:
        q: queue.Queue = queue.Queue()
        fails: list[Job] = []

        def worker(wid: int) -> None:
            while True:
                j = q.get()
                if j is None:
                    q.task_done()
                    return
                if run_job(j, wid, a.skip_existing) != 0:
                    fails.append(j)
                q.task_done()

        ths = [threading.Thread(target=worker, args=(i,), daemon=True) for i in range(a.workers)]
        for t in ths:
            t.start()
        for j in jobs:
            q.put(j)
        for _ in ths:
            q.put(None)
        q.join()
        if fails:
            print(f"{len(fails)} failed runs:")
            for j in fails:
                print("  ", j.sim_tag)

    summarize(jobs, RESULTS / f"{a.campaign}_summary.csv", rev, dirty)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
