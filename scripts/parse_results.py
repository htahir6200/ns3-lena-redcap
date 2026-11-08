#!/usr/bin/env python3
"""Parse campaign flow + energy logs into a summary CSV."""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NS3_RESULTS = ROOT / "ns-3.38" / "Results" / "Cni-Szenario"
OUT_DIR = ROOT / "results" / "campaigns"

FLOW_RE = re.compile(
    r"^(?P<tx_pkts>\d+):(?P<rx_pkts>\d+):(?P<delay>[\d.]+):(?P<jitter>[\d.]+):(?P<tx_bytes>\d+):(?P<rx_bytes>\d+)(?::(?P<ue_class>\w+))?$"
)
ENERGY_RE = re.compile(
    r"imsi;\s*(?P<imsi>\d+),\s*Energy in state (?P<state>[^:]+):(?P<energy>[\deE.+-]+)"
)
TIME_RE = re.compile(
    r"imsi;\s*(?P<imsi>\d+),\s*Time in state (?P<state>[^:]+):(?P<time>[\deE.+-]+)ms"
)


def find_flow_file(sim_tag: str, seed: int) -> Path | None:
    base = NS3_RESULTS / sim_tag / "Ausgaben"
    if not base.exists():
        return None
    matches = list(base.rglob(f"flowOutput_Seed{seed}.txt"))
    return matches[0] if matches else None


def find_energy_file(sim_tag: str, seed: int) -> Path | None:
    base = NS3_RESULTS / sim_tag / "Ausgaben"
    if not base.exists():
        return None
    matches = list(base.rglob(f"Seed_{seed}/Energystates.log"))
    return matches[0] if matches else None


def find_resource_file(sim_tag: str, seed: int) -> Path | None:
    base = NS3_RESULTS / sim_tag / "Ressourcen"
    if not base.exists():
        return None
    matches = list(base.rglob(f"Seed{seed}.txt"))
    return matches[0] if matches else None


def find_outfile(sim_tag: str, seed: int) -> Path | None:
    base = NS3_RESULTS / sim_tag / "Ausgaben"
    if not base.exists():
        return None
    matches = list(base.rglob(f"outFile_Seed{seed}_.txt"))
    return matches[0] if matches else None


def find_access_file(sim_tag: str, seed: int) -> Path | None:
    base = NS3_RESULTS / sim_tag / "Ausgaben"
    if not base.exists():
        return None
    matches = list(base.rglob(f"accessStats_Seed{seed}.txt"))
    return matches[0] if matches else None


def parse_access_stats(path: Path) -> dict:
    """Parse accessStats_Seed*.txt key=value dump from NrAccessStats."""
    out = {
        "preamble_tx": 0,
        "rar_timeout": 0,
        "ra_success": 0,
        "ra_failed": 0,
        "rrc_connected": 0,
        "rrc_timeout": 0,
        "prach_success_rate": float("nan"),
        "attach_fail": 0,
    }
    with path.open() as f:
        for line in f:
            line = line.strip()
            if not line or "=" not in line:
                continue
            k, v = line.split("=", 1)
            if k in out and k != "prach_success_rate" and k != "attach_fail":
                try:
                    out[k] = int(v)
                except ValueError:
                    pass
    pret = out["preamble_tx"]
    if pret > 0:
        out["prach_success_rate"] = out["ra_success"] / pret
    out["attach_fail"] = out["ra_failed"] + out["rrc_timeout"]
    return out


def parse_resources(path: Path) -> dict:
    """Parse Ressourcen Seed*.txt (free/used RB aggregates)."""
    empty = {
        "free_rb_dl": 0.0,
        "free_rb_ul": 0.0,
        "used_rb_dl": 0.0,
        "used_rb_ul": 0.0,
        "prach_rb": 0.0,
        "pdcch_rb": 0.0,
        "pdcch_used": 0.0,
        "pucch_rb": 0.0,
        "control_rb": 0.0,
        "ul_rb_util": 0.0,
        "dl_rb_util": 0.0,
    }
    with path.open() as f:
        header = f.readline()
        data = f.readline().strip()
    if not data:
        return empty
    parts = data.split(":")
    if len(parts) < 9:
        return empty
    vals = [float(x) for x in parts[:9]]
    free_dl, free_ul, used_dl, used_ul, prach, pdcch, pdcch_used, pucch, control = vals
    ul_den = used_ul + free_ul
    dl_den = used_dl + free_dl
    return {
        "free_rb_dl": free_dl,
        "free_rb_ul": free_ul,
        "used_rb_dl": used_dl,
        "used_rb_ul": used_ul,
        "prach_rb": prach,
        "pdcch_rb": pdcch,
        "pdcch_used": pdcch_used,
        "pucch_rb": pucch,
        "control_rb": control,
        "ul_rb_util": (used_ul / ul_den) if ul_den else 0.0,
        "dl_rb_util": (used_dl / dl_den) if dl_den else 0.0,
    }


def parse_outfile_capacity(path: Path) -> dict:
    """Pull Capacity usage DL/UL lines from outFile if present."""
    out = {"capacity_usage_dl": float("nan"), "capacity_usage_ul": float("nan")}
    with path.open() as f:
        for line in f:
            if line.startswith("Capacity usage DL:"):
                try:
                    out["capacity_usage_dl"] = float(line.split(":", 1)[1].strip())
                except ValueError:
                    pass
            elif line.startswith("Capacity usage UL:"):
                try:
                    out["capacity_usage_ul"] = float(line.split(":", 1)[1].strip())
                except ValueError:
                    pass
    return out


def parse_flows(path: Path) -> dict:
    rows = []
    with path.open() as f:
        header = f.readline()
        for line in f:
            line = line.strip()
            m = FLOW_RE.match(line)
            if not m:
                continue
            d = m.groupdict()
            row = {}
            for k, v in d.items():
                if v is None:
                    continue
                if k == "ue_class":
                    row[k] = v
                elif v.lower() == "nan":
                    row[k] = float("nan")
                elif "." in v:
                    row[k] = float(v)
                else:
                    row[k] = int(v)
            rows.append(row)
    if not rows:
        return {
            "n_flows": 0,
            "tx_packets": 0,
            "rx_packets": 0,
            "tx_bytes": 0,
            "rx_bytes": 0,
            "pdr": 0.0,
            "mean_delay_ms": 0.0,
            "payload_flows": 0,
            "payload_mean_delay_ms": 0.0,
            "payload_rx_bytes": 0,
            "payload_tx_bytes": 0,
            "payload_pdr": 0.0,
        }

    tx_p = sum(r["tx_pkts"] for r in rows)
    rx_p = sum(r["rx_pkts"] for r in rows)
    tx_b = sum(r["tx_bytes"] for r in rows)
    rx_b = sum(r["rx_bytes"] for r in rows)
    # Payload-ish flows: ignore tiny init (48 B) flows
    payload = [r for r in rows if r["tx_bytes"] > 100]
    delays = [r["delay"] for r in rows if r["rx_pkts"] > 0 and r["delay"] == r["delay"]]
    payload_delays = [
        r["delay"] for r in payload if r["rx_pkts"] > 0 and r["delay"] == r["delay"]
    ]
    pay_tx = sum(r["tx_bytes"] for r in payload)
    pay_rx = sum(r["rx_bytes"] for r in payload)

    def class_payload(name: str) -> dict:
        sub = [r for r in payload if r.get("ue_class") == name]
        tx = sum(r["tx_bytes"] for r in sub)
        rx = sum(r["rx_bytes"] for r in sub)
        dly = [r["delay"] for r in sub if r["rx_pkts"] > 0 and r["delay"] == r["delay"]]
        return {
            f"{name}_payload_tx_bytes": tx,
            f"{name}_payload_rx_bytes": rx,
            f"{name}_payload_pdr": (100.0 * rx / tx) if tx else 0.0,
            f"{name}_payload_mean_delay_ms": (sum(dly) / len(dly)) if dly else 0.0,
        }

    out = {
        "n_flows": len(rows),
        "tx_packets": tx_p,
        "rx_packets": rx_p,
        "tx_bytes": tx_b,
        "rx_bytes": rx_b,
        "pdr": (100.0 * rx_p / tx_p) if tx_p else 0.0,
        "mean_delay_ms": (sum(delays) / len(delays)) if delays else 0.0,
        "payload_flows": len(payload),
        "payload_mean_delay_ms": (sum(payload_delays) / len(payload_delays))
        if payload_delays
        else 0.0,
        "payload_rx_bytes": pay_rx,
        "payload_tx_bytes": pay_tx,
        "payload_pdr": (100.0 * pay_rx / pay_tx) if pay_tx else 0.0,
    }
    # Prefer RedCap payload as headline when classes are present (coexistence)
    if any(r.get("ue_class") == "redcap" for r in payload):
        out.update(class_payload("redcap"))
        out.update(class_payload("embb"))
        out["payload_pdr"] = out["redcap_payload_pdr"]
        out["payload_mean_delay_ms"] = out["redcap_payload_mean_delay_ms"]
        out["payload_tx_bytes"] = out["redcap_payload_tx_bytes"]
        out["payload_rx_bytes"] = out["redcap_payload_rx_bytes"]
    return out


def parse_energy(path: Path) -> dict:
    """Parse Energystates.log.

    The model may append the full per-UE snapshot more than once. Summing every
    dump double/triple-counts energy. Keep the *last* value seen for each
    (imsi, state), then sum states per UE.
    """
    # imsi -> state -> last energy / time
    energy_last: dict[int, dict[str, float]] = {}
    time_last: dict[int, dict[str, float]] = {}
    with path.open() as f:
        for line in f:
            em = ENERGY_RE.search(line)
            if em:
                imsi = int(em.group("imsi"))
                energy_last.setdefault(imsi, {})[em.group("state")] = float(em.group("energy"))
                continue
            tm = TIME_RE.search(line)
            if tm:
                imsi = int(tm.group("imsi"))
                time_last.setdefault(imsi, {})[tm.group("state")] = float(tm.group("time"))
    empty = {
        "ue_count_energy": 0,
        "mean_energy_j": 0.0,
        "total_energy_j": 0.0,
        "mean_rrc_connected_ms": 0.0,
        "mean_rrc_edrx_ms": 0.0,
        "mean_rrc_edrx_deep_ms": 0.0,
        "mean_rrc_edrx_ptw_ms": 0.0,
        "mean_energy_connected_j": 0.0,
        "mean_energy_tx_j": 0.0,
        "mean_energy_edrx_deep_j": 0.0,
        "mean_energy_edrx_ptw_j": 0.0,
    }
    if not energy_last:
        return empty

    totals = [sum(states.values()) for states in energy_last.values()]
    connected_ms = [time_last.get(i, {}).get("RRC_Connected", 0.0) for i in energy_last]
    # Deep eDRX (between PTW) — also the flat eDRX bucket when PTW is off
    edrx_deep_ms = [time_last.get(i, {}).get("RRC_IDLE_EDRX", 0.0) for i in energy_last]
    edrx_ptw_ms = [time_last.get(i, {}).get("RRC_IDLE_EDRX_PTW", 0.0) for i in energy_last]
    edrx_ms = [d + p for d, p in zip(edrx_deep_ms, edrx_ptw_ms)]
    e_conn = [energy_last[i].get("RRC_Connected", 0.0) for i in energy_last]
    e_tx = [
        energy_last[i].get("RRC_SENDING_PUSCH", 0.0)
        + energy_last[i].get("RRC_SENDING_PRACH", 0.0)
        for i in energy_last
    ]
    e_deep = [energy_last[i].get("RRC_IDLE_EDRX", 0.0) for i in energy_last]
    e_ptw = [energy_last[i].get("RRC_IDLE_EDRX_PTW", 0.0) for i in energy_last]
    n = len(totals)
    return {
        "ue_count_energy": n,
        "mean_energy_j": sum(totals) / n,
        "total_energy_j": sum(totals),
        "mean_rrc_connected_ms": sum(connected_ms) / n,
        "mean_rrc_edrx_ms": sum(edrx_ms) / n,
        "mean_rrc_edrx_deep_ms": sum(edrx_deep_ms) / n,
        "mean_rrc_edrx_ptw_ms": sum(edrx_ptw_ms) / n,
        "mean_energy_connected_j": sum(e_conn) / n,
        "mean_energy_tx_j": sum(e_tx) / n,
        "mean_energy_edrx_deep_j": sum(e_deep) / n,
        "mean_energy_edrx_ptw_j": sum(e_ptw) / n,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", type=Path, required=True)
    ap.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Output summary CSV (default: results/campaigns/<campaign>_summary.csv)",
    )
    args = ap.parse_args()

    rows_out = []
    with args.manifest.open() as f:
        for row in csv.DictReader(f):
            tag = row["sim_tag"]
            seed = int(row["seed"])
            flow_path = find_flow_file(tag, seed)
            energy_path = find_energy_file(tag, seed)
            res_path = find_resource_file(tag, seed)
            outfile_path = find_outfile(tag, seed)
            access_path = find_access_file(tag, seed)
            parsed = {
                **row,
                "flow_file": str(flow_path) if flow_path else "",
                "energy_file": str(energy_path) if energy_path else "",
                "resource_file": str(res_path) if res_path else "",
                "access_file": str(access_path) if access_path else "",
            }
            if flow_path:
                parsed.update(parse_flows(flow_path))
            if energy_path:
                parsed.update(parse_energy(energy_path))
            if res_path:
                parsed.update(parse_resources(res_path))
            if outfile_path:
                parsed.update(parse_outfile_capacity(outfile_path))
            if access_path:
                parsed.update(parse_access_stats(access_path))
            rows_out.append(parsed)

    out = args.out
    if out is None:
        name = args.manifest.stem.replace("_manifest", "_summary")
        out = OUT_DIR / f"{name}.csv"
    out.parent.mkdir(parents=True, exist_ok=True)

    if not rows_out:
        print("No rows parsed")
        return 1

    # Union keys across rows (coexistence adds redcap_/embb_* only on some runs)
    fields: list[str] = []
    seen = set()
    for r in rows_out:
        for k in r.keys():
            if k not in seen:
                seen.add(k)
                fields.append(k)
    with out.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows_out)

    print(f"Wrote {out}")
    # Compact console table
    print(
        f"{'config':<12} {'seed':>4} {'payPDR':>7} {'payDelay_ms':>12} "
        f"{'ra_ok':>6} {'rrc':>5} {'attFail':>7} {'prachS':>7} "
        f"{'ulUtil':>8} {'pdcchU':>8}"
    )
    for r in rows_out:
        prach_s = r.get("prach_success_rate", float("nan"))
        try:
            prach_s_f = float(prach_s)
            prach_s_s = f"{prach_s_f:7.2f}" if prach_s_f == prach_s_f else f"{'nan':>7}"
        except (TypeError, ValueError):
            prach_s_s = f"{'nan':>7}"
        print(
            f"{r.get('config_name', '?'):<12} {r.get('seed', '?'):>4} "
            f"{float(r.get('payload_pdr', 0)):7.2f} "
            f"{float(r.get('payload_mean_delay_ms', 0)):12.3f} "
            f"{int(float(r.get('ra_success', 0) or 0)):6d} "
            f"{int(float(r.get('rrc_connected', 0) or 0)):5d} "
            f"{int(float(r.get('attach_fail', 0) or 0)):7d} "
            f"{prach_s_s} "
            f"{float(r.get('ul_rb_util', 0)):8.3f} "
            f"{float(r.get('pdcch_used', 0)):8.0f}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
