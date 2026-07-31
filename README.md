# ns3-lena-redcap

ns-3.38 + patched 5G-LENA fork for **5G RedCap / eRedCap** simulation.

Baseline: [tudo-cni/ns3-lena-redcap](https://github.com/tudo-cni/ns3-lena-redcap) (TUDO CNI).

## Status

- Builds on Apple Silicon
- Smoke test `cni-playground-base` runs (PDR 100% on a short scenario)

## Layout

| Path | Purpose |
|------|---------|
| `ns-3.38/` | Simulator tree (stock build-support + TUDO RedCap patches) |

## Build (macOS)

```bash
cd ns-3.38
# Optional: put a local CMake on PATH if system CMake is too old
python3 ./ns3 configure --enable-examples --disable-python
python3 ./ns3 build
```

## Quick RedCap run

```bash
cd ns-3.38
python3 ./ns3 run "cni-playground-base --simTime=3 --enddevicesNumPergNb=1 --packetSize=1000 --intervall=1 --simTag=smoke --seed=1"
```

Outputs land under `ns-3.38/Results/Cni-Szenario/<simTag>/`.

Useful CLI flags:

```bash
--redCapConfig=1   # 1=RedCap min, 2=RedCap max, 3=eRedCap (5 MHz)
--downlink=true    # false => uplink-centered traffic
--enddevicesNumPergNb=5 --packetSize=5000 --intervall=5 --simTime=36 --simTag=probe
```

## Local fixes vs upstream

1. Restored missing `build-support/` from official ns-3.38 (upstream zip was incomplete).
2. Linked `${libenergy}` in `contrib/nr/CMakeLists.txt` (LiIonEnergySource).
3. Replaced nested `create_directory` with `create_directories` in `scratch/cni-playground-base.cc`.
