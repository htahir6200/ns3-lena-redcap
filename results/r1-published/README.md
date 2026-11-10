# IoTJ R1 campaign results

Cleaned per-run summaries (`*_summary.csv`), per-point aggregates (`*_agg.csv`,
mean/std/95 % CI over seeds), manifests and the paired error-model comparison.

- `f_*`: final campaigns with all R1 scheduler corrections enabled and full
  delivery accounting (FlowMonitor MaxPerHopDelay raised); produced with the
  simulator at the commit named in each manifest's `git_rev` column and
  reproducible from the tagged commit cited in the paper (grids in
  `scripts/r1/grids/f_*.json`).
- `r1_base_*`: the original scheduler (corrections disabled, deterministic DCI
  ordering), kept for the comparison with the submitted version.

Regenerate: `python3 -I scripts/r1/run_r1.py --grid scripts/r1/grids/<grid>.json --campaign <name>`
then `scripts/r1/analyze_r1.py`, `make_tables.py`, `plot_r1.py`.
