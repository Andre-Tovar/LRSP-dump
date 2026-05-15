# PROJECT EXPERIMENT INVENTORY

Companion to `PROJECT_RECONSTRUCTION_REPORT.md`. A focused catalogue of every
experiment output on disk, what produced it, and whether it is chart-ready.

## Result folders

| Folder | Experiment | Driver script | Rows / scope | Engines | Figures | Chart-ready? |
|---|---|---|---|---|---|---|
| `results/lrsp_dp_vs_ip_full/` | LRSP full sparse sweep, 60 s budget — 180 cells (20 (N,F) × 3 regimes × 3 seeds) | `lrsp_native/scripts/paper_lrsp_dp_vs_ip_full.py` | `raw_results.csv` ~161 rows | DP, IP | 8 PNGs (`fig_runtime_by_n`, `_by_f`, `runtime_box`, `completion`, `master_vs_pricing`, `columns_by_n`, `speedup_by_n`, `winner_by_pool_size`) + `summary.md` | Yes |
| `results/lrsp_dp_vs_ip_dense/` | LRSP dense sweep, 30 s budget — 234 cells (N=5..30, F=max(2,N//3), 3 regimes × 3 seeds) | `paper_lrsp_dp_vs_ip_dense.py` | `raw_results.csv` ~193 rows; `cells.csv` 234 rows | DP, IP, Hybrid | 7 PNGs + `summary.md`, `README.md`, `hybrid_selector.md`, `hybrid_validation*.csv/.md` | Yes — hybrid training set |
| `results/lrsp_dp_vs_ip_dense_600s/` | Same 234-cell corpus, 600 s budget — the definitive long-budget run | `paper_lrsp_dp_vs_ip_dense.py --time-limit-seconds 600` | `raw_results.csv` ~203 rows; `cells.csv`; `hybrid_validation*` | DP, IP, Hybrid v3 + v4 | 6 PNGs + `README.md`, `summary.md`, `hybrid_*`, `cutoff_state.json` | Yes — **primary data set** |
| `results/lrsp_c_dp_vs_ip/` | C LRSP DP-vs-IP on Akca `p11-*` instances | (compare driver) | `raw_results.csv` small | DP, IP | `scaling.png` + `summary.md` | Smoke only |
| `results/lrsp_c_dp_vs_ip_synthetic/` | C LRSP DP-vs-IP on synthetic instances | (compare driver) | `raw_results.csv` small | DP, IP | `scaling.png` + `summary.md` | Smoke only |
| `results/c_lrsp_comparison/` | Output of `compare_ip_dp.exe` | `lrsp_native/examples/compare_ip_dp.c` | `raw_results.csv` single instance | DP, IP | — | Demo only |
| `results/lrsp_pricing_comparison/` | Python LRSP IP-vs-DP over discovered Akca instances | `scripts/run_lrsp_pricing_comparison.py` | `comparison_results.csv/.json`, `comparison_table.txt`, `instance_inventory.json`, `synthetic/` microbench | DP, IP | — | Partial — many runs hit `iteration_limit` |
| `results/phase2_dp_vs_ip/` | Standalone **MESPPRC** Phase 2 DP-vs-IP (native) | `mespprc_native/scripts/paper_phase2_dp_vs_ip.py` | `native_dp_vs_ip.csv` (50 rows) | DP, IP | `native_dp_vs_ip.png` + `_summary.md` | Yes — conflict markers resolved 2026-05-15 |
| `Test Results/` | Early DP-vs-IP runtime probe | (early script) | `lrsp_dp_dp_vs_dp_ip_runtime.csv` | DP variants | `*.png`, `1, DP=8, IP=22.png` | Superseded by the sweeps |

## CSV schemas

### `raw_results.csv` (full / dense LRSP sweeps) — one row per (instance, engine)

`instance, pricing, customers, facilities, regime, seed, status, iterations,
pricing_calls, total_columns, seed_columns, phase1_route_columns,
phase2_pairing_columns, max_routes_per_column, avg_routes_per_pairing,
reached_optimality, total_seconds, master_seconds, pricing_seconds,
root_lp_objective, integer_objective, open_facilities`

### `cells.csv` (dense sweeps) — one row per attempted cell (paired DP+IP)

Features: `instance, n, f, regime, seed, bv_factor, bf_factor, gt_factor,
total_demand, mean_demand, max_demand, std_demand, vehicle_capacity,
vehicle_time_limit, vehicle_fixed_cost, mean_cust_nearest_fac_dist,
max_cust_nearest_fac_dist, mean_cust_cust_dist, demand_to_capacity_ratio,
max_singleton_round_trip, avg_singleton_round_trip, gt_slack, bv_slack`.
Per-engine outcomes: `dp_*` and `ip_*` (completed, total/master/pricing
seconds, iterations, pricing_calls, total_columns, phase1_route_columns,
phase2_pairing_columns, max_routes_per_column, avg_pool_per_call, root_lp,
integer). Labels: `winner, dp_useful, ip_useful, speedup_ip_over_dp,
log_speedup, avg_pool_per_call`.

### `comparison_results.csv` (Python `lrsp_pricing_comparison`)

`instance_name, instance_path, pricing_method, num_customers, num_facilities,
vehicle_time_limit, status, objective_value, root_lp_objective,
integer_objective, iterations, pricing_calls, total_columns, master_runtime,
pricing_runtime, total_runtime, avg_pricing_time_per_iteration,
max_pricing_time_in_iteration, reached_optimality, no_improving_column_found,
hit_time_limit, failed, failure_message`

### `native_dp_vs_ip.csv` (standalone MESPPRC Phase 2)

`n, replicate, seed, phase1_ms, phase1_route_count, dp_ms, dp_objective,
dp_feasible, dp_infeasibility_reason, ip_ms, ip_objective, ip_feasible,
ip_infeasibility_reason, ip_reduced_route_count, objective_match`
(Merge-conflict markers removed 2026-05-15; 50 data rows, parses cleanly.)

## Known data issues

1. **Merge conflicts — RESOLVED 2026-05-15.** 152 files repo-wide (the whole
   `mespprc/instance_db/` corpus, `results/phase2_dp_vs_ip/` CSV + summary,
   `tests/test_mespprc_c.py`, and `LRSP_Final_Package/` mirrors) had markers;
   both sides were byte-identical, stripped losslessly. No data issue remains.
2. `results/lrsp_pricing_comparison/` runs terminate at `iteration_limit=1` on
   several Akca instances — not a clean optimality data set.
3. `Test Results/` is an early probe superseded by the `results/` sweeps.

## Recommended canonical data set for charting

`results/lrsp_dp_vs_ip_dense_600s/cells.csv` (234 cells, full feature set,
long budget) — with `results/lrsp_dp_vs_ip_full/raw_results.csv` as the
F-varying cross-check. Both sweep scripts support `--reanalyze` to rebuild
figures from the CSVs without re-running the solver.
