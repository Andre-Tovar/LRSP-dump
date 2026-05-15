# 08 — Paper Claims and Evidence

Only claims defensible from repository evidence are listed. Each row gives the
code evidence, the test/result evidence, supporting literature, a confidence
level, and caveats. **The paper must not exceed these claims.** Numbers are
from the repository's own `summary.md` / `README.md` files and the cleaned
tables in `05_RESULTS_TABLES/`.

Confidence scale: **High** = directly measured/verified, stable; **Medium** =
supported but with confounds or limited scope; **Low** = suggestive, needs more
work before publication.

---

### Claim 1 — The solver addresses the full LRSP (Location + Routing + Scheduling), not LRP or VRP
- **Code:** `lrsp_native/src/master.c` has facility-open variables + opening
  costs (Location), pairing/route columns under vehicle capacity (Routing);
  `pricing_graph.c` attaches a *global* duty-time resource enforced across
  multi-trip pairings (Scheduling). `instance_io.c` **rejects** any instance
  lacking `vehicle_time_limit`.
- **Tests/results:** `tests/lrsp/test_instance_loading.py`; the 369-instance
  corpus all carry duty-time budgets.
- **Literature:** Akça dissertation (2009); Akça/Berger/Ralphs LRSP paper.
- **Confidence:** High.
- **Caveats:** Scheduling is a continuous per-vehicle duty-time budget, not
  discrete time windows/periods. Say "duty-time scheduling," not "time-window."

### Claim 2 — The C solvers are faithful ports of the Python solvers (equivalence verified)
- **Code:** `lrsp_native/` mirrors `lrsp_solver/` module-for-module
  (`docs/C_LRSP_ARCHITECTURE.md`); `mespprc_native/` mirrors `mespprc/`.
- **Tests/results:** `mespprc_native/tests/test_phase1_equivalence.py` passes
  18/18; `validate_against_python.py` reports C vs Python root-LP and integer
  objectives matching to ~1e-6 on Akca `p11-f25`/`p11-f30`.
- **Literature:** —
- **Confidence:** High.
- **Caveats:** Integer objectives can differ by ~0.1% when the C and Python
  column pools admit different IP-optimal supports (both valid); equivalence is
  on the root LP and on objective value, not on the selected column set.

### Claim 3 — DP and IP pricing solve the same problem (no solution-quality difference)
- **Code:** both engines share Phase 1, the master, warm start, dual handling;
  only the Phase-2 call differs (`lrsp_native/src/pricing.c`).
- **Tests/results:** on shared instances DP and IP root-LP objectives agree to
  ~1e-6 (sweep `summary.md` files; figure F8 in `06_FIGURE_READY_DATA.md`).
- **Literature:** —
- **Confidence:** High.
- **Caveats:** The comparison is therefore strictly about speed/reliability,
  not objective quality. Integer objectives may differ slightly per Claim 2.

### Claim 4 — Inside LRSP, IP pricing is substantially more reliable than DP pricing across the corpus
- **Code:** engine selected by `lrsp_pricing_method_t` in `pricing.c`.
- **Tests/results:** E3 (234-cell, 600 s budget): IP completes **165/234**, DP
  **38/234** (`results/lrsp_dp_vs_ip_dense_600s/README.md`,
  `cleaned/lrsp_dp_vs_ip_dense_600s.csv` has 165 IP + 38 DP completed rows).
  E1 (180-cell, 60 s): IP "usefully solves" 57% of cells, DP 12%
  (`results/lrsp_dp_vs_ip_full/summary.md`).
- **Literature:** consistent with HiGHS-style MIP presolve strength.
- **Confidence:** High (for this corpus / these budgets).
- **Caveats:** "Reliability" = completion within a fixed time budget. Results
  are for synthetic instances, N ≤ 30, single-thread HiGHS. The 600 s sweep
  used per-regime engine cutoffs (`cutoff_state.json`).

### Claim 5 — DP completion collapses sharply with customer count; IP degrades gracefully
- **Code:** Phase 2 DP `phase2_dp.c` (route-network covering DP).
- **Tests/results:** `dense_600s/README.md` cliff table — DP completion drops
  below 50% at N≈9 (easy/moderate) / N≈10 (tight); IP stays solvable far
  longer (easy N≈13, moderate N≈24, tight through N=30).
- **Literature:** exponential label growth in covering DPs.
- **Confidence:** High.
- **Caveats:** Exact cliff N depends on regime and budget; cite the regime.

### Claim 6 — The DP's cost is governed by the Phase-1 route-pool size, not by N directly
- **Code:** `phase2_dp.c` label space ≈ antichains in the route-compatibility
  order; pool size set by what Phase 1 emits, which depends on regime.
- **Tests/results:** winner-by-pool-size figures
  (`fig_winner_by_pool_size*.png`); `cells.csv` `avg_pool_per_call` is the
  single best winner predictor (dense `README.md`).
- **Literature:** —
- **Confidence:** Medium-High (strong empirical support; "antichain" framing is
  the repo's mechanistic explanation, not a proven bound).
- **Caveats:** Present the antichain argument as a mechanism/hypothesis
  consistent with the data, not a theorem.

### Claim 7 — At the standalone MESPPRC level, the DP-vs-IP Phase-2 crossover is between n=5 and n=6
- **Code:** `mespprc_native/scripts/paper_phase2_dp_vs_ip.py`.
- **Tests/results:** `mespprc_native/README.md` timing table; E4
  (`cleaned/mespprc_phase2_standalone.csv`).
- **Literature:** —
- **Confidence:** High (standalone); Medium as a *predictor* of in-LRSP
  behavior — inside LRSP the threshold shifts with regime.
- **Caveats:** Standalone n is not the same axis as in-LRSP N; the crossover is
  a route-pool-size effect (Claim 6), and the pool size at a given N varies.

### Claim 8 — A trained per-instance hybrid selector reproduces the per-cell winner on the corpus
- **Code:** `lrsp_hybrid_select_engine` (depth-3 tree) in
  `lrsp_native/src/column_generation.c`; trained by `train_hybrid_selector.py`.
- **Tests/results:** `dense_600s/README.md` — hybrid v4 achieves 100% in-sample
  winner-prediction accuracy on all 234 cells.
- **Literature:** —
- **Confidence:** Medium. It is **in-sample** accuracy on the training corpus.
- **Caveats:** No held-out / cross-validated generalization is reported. Do not
  claim the selector generalizes beyond this corpus without a held-out test.

### Claim 9 — The hybrid does not beat always-IP in deployed completion count at the 600 s budget
- **Code:** as Claim 8.
- **Tests/results:** `dense_600s/README.md` — hybrid v4 completes 156/234,
  always-IP 165/234.
- **Literature:** —
- **Confidence:** Medium.
- **Caveats:** The repo attributes the 9-cell gap to run-to-run timing variance
  on budget-edge cells, not classifier error. Honest framing: the hybrid is a
  *correct selector* but not a *wall-clock win* over always-IP at long budgets;
  it may help at very short budgets (v3). Present both sides.

### Claim 10 — As N grows, pricing dominates total runtime; the master LP is comparatively cheap
- **Code:** per-run `master_seconds` vs `pricing_seconds` recorded.
- **Tests/results:** `fig_master_vs_pricing.png` and full-sweep `summary.md`;
  `master_time` vs `pricing_time` columns in the cleaned tables.
- **Literature:** standard for column generation on routing problems.
- **Confidence:** High.
- **Caveats:** Holds for the studied sizes; the master could matter more at
  much larger F or with cuts added.

### Claim 11 — A native-code reimplementation is required for a fair DP-vs-IP comparison
- **Code:** Python IP path uses CBC via PuLP (compiled) while Python DP is
  interpreted; `mespprc_native`/`lrsp_native` make both engines + master native.
- **Tests/results:** narrative documented in `mespprc_native/README.md`,
  `lrsp_native/README.md`.
- **Literature:** —
- **Confidence:** High (as a methodological argument).
- **Caveats:** This is a statement about experimental validity, not a numeric
  result. It justifies *why* the C port exists.

---

## Claims the paper must NOT make (unsupported by this repository)

- ❌ "DP pricing is obsolete / always worse." — DP wins on small/tight pools;
  it is a route-pool-size effect.
- ❌ "The hybrid selector is the best engine." — it does not beat always-IP at
  600 s; only in-sample winner accuracy is shown.
- ❌ Any claim about instances with N > 30, real-world data, or against the
  published Akça branch-and-price benchmark tables — not tested here.
- ❌ Branch-and-price / cut-generation performance — v1 is column generation +
  final integer master only (`docs/C_LRSP_TODO.md`).
- ❌ Multi-threaded or alternative-LP-solver results — single-thread HiGHS only.
- ❌ Treating LRSP results as LRP or VRP results, or vice versa.
