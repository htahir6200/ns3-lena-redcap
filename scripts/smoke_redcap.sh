#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT/cmake/CMake.app/Contents/bin:$PATH"
cd "$ROOT/ns-3.38"

# Usage: ./scripts/smoke_redcap.sh [simTime] [redCapConfig] [downlink]
SIM_TIME="${1:-3}"
RED_CAP="${2:-1}"
DOWNLINK="${3:-true}"

python3 ./ns3 run "cni-playground-base \
  --simTime=${SIM_TIME} \
  --enddevicesNumPergNb=1 \
  --packetSize=1000 \
  --intervall=1 \
  --redCapConfig=${RED_CAP} \
  --downlink=${DOWNLINK} \
  --simTag=smoke \
  --seed=1"
