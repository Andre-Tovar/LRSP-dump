# PROJECT NEXT STEPS

Companion to `PROJECT_RECONSTRUCTION_REPORT.md`. The next phase is **analysis,
charting, and paper preparation** — not solver development.

## Priority 0 — unblock

1. **Resolve merge conflicts — DONE (2026-05-15).** All 152 affected files
   (the whole `mespprc/instance_db/` corpus, the `phase2_dp_vs_ip` CSV +
   summary, `tests/test_mespprc_c.py`, and the `LRSP_Final_Package/` mirrors)
   were resolved losslessly — both conflict sides were byte-identical.
   Verified: instance JSONs + manifest parse, `native_dp_vs_ip.csv` parses,
   `mespprc_native` Phase 1 equivalence tests pass 18/18, LRSP Python tests
   pass 31/31. **Remaining follow-up:** `tests/test_mespprc_c.py` is still not
   runnable — it imports the obsolete `ARCHIVED/mespprc_c` package (broken
   relative import; `ARCHIVED/` is not a package). Delete the test or rewrite
   it to target `mespprc_native`.

## Priority 1 — confirm the data

2. **Verify final experiment outputs.** Confirm `lrsp_dp_vs_ip_dense_600s/` is
   the canonical sweep. Check `raw_results.csv` (~203 rows) and `cells.csv`
   (234 rows) load and that all feature columns are populated.
3. **Standardise result columns.** Define one documented CSV schema shared by
   `lrsp_dp_vs_ip_full`, `_dense`, `_dense_600s`. Document the regime knobs
   (β_v, β_f, γ_t) and the `winner` label encoding
   (`dp/ip/tie/dp_only/ip_only/both_to`).

## Priority 2 — charts and tables

4. **Regenerate publication-quality charts** via the sweep scripts'
   `--reanalyze` flag (no re-solve): runtime vs N (faceted by regime),
   DP/IP speedup vs N and vs route-pool size, completion rate vs N (the DP
   cliff), columns generated vs N stacked by type, pricing fraction of total
   runtime, hybrid vs pure IP/DP.
5. **Per-instance comparison reports** — a table per (N, F, regime) with DP vs
   IP vs Hybrid total time, status, and objective parity.
6. **Summary tables** — top-line completion/usefulness counts per regime;
   DP/IP runtime ratios; hybrid head-to-head with always-IP.

## Priority 3 — research synthesis

7. **State the IP vs DP vs Hybrid findings** (4–6 bullet conclusions) with the
   mechanistic explanation: Phase 2 DP's label space is exponential in
   route-pool size; pool size is regime-driven, not N-driven; IP has flat
   per-call overhead; hybrid is a correct classifier but only a marginal
   wall-clock win at long budgets.
8. **Identify missing experiments** — N > 30 scalability; C-vs-published-Akca
   benchmark comparison; per-iteration pricing-time instrumentation if a
   time-series chart is desired.
9. **Reconcile the two hybrid selectors** — document that the deployed hybrid
   is the per-instance decision tree in `column_generation.c`, and clarify the
   status of the older `LRSP_HYBRID_PHASE1_THRESHOLD` per-call selector
   described in `lrsp.h`.

## Priority 4 — paper

10. **Draft a technical outline:** problem (LRSP — location + routing +
    scheduling) → formulation (Akca set-partitioning / pairing) → column
    generation framework → MESPPRC pricing (two-phase) → DP vs IP Phase 2
    engines → native-C fairness motivation → experimental design (234-cell
    sweep, 3 regimes) → results → hybrid selector → conclusions.
11. **Position against prior work** — Zeliha Akca's dissertation and the LRSP
    paper in `Akca Repo/`; cite the dissertation formulation and B&P method.

## Explicitly NOT next steps

- No new solver features (branch-and-price, cuts, alternative LP back-ends are
  all deferred in `docs/C_LRSP_TODO.md` and out of scope for the paper).
- No re-porting — the C solvers are built and validated.
