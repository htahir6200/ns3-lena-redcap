#!/usr/bin/env python3
"""Batch runner for ns3-lena-redcap (lena-nb style)."""

from __future__ import annotations

import argparse
import csv
import os
import queue
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NS3 = ROOT / "ns-3.38"
CMAKE_BIN = ROOT / "cmake" / "CMake.app" / "Contents" / "bin"
RESULTS = ROOT / "results" / "campaigns"


@dataclass(frozen=True)
class Job:
    sim_tag: str
    seed: int
    red_cap_config: int
    downlink: bool
    num_ues: int
    packet_size: int
    interval_s: float
    sim_time: float
    inactivity_timer_ms: int = 60000
    embb_ues: int = 0
    embb_packet_size: int = 1500
    embb_interval_s: float = 0.1
    error_model: bool = False
    enable_harq: bool = False
    # None = omit flag (ns-3 default true5MHz=true for config 3); else force.
    true_5mhz: bool | None = None
    gnb_num: int = 1
    gnb_isd: float = 500.0
    enable_edrx_ptw: bool = False
    ptw_length_ms: int = 1280
    enable_rai: bool = False
    rai_guard_ms: int = 20
    edrx_cycle_frames: int = 256

    @property
    def config_name(self) -> str:
        return {1: "redcap-min", 2: "redcap-max", 3: "eredcap"}[self.red_cap_config]

    def ns3_args(self) -> str:
        args = (
            f"cni-playground-base "
            f"--simTime={self.sim_time} "
            f"--enddevicesNumPergNb={self.num_ues} "
            f"--gNbNum={self.gnb_num} "
            f"--gNbIsd={self.gnb_isd} "
            f"--embbUes={self.embb_ues} "
            f"--embbPacketSize={self.embb_packet_size} "
            f"--embbIntervall={self.embb_interval_s} "
            f"--packetSize={self.packet_size} "
            f"--intervall={self.interval_s} "
            f"--redCapConfig={self.red_cap_config} "
            f"--downlink={'true' if self.downlink else 'false'} "
            f"--inactivityTimer={self.inactivity_timer_ms} "
            f"--simTag={self.sim_tag} "
            f"--seed={self.seed} "
            f"--DataErrorModelEnabled={'true' if self.error_model else 'false'} "
            f"--EnableHarqReTx={'true' if self.enable_harq else 'false'} "
            f"--enableEdrxPtw={'true' if self.enable_edrx_ptw else 'false'} "
            f"--ptwLengthMs={self.ptw_length_ms} "
            f"--enableRai={'true' if self.enable_rai else 'false'} "
            f"--raiGuardMs={self.rai_guard_ms} "
            f"--edrxCycleFrames={self.edrx_cycle_frames}"
        )
        if self.true_5mhz is not None:
            args += f" --true5MHz={'true' if self.true_5mhz else 'false'}"
        return args


def job_already_done(job: Job) -> bool:
    """Skip if accessStats or flowOutput already exists for this seed."""
    base = NS3 / "Results" / "Cni-Szenario" / job.sim_tag / "Ausgaben"
    if not base.exists():
        return False
    if list(base.rglob(f"accessStats_Seed{job.seed}.txt")):
        return True
    if list(base.rglob(f"flowOutput_Seed{job.seed}.txt")):
        return True
    return False


def run_job(job: Job, worker_id: int, skip_existing: bool = False) -> int:
    env = dict(os.environ)
    env["PATH"] = f"{CMAKE_BIN}:{env.get('PATH', '')}"
    log_dir = RESULTS / job.sim_tag
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / f"seed{job.seed}_w{worker_id}.log"
    if skip_existing and job_already_done(job):
        print(f"[worker {worker_id}] SKIP {job.sim_tag} seed={job.seed}", flush=True)
        with log_path.open("a") as log:
            log.write(f"# skipped existing results {time.strftime('%F %T')}\n")
        return 0
    cmd = [sys.executable, str(NS3 / "ns3"), "run", job.ns3_args()]
    print(f"[worker {worker_id}] START {job.sim_tag} seed={job.seed}", flush=True)
    with log_path.open("w") as log:
        log.write(f"# cmd: {' '.join(cmd)}\n")
        proc = subprocess.run(
            cmd,
            cwd=NS3,
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=False,
        )
    status = "OK" if proc.returncode == 0 else f"FAIL({proc.returncode})"
    print(f"[worker {worker_id}] {status} {job.sim_tag} seed={job.seed} -> {log_path}", flush=True)
    return proc.returncode


def worker_loop(q: queue.Queue, worker_id: int, failures: list, skip_existing: bool) -> None:
    while True:
        job = q.get()
        if job is None:
            q.task_done()
            break
        code = run_job(job, worker_id, skip_existing=skip_existing)
        if code != 0:
            failures.append(job)
        q.task_done()


def build_jobs(args: argparse.Namespace) -> list[Job]:
    jobs: list[Job] = []
    for seed in range(1, args.seeds + 1):
        for cfg in args.configs:
            direction = "dl" if args.downlink else "ul"
            embb_tag = f"_embb{args.embb_ues}" if args.embb_ues else ""
            err_tag = f"_err{int(args.error_model)}"
            harq_tag = f"_harq{int(args.enable_harq)}"
            t5 = getattr(args, "true_5mhz", None)
            t5_tag = ""
            true_5mhz_val = None
            if t5 == "true":
                true_5mhz_val = True
                t5_tag = "_t5"
            elif t5 == "false":
                true_5mhz_val = False
                t5_tag = "_cap"
            gnb_num = int(getattr(args, "gnb_num", 1))
            gnb_isd = float(getattr(args, "gnb_isd", 500.0))
            cell_tag = f"_g{gnb_num}" if gnb_num != 1 else ""
            enable_ptw = bool(getattr(args, "enable_edrx_ptw", False))
            enable_rai = bool(getattr(args, "enable_rai", False))
            ptw_ms = int(getattr(args, "ptw_length_ms", 1280))
            rai_ms = int(getattr(args, "rai_guard_ms", 20))
            edrx_fr = int(getattr(args, "edrx_cycle_frames", 256))
            # Only tag when non-default so Phase A/B sim_tags stay unchanged.
            ptw_tag = f"_ptw{ptw_ms}" if enable_ptw else ""
            rai_tag = "_rai1" if enable_rai else ""
            edrx_tag = f"_edrx{edrx_fr}" if edrx_fr != 256 else ""
            tag = (
                f"{args.campaign}_{direction}_c{cfg}_ues{args.ues}"
                f"{embb_tag}_ps{args.packet_size}{err_tag}{harq_tag}{t5_tag}"
                f"{ptw_tag}{rai_tag}{edrx_tag}{cell_tag}_s{seed}"
            )
            jobs.append(
                Job(
                    sim_tag=tag,
                    seed=seed,
                    red_cap_config=cfg,
                    downlink=args.downlink,
                    num_ues=args.ues,
                    packet_size=args.packet_size,
                    interval_s=args.interval,
                    sim_time=args.sim_time,
                    inactivity_timer_ms=args.inactivity_timer,
                    embb_ues=args.embb_ues,
                    embb_packet_size=args.embb_packet_size,
                    embb_interval_s=args.embb_interval,
                    error_model=args.error_model,
                    enable_harq=args.enable_harq,
                    true_5mhz=true_5mhz_val,
                    gnb_num=gnb_num,
                    gnb_isd=gnb_isd,
                    enable_edrx_ptw=enable_ptw,
                    ptw_length_ms=ptw_ms,
                    enable_rai=enable_rai,
                    rai_guard_ms=rai_ms,
                    edrx_cycle_frames=edrx_fr,
                )
            )
    return jobs


def write_manifest(jobs: list[Job], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(
            f,
            fieldnames=[
                "sim_tag",
                "seed",
                "red_cap_config",
                "config_name",
                "downlink",
                "num_ues",
                "embb_ues",
                "packet_size",
                "embb_packet_size",
                "interval_s",
                "embb_interval_s",
                "sim_time",
                "inactivity_timer_ms",
                "error_model",
                "enable_harq",
                "true_5mhz",
                "gnb_num",
                "gnb_isd",
            ],
        )
        w.writeheader()
        for j in jobs:
            w.writerow(
                {
                    "sim_tag": j.sim_tag,
                    "seed": j.seed,
                    "red_cap_config": j.red_cap_config,
                    "config_name": j.config_name,
                    "downlink": j.downlink,
                    "num_ues": j.num_ues,
                    "embb_ues": j.embb_ues,
                    "packet_size": j.packet_size,
                    "embb_packet_size": j.embb_packet_size,
                    "interval_s": j.interval_s,
                    "embb_interval_s": j.embb_interval_s,
                    "sim_time": j.sim_time,
                    "inactivity_timer_ms": j.inactivity_timer_ms,
                    "error_model": j.error_model,
                    "enable_harq": j.enable_harq,
                    "true_5mhz": "" if j.true_5mhz is None else int(j.true_5mhz),
                    "gnb_num": j.gnb_num,
                    "gnb_isd": j.gnb_isd,
                }
            )


def main() -> int:
    p = argparse.ArgumentParser(description="Run RedCap/eRedCap comparison campaigns")
    p.add_argument("--campaign", default="cmp", help="Campaign name prefix")
    p.add_argument("--configs", nargs="+", type=int, default=[1, 2, 3], choices=[1, 2, 3])
    p.add_argument("--seeds", type=int, default=1)
    p.add_argument("--ues", type=int, default=5)
    p.add_argument("--embb-ues", type=int, default=0, help="eMBB UEs sharing the cell")
    p.add_argument("--embb-packet-size", type=int, default=1500)
    p.add_argument("--embb-interval", type=float, default=0.1)
    p.add_argument("--packet-size", type=int, default=5000)
    p.add_argument("--interval", type=float, default=5.0)
    p.add_argument("--sim-time", type=float, default=36.0)
    p.add_argument(
        "--inactivity-timer",
        type=int,
        default=60000,
        help="RRC data inactivity timer in ms (1000 breaks DL traffic; use 60000)",
    )
    p.add_argument("--downlink", action="store_true", help="DL-centered (default: UL)")
    p.add_argument(
        "--error-model",
        action="store_true",
        help="Enable NrSpectrumPhy DataErrorModelEnabled (default: off)",
    )
    p.add_argument(
        "--enable-harq",
        action="store_true",
        help="Enable scheduler HARQ retransmissions (default: off; pair with --error-model)",
    )
    p.add_argument(
        "--true-5mhz",
        choices=["true", "false"],
        default=None,
        help="For config 3: true=5 MHz BWP; false=capability-style (IoTJ). Omit = ns-3 default.",
    )
    p.add_argument("--gnb-num", type=int, default=1, help="Number of gNBs (static multi-cell)")
    p.add_argument("--gnb-isd", type=float, default=500.0, help="Inter-site distance in meters")
    p.add_argument(
        "--enable-edrx-ptw",
        action="store_true",
        help="Phase C: enable eDRX PTW duty-cycle energy billing",
    )
    p.add_argument("--ptw-length-ms", type=int, default=1280, help="Phase C PTW length (ms)")
    p.add_argument(
        "--enable-rai",
        action="store_true",
        help="Phase C research AS RAI: early release after last UL",
    )
    p.add_argument("--rai-guard-ms", type=int, default=20, help="Phase C RAI guard (ms)")
    p.add_argument(
        "--edrx-cycle-frames",
        type=int,
        default=256,
        choices=[256, 512, 1024, 2048],
        help="RAN eDRX cycle in radio frames (256→2.56s … 2048→20.48s)",
    )
    p.add_argument("--workers", type=int, default=1)
    p.add_argument(
        "--skip-existing",
        action="store_true",
        help="Skip jobs that already have accessStats/flowOutput for this seed",
    )
    args = p.parse_args()

    if not (NS3 / "ns3").exists():
        print(f"ns-3 tree not found at {NS3}", file=sys.stderr)
        return 1

    jobs = build_jobs(args)
    RESULTS.mkdir(parents=True, exist_ok=True)
    manifest = RESULTS / f"{args.campaign}_manifest.csv"
    write_manifest(jobs, manifest)
    print(f"Queued {len(jobs)} jobs. Manifest: {manifest}")

    q: queue.Queue = queue.Queue()
    failures: list[Job] = []
    threads = []
    for i in range(max(1, args.workers)):
        t = threading.Thread(
            target=worker_loop,
            args=(q, i, failures, args.skip_existing),
            daemon=True,
        )
        t.start()
        threads.append(t)

    t0 = time.time()
    for job in jobs:
        q.put(job)
    for _ in threads:
        q.put(None)
    q.join()
    for t in threads:
        t.join()

    elapsed = time.time() - t0
    print(f"Finished in {elapsed:.1f}s. Failures: {len(failures)}")
    if failures:
        for j in failures:
            print(f"  FAIL {j.sim_tag}")
        return 2

    # Auto-parse summary
    parse = ROOT / "scripts" / "parse_results.py"
    if parse.exists():
        subprocess.run(
            [sys.executable, str(parse), "--manifest", str(manifest)],
            check=False,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
