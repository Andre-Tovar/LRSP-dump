# 06 — Figure-Ready Data

Plots the existing data supports. For each: axes, grouping, source CSV, and the
research claim it serves. Many of these **already exist as PNGs** in the
`results/*/` folders (regenerable via the sweep scripts' `--reanalyze` flag) —
the future paper can restyle or regenerate rather than build from scratch.

Primary cleaned source files (in `05_RESULTS_TABLES/cleaned/`):
- `lrsp_dp_vs_ip_dense_600s.csv` — main data set (one row per completed engine-run)
- `lrsp_dp_vs_ip_full.csv` — coarse sweep with varied F
- `mespprc_phase2_standalone.csv` — standalone Phase-2 micro-benchmark
- Per-cell features + labels + timeouts: `05_RESULTS_TABLES/raw/lrsp_dp_vs_ip_dense_600s__cells.csv`

> Reminder: completion-rate / "useful" plots must come from `cells.csv` (which
> includes timeouts), not from the cleaned `*_600s.csv` (completed runs only).

## Proposed figures

### F1 — Total runtime vs. customer count
- x: `n_customers` · y: `total_time` (log scale) · group: `method` (DP/IP), facet by `regime`
- Source: `cleaned/lrsp_dp_vs_ip_dense_600s.csv`
- Claim: IP runtime grows smoothly with N; DP either finishes near-instantly or
  not at all — the curves diverge past the crossover.

### F2 — Completion rate vs. customer count ("the DP cliff")
- x: `n_customers` · y: fraction of attempted cells completed within budget ·
  group: `method` · facet by `regime`
- Source: `raw/lrsp_dp_vs_ip_dense_600s__cells.csv` (`dp_completed`, `ip_completed`)
- Claim: DP completion collapses around N≈8–10; IP degrades gracefully. This is
  the headline reliability result.

### F3 — DP/IP speedup ratio vs. customer count
- x: `n_customers` · y: `speedup_ip_over_dp` (= dp_time / ip_time, log scale) ·
  group/color: `regime` · both-completed cells only
- Source: `cells.csv`
- Claim: where both finish, the ratio is heavy-tailed; DP only competitive on
  tiny/tight pools.

### F4 — Winner by route-pool size (the mechanistic figure)
- x: `avg_pool_per_call` (total_columns / pricing_calls) · y: outcome
  (DP-wins / IP-wins / tie) or DP/IP ratio · color: `regime`
- Source: `cells.csv`
- Claim: the route-pool size — not N directly — predicts the winner; this is
  the mechanism behind F1–F3. (Existing PNG: `fig_winner_by_pool_size*.png`.)

### F5 — Pricing time as a fraction of total runtime vs. N
- x: `n_customers` · y: `pricing_time / total_time` · group: `method`
- Source: `cleaned/lrsp_dp_vs_ip_dense_600s.csv`
- Claim: as N grows, pricing dominates the solve and the master LP is nearly
  free — so pricing-engine choice governs end-to-end performance.

### F6 — Columns generated, decomposed
- x: `n_customers` · y: stacked `seed_columns` / `phase1_route_columns` /
  `phase2_pairing_columns` · (IP runs; DP shape is similar)
- Source: `raw/…__raw_results.csv` (has the column-type breakdown)
- Claim: Phase-2 pairing columns appear only in looser regimes — exactly where
  the DP engine struggles.

### F7 — Column-generation iterations by method
- x: `n_customers` · y: `iterations` · group: `method`
- Source: `cleaned/lrsp_dp_vs_ip_dense_600s.csv`
- Claim: both engines drive the same column-generation process to comparable
  iteration counts; the runtime gap is per-call cost, not loop length.

### F8 — Objective agreement between DP and IP
- x: DP `objective` · y: IP `objective` (scatter, y=x reference); also
  `root_lp_objective` parity
- Source: `cleaned/lrsp_dp_vs_ip_dense_600s.csv` (join DP and IP rows on `instance`)
- Claim: DP and IP are solving the *same* problem — objectives agree (to ~1e-6
  on root LP) — so the comparison is of speed/reliability, not of solution
  quality. Validity check, not a performance claim.

### F9 — Hybrid vs. pure engines: completion count
- x: engine ∈ {DP, IP, Hybrid v3, Hybrid v4} · y: cells completed within budget
- Source: `results/lrsp_dp_vs_ip_dense_600s/hybrid_validation*` + `README.md`
- Claim: hybrid matches the per-cell winner on the training corpus; at 600 s it
  trails always-IP slightly (documented as timing variance, not classifier
  error — state this caveat on the figure).

### F10 — Standalone MESPPRC Phase-2 crossover
- x: `n_customers` · y: `total_time` (log) · group: `method`
- Source: `cleaned/mespprc_phase2_standalone.csv`
- Claim: at the MESPPRC level the DP-vs-IP crossover sits between n=5 and n=6 —
  the same mechanism as the in-LRSP cliff, observed in isolation.

### F11 — Runtime vs. facility count F
- x: `n_facilities` · y: `total_time` · group: `method`
- Source: `cleaned/lrsp_dp_vs_ip_full.csv` (the full sweep varies F; the dense
  sweep fixes F=max(2,N//3))
- Claim: F is a secondary axis; the pricing-engine effect is driven by N and
  regime, not F.

## Notes for whoever makes the figures
- Prefer log scale on all time axes (runtimes span microseconds to 600 s).
- Always show, or annotate, timeouts — a DP curve that "ends" is DP failing,
  not DP being absent. Truncating silently overstates DP.
- Facet by `regime`; it is the strongest moderator after N.
- For DP/IP ratio plots, restrict to both-completed cells and *say so* in the
  caption — that subset is biased small/tight.
