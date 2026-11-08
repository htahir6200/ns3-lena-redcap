#!/usr/bin/env python3
"""Split FlowMonitor flows into RedCap vs eMBB using _ports_Seed*.txt tags."""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NS3_RESULTS = ROOT / "ns-3.38" / "Results" / "Cni-Szenario"
OUT_DIR = ROOT / "results" / "campaigns"

FLOW_RE = re.compile(
    r"^(?P<tx_pkts>\d+):(?P<rx_pkts>\d+):(?P<delay>[\d.]+):(?P<jitter>[\d.]+):(?P<tx_bytes>\d+):(?P<rx_bytes>\d+)$"
)


def find_ports(sim_tag: str, seed: int) -> Path | None:
    base = NS3_RESULTS / sim_tag / "Ausgaben"
    if not base.exists():
        return None
    matches = list(base.rglob(f"_ports_Seed{seed}.txt"))
    return matches[0] if matches else None


def find_flow(sim_tag: str, seed: int) -> Path | None:
    base = NS3_RESULTS / sim_tag / "Ausgaben"
    if not base.exists():
        return None
    matches = list(base.rglob(f"flowOutput_Seed{seed}.txt"))
    return matches[0] if matches else None


def load_port_classes(ports_path: Path) -> dict[int, str]:
    """port -> 'enddevice' | 'embb' (datastream only)."""
    out: dict[int, str] = {}
    with ports_path.open() as f:
        for line in f:
            parts = line.strip().split(",")
            if len(parts) < 3:
                continue
            kind, role, port_s = parts[0], parts[1], parts[2]
            if role != "datastream":
                continue
            try:
                out[int(port_s)] = kind
            except ValueError:
                pass
    return out


def parse_flow_rows(flow_path: Path) -> list[dict]:
    rows = []
    with flow_path.open() as f:
        f.readline()
        for line in f:
            line = line.strip()
            m = FLOW_RE.match(line)
            if not m:
                continue
            d = m.groupdict()
            rows.append({k: float(v) if "." in v else int(v) for k, v in d.items()})
    return rows


def summarize(rows: list[dict], label: str) -> dict:
    payload = [r for r in rows if r["tx_bytes"] > 100]
    pay_tx = sum(r["tx_bytes"] for r in payload)
    pay_rx = sum(r["rx_bytes"] for r in payload)
    delays = [r["delay"] for r in payload if r["rx_pkts"] > 0]
    return {
        f"{label}_payload_flows": len(payload),
        f"{label}_payload_tx_bytes": pay_tx,
        f"{label}_payload_rx_bytes": pay_rx,
        f"{label}_payload_pdr": (100.0 * pay_rx / pay_tx) if pay_tx else 0.0,
        f"{label}_payload_mean_delay_ms": (sum(delays) / len(delays)) if delays else 0.0,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", type=Path, required=True)
    ap.add_argument("--out", type=Path, default=None)
    args = ap.parse_args()

    # flowOutput does not store ports — only TX/RX aggregates.
    # Ports file lists datastream ports in install order; FlowMonitor order
    # is not guaranteed to match. Prefer IMSI-correlated outFile if needed later.
    # Here we report cell-wide payload KPIs plus attach stats, and note that
    # RedCap/eMBB PDR split needs classifier-aware dump (future).
    # For now: if embb_ues>0, mark coexist=1 and keep aggregate payload_*.

    rows_out = []
    with args.manifest.open() as f:
        for row in csv.DictReader(f):
            tag = row["sim_tag"]
            seed = int(row["seed"])
            ports = find_ports(tag, seed)
            flow = find_flow(tag, seed)
            parsed = {**row, "ports_file": str(ports) if ports else "", "flow_file": str(flow) if flow else ""}
            if ports:
                pc = load_port_classes(ports)
                parsed["n_redcap_datastream_ports"] = sum(1 for v in pc.values() if v == "enddevice")
                parsed["n_embb_datastream_ports"] = sum(1 for v in pc.values() if v == "embb")
            if flow:
                # Without per-flow port in flowOutput, keep aggregate only.
                fr = parse_flow_rows(flow)
                parsed.update(summarize(fr, "all"))
            rows_out.append(parsed)

    out = args.out or OUT_DIR / (args.manifest.stem.replace("_manifest", "_coexist_flows") + ".csv")
    out.parent.mkdir(parents=True, exist_ok=True)
    if not rows_out:
        print("No rows")
        return 1
    with out.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows_out[0].keys()))
        w.writeheader()
        w.writerows(rows_out)
    print(f"Wrote {out}")
    print(
        "Note: flowOutput lacks per-flow ports; RedCap vs eMBB PDR split needs "
        "scratch enhancement (export IMSI/port in flow lines) — tracked for M2b."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
