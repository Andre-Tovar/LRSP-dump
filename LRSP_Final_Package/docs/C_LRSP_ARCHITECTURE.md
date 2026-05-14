# C LRSP Architecture

How `lrsp_native/` maps to `lrsp_solver/` (Python) and how the pieces talk to
each other at runtime.

## Top-level call graph

```
                      lrsp_solve(instance, config, &result)            [src/solver.c]
                                       │
                                       ▼
                       lrsp_run_column_generation                      [src/column_generation.c]
                                       │
                       ┌───────────────┼─────────────────────────────────────┐
                       │               │                                     │
                       ▼               ▼                                     ▼
              build_singleton_     for each iteration:                  HiGHS final IP
              warmstart_columns       └─ master.solve_lp ──→ duals          (set integrality
              [src/singleton_         └─ for each facility:                  to integer, run)
              warmstart.c]               └─ pricing.solve ──→ columns
                                         └─ master.add_columns
```

## Module-level mapping

| Python module | C file | Notes |
|---|---|---|
| `lrsp_solver/instance.py` (`LRSPInstance`, `Customer`, `Facility`) | `src/instance.c` + structs in `src/internal.h` | POD copies. `lrsp_instance_t` is opaque to ABI consumers; struct definition lives in `internal.h`. |
| `lrsp_solver/instance.py::load_lrsp_instance` | `src/instance_io.c` | Akca `.txt` loader. Rejects files without `vehicle_time_limit` (line 4 must have ≥ 3 fields), preserving LRSP semantics. |
| `lrsp_solver/column.py` (`Column`) | `src/column.c` | Signature is FNV-1a 64-bit hash over `(facility_id, sorted covered_customer_ids, route_paths)`. |
| `lrsp_solver/column.py` (`MasterDuals`) | `src/duals.c` + `lrsp_duals_t` in `internal.h` | Sign convention same as PuLP / HiGHS: `==` rows return any-sign duals; `≤` rows return ≤ 0 when binding. |
| `lrsp_solver/master_problem.py::RestrictedMasterProblem` | `src/master.c` | HiGHS C API. Full Akca formulation: coverage `==1`, capacity `≤`, linking `Σ_{p facility=j ∧ i∈p} λ_p − y_j ≤ 0`, and min-open `Σ y_j ≥ K`. All rows added once at create time; columns added incrementally via `Highs_addCol` with linking-row coefficients in lockstep. |
| `lrsp_solver/pricing_graph.py::build_facility_pricing_graph` | `src/pricing_graph.c` | Builds a fresh `mespprc_instance_t` per facility per CG iteration with arc costs already adjusted by duals. |
| `lrsp_solver/pricing_dp.py`, `pricing_ip.py` | `src/pricing.c` (`lrsp_pricing_solve`) | Single function with `lrsp_pricing_method_t` switch. Phase 1 → singleton columns; Phase 2 (DP / IP) → optional pairing column. |
| `lrsp_solver/column_generation.py::ColumnGenerationSolver.solve` | `src/column_generation.c::lrsp_run_column_generation` | Same termination criteria: no-new-columns → `lp_optimal`; iteration cap → `iteration_limit`; time cap → `time_limit`; LP failure → `master_failed`. |
| `lrsp_solver/column_generation.py::_build_singleton_seed_columns` | `src/singleton_warmstart.c` | Per-(facility, customer) singletons; same skip rules (`demand > capacity` or round-trip travel > time limit). |
| `lrsp_solver/solver.py::LRSPSolver` | `src/solver.c::lrsp_solve` | Thin dispatcher. |
| `lrsp_solver/results.py` | `src/results.c` + accessor functions | Accessors copy out into caller buffers so the public ABI stays stable across struct layout changes. |

## Memory ownership

- One arena per solve is allocated inside `lrsp_run_column_generation`. The
  result handle owns it; calling `lrsp_result_destroy` frees the entire
  arena. Per-iteration scratch lives in a separate arena that is reset
  (not freed) between iterations to avoid quadratic growth.
- The master keeps its own arena for the long-lived column pool. The CG
  loop snapshots the pool into the result arena at the end of the solve so
  the result outlives the master.
- The MESPPRC pricing instance is allocated and freed inside
  `lrsp_pricing_graph_build` / `lrsp_pricing_graph_destroy` — once per
  facility per iteration. (The "rebuild per iteration" decision from the
  plan; sub-millisecond per call at our target instance sizes.)
- The HiGHS handle is owned by the master and destroyed in
  `lrsp_master_destroy`.

## Dual flow into pricing

For each customer i and facility j, on every arc into customer i:

```
rc(arc into i) = base_travel
                 - coverage_dual[i]                           [== row dual]
                 - facility_capacity_dual[j] * demand[i]      [≤ row dual, ≤ 0]
                 - facility_customer_link_dual[(i, j)]        [≤ row dual, ≤ 0; off in v1]
```

Arcs into the sink keep their raw travel cost. The pairing constant
(`vehicle_fixed_cost − min_open_facilities_dual * 0`, matching the Python
placeholder) is added to each emitted column's reduced cost so the LRSP
column dual exactly matches the master's view.

This formula is enforced in [`src/pricing_graph.c`](../lrsp_native/src/pricing_graph.c)
and verified against the Python implementation (same numerical result on
identical instance + dual vector).

## How the C MESPPRC solver is connected

`lrsp_native/` is a CMake subproject of itself but pulls in `mespprc_native`
via `add_subdirectory(../mespprc_native)`. That gives us the
`mespprc_native` shared-library target plus the `highs` static-library
target without duplicating the HiGHS build. We then link
`lrsp_native.dll → mespprc_native.dll → highs.lib`.

At runtime the pricing path is:

```
lrsp_pricing_solve
  → lrsp_pricing_graph_build  (constructs mespprc_instance_t)
  → mespprc_solve_phase1      (label-setting DP — generates routes)
  → for each route with reduced cost < -tol:
      build_phase1_column     (singleton column)
  → if vehicle_time_limit set AND ≥ 2 negative routes:
      mespprc_solve_phase2_{dp,ip}
      build_pairing_column    (multi-route pairing)
  → return columns to CG loop
```

## File-by-file dependencies

```
include/lrsp.h            ← public ABI (only this is in the dllimport contract)

src/internal.h            ← shared types (arena, instance, column, duals,
                            master, pricing graph, pricing result, iteration
                            summary, result struct)
src/arena.c               ← arena, FNV hash, euclidean distance
src/instance.c            ← LRSPInstance lifecycle + accessors
src/instance_io.c         ← Akca .txt loader
src/column.c              ← Column construction + signature
src/duals.c               ← MasterDuals lifecycle
src/master.c              ← HiGHS-backed restricted master
src/pricing_graph.c       ← per-facility reduced-cost MESPPRC instance
                            (depends on mespprc_native's public ABI)
src/pricing.c             ← DP / IP pricing adapters
src/singleton_warmstart.c ← per-(facility,customer) seed columns
src/column_generation.c   ← CG outer loop
src/solver.c              ← public lrsp_solve dispatcher
src/results.c             ← result accessors + destroy
src/api.c                 ← lrsp_version, lrsp_struct_sizes, status names,
                            lrsp_solver_config_default
```

## Equivalence to Python

- Master: same row structure (coverage, capacity, optional linking, optional
  min-open). Same dual sign convention.
- Pricing graph: identical reduced-cost formula. Identical resource layout
  (`local = [demand]` or `[demand, travel]`; `global = []` or `[travel]`).
- Singleton warmstart: identical skip rules.
- CG loop: identical termination criteria. Identical "re-solve LP after the
  loop" step so the reported root LP reflects every column added.
- Phase 1 / Phase 2: through the existing `mespprc_native` library, which
  already has its own equivalence guarantee against `mespprc/` (see
  `mespprc_native/README.md`).

The verification script [`scripts/validate_against_python.py`](../lrsp_native/scripts/validate_against_python.py)
asserts root LP and integer objectives match within 1e-6 and iteration
counts are within ±3 (HiGHS vs CBC may pick different LP optima during
intermediate CG iterations, perturbing the trajectory by a couple of
iterations).

## Akca / Samija archive — what was reused

Algorithmic patterns only, no archive C/C++ links into the build. The
ARCHIVED Asta/Akca/Samija material is bound to CPLEX 9.1 + Minto 10-07 and
will not compile in 2026 on Windows.

- `Akca .../exact-1stp-bp/pricingprob.c` — control-flow reference for
  facility-wise pricing (cap to N improving columns per call, fall back
  if Phase 1 saturates label limit). Influenced the design of
  `src/pricing.c`.
- `Akca .../exact-1stp-bp/InitColGenerator.c` — Clark-Wright / sweep
  warmstart heuristics. Logged in `C_LRSP_TODO.md` for v2.
- Akca `.txt` instances under `Akca Repo/.../comb_pricing_pro-6/` —
  load directly via `lrsp_instance_load_akca_txt`.
- Akca / Samija dissertations and papers — formulation grounding only.

`ARCHIVED/mespprc_c/` was an earlier C MESPPRC port, superseded by
`mespprc_native/`. Not in the build path.
