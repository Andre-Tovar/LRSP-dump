# 05 — Results Tables

Standardized result tables for the paper, plus verbatim raw copies.

```
05_RESULTS_TABLES/
├── README.md      ← this file
├── raw/           ← verbatim copies of the source CSVs (do not edit)
└── cleaned/       ← standardized one-row-per-(method,instance) CSVs
```

## `cleaned/` — standardized schema

Every file in `cleaned/` uses this exact column order:

| column | meaning |
|---|---|
| `method` | pricing engine: `dp`, `ip` (or `hybrid` where applicable) |
| `instance` | instance identifier |
| `n_customers` | number of customers N |
| `n_facilities` | number of candidate facilities F (blank for standalone MESPPRC) |
| `n_periods` | **always blank** — see note below |
| `total_time` | total solve wall-clock, seconds |
| `pricing_time` | time inside the pricing subproblem, seconds |
| `master_time` | time inside the master LP/IP, seconds |
| `iterations` | column-generation iterations |
| `columns_generated` | total columns added to the master |
| `objective` | integer objective (LRSP) or Phase-2 objective (standalone) |
| `status` | termination status (`lp_optimal`, `iteration_limit`, `feasible`, …) |
| `notes` | free-text provenance / extra flags |
| `regime` | difficulty regime `easy`/`moderate`/`tight` (blank if N/A) |
| `seed` | generator seed |
| `root_lp_objective` | column-generation root LP objective (blank if N/A) |
| `phase2_pairing_columns` | count of multi-trip pairing columns generated |

### Why `n_periods` is always blank

The packet's requested schema includes `n_periods`, but **this LRSP variant has
no discrete scheduling periods**. The Scheduling layer is modeled as a
*continuous per-vehicle duty-time budget* (`vehicle_time_limit`), enforced as a
global resource across the routes of a multi-trip pairing. The column is kept
for schema stability but is intentionally empty. Do not infer a period count.

## `cleaned/` files and their source

| cleaned file | source (in `raw/`) | rows | what it is |
|---|---|---|---|
| `lrsp_dp_vs_ip_full.csv` | `lrsp_dp_vs_ip_full__raw_results.csv` | 161 | 180-cell "full" LRSP sweep, 60 s budget (completed runs) |
| `lrsp_dp_vs_ip_dense_30s.csv` | `lrsp_dp_vs_ip_dense__raw_results.csv` | 193 | 234-cell "dense" sweep, 30 s budget (completed runs) |
| `lrsp_dp_vs_ip_dense_600s.csv` | `lrsp_dp_vs_ip_dense_600s__raw_results.csv` | 203 | 234-cell "dense" sweep, 600 s budget (completed runs) — **primary data set** |
| `lrsp_c_dp_vs_ip_akca.csv` | `lrsp_c_dp_vs_ip__raw_results.csv` | 7 | C LRSP DP-vs-IP on real Akca instances (smoke scale) |
| `lrsp_pricing_comparison_python.csv` | `lrsp_pricing_comparison__comparison_results.csv` | 4 | Python-side LRSP comparison — **suspect, see below** |
| `mespprc_phase2_standalone.csv` | `phase2_dp_vs_ip__native_dp_vs_ip.csv` | 100 | standalone MESPPRC Phase-2 DP-vs-IP micro-benchmark (paired rows split into one row per engine) |

## `raw/` — verbatim copies (authoritative)

`raw/` holds untouched copies of the original `results/` CSVs, named
`{study}__{file}.csv`. Two are **not** in `cleaned/` because they are per-cell
feature tables, not per-run tables — keep them as-is for modeling/plots:

- `raw/lrsp_dp_vs_ip_dense__cells.csv` and
  `raw/lrsp_dp_vs_ip_dense_600s__cells.csv` — **234 rows each**, one per
  attempted (N,F,regime,seed) cell, with ~25 instance/regime features, per-engine
  outcomes (`dp_*`, `ip_*`), and training labels (`winner`, `dp_useful`,
  `ip_useful`, `speedup_ip_over_dp`, `avg_pool_per_call`). **Use these — not the
  cleaned per-run tables — for completion-rate, "useful", and winner analyses**,
  because they include timed-out cells that the `raw_results.csv` files omit.

## Critical caveats before using these tables

1. **`raw_results.csv` / the `cleaned/lrsp_dp_vs_ip_*` files contain only
   COMPLETED runs.** Timeouts are absent. Completion-rate and DP-vs-IP
   "win-rate" analyses must come from `cells.csv`, or DP will be silently
   overstated.
2. **`cleaned/lrsp_pricing_comparison_python.csv` is suspect.** Those Python
   runs hit `iteration_limit` (not LP optimality) and the timings are
   language-confounded (Python DP vs CBC-backed IP). Do not use for runtime
   claims; included only for completeness.
3. **600 s sweep cutoffs.** The dense 600 s sweep skipped an engine for larger
   N in a regime once it timed out on all 3 seeds — so DP/IP attempt counts
   differ per cell. Weight per-N aggregates by attempted cells.
4. Objectives: `objective` is the integer objective for LRSP rows; for the
   standalone MESPPRC table it is the Phase-2 objective and `master_time` is
   blank (no LRSP master in that study).

The cleaned tables were derived mechanically from the raw copies (drop nothing,
rename/relocate columns only). Regenerating them is a pure function of `raw/`.
