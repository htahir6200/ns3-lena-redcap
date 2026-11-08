#!/usr/bin/env bash
# Overnight dense-capacity campaign in the currently stable operating region.
# See docs/DENSE_CAPACITY_PLAN.md and docs/BUGFIX_*.md
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
RUNNER=(python3 scripts/runner_redcap.py)
WORKERS="${WORKERS:-2}"
SEEDS="${SEEDS:-3}"
LOG="$ROOT/results/campaigns/overnight_$(date +%Y%m%d_%H%M%S).log"
mkdir -p "$ROOT/results/campaigns"
exec >>"$LOG" 2>&1

echo "=== Overnight dense campaign start $(date) ==="
echo "log=$LOG workers=$WORKERS seeds=$SEEDS"

# Phase A: UE densification at interval 0.5 s (all configs stable here for 40 UEs)
for ues in 20 40 60 80 100; do
  echo "=== Phase A ues=$ues interval=0.5 ==="
  "${RUNNER[@]}" --campaign "ul_dense_cap_v1_u${ues}" \
    --configs 1 2 3 --seeds "$SEEDS" --ues "$ues" \
    --packet-size 5000 --interval 0.5 --sim-time 36 \
    --inactivity-timer 60000 --workers "$WORKERS" || true
done

# Phase B: offered-load at 40 UEs (min/max can go harder; eRedCap skip 0.2 for now)
echo "=== Phase B offered load ues=40 configs 1 2 ==="
for iv in 1.0 0.5 0.2 0.1; do
  tag=$(echo "$iv" | tr . p)
  "${RUNNER[@]}" --campaign "ul_load_cap_v1_i${tag}" \
    --configs 1 2 --seeds "$SEEDS" --ues 40 \
    --packet-size 5000 --interval "$iv" --sim-time 36 \
    --inactivity-timer 60000 --workers "$WORKERS" || true
done

echo "=== Phase B offered load ues=40 eRedCap (safer intervals) ==="
for iv in 1.0 0.5 0.3; do
  tag=$(echo "$iv" | tr . p)
  "${RUNNER[@]}" --campaign "ul_load_cap_v1_eredcap_i${tag}" \
    --configs 3 --seeds "$SEEDS" --ues 40 \
    --packet-size 5000 --interval "$iv" --sim-time 36 \
    --inactivity-timer 60000 --workers "$WORKERS" || true
done

# Phase C: combine manifests into one summary if possible
python3 - <<'PY'
from pathlib import Path
import csv
root = Path("results/campaigns")
rows = []
for m in sorted(root.glob("ul_dense_cap_v1_*_manifest.csv")) + sorted(root.glob("ul_load_cap_v1_*_manifest.csv")):
    with m.open() as f:
        rows.extend(csv.DictReader(f))
out = root / "ul_dense_overnight_combined_manifest.csv"
if rows:
    with out.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"Combined manifest: {out} ({len(rows)} jobs)")
PY

if [[ -f results/campaigns/ul_dense_overnight_combined_manifest.csv ]]; then
  python3 scripts/parse_results.py \
    --manifest results/campaigns/ul_dense_overnight_combined_manifest.csv \
    --out results/campaigns/ul_dense_overnight_summary.csv || true
fi

echo "=== Overnight dense campaign end $(date) ==="
