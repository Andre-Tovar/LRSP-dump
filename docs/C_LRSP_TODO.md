# C LRSP — Open Items

Things deliberately deferred from v1, ordered by likely usefulness.

## Tier 1 — small, no design risk

- **Python ctypes binding**. The C ABI in [`include/lrsp.h`](../lrsp_native/include/lrsp.h)
  is stable and mirrors `mespprc_native`'s pattern. A `python/_native.py`
  + `python/adapters.py` pair would let Python tests drive the C solver
  directly instead of going through the `run_lrsp.exe` subprocess +
  stdout-parsing trick the validation script currently uses. Estimate:
  half a day. Not blocking the equivalence claim — the stdout parser
  works fine.
- **`lrsp_pricing_graph` cross-check test**. The plan called for a
  `tests/test_pricing_graph.c` that builds the graph for a fixed instance
  + dual vector and asserts every arc cost matches the Python builder to
  1e-9. The current end-to-end equivalence on `p11-f25-v1t1.txt` already
  validates the formula on real data, but a per-arc test would localise
  any future regression faster. Estimate: half a day.
- **Sweep over the bundled Akca corpus**. The validation script currently
  takes one `--instance`. Wrap it in a shell loop that runs every Akca
  `.txt` under `Akca Repo/.../comb_pricing_pro-6/` and `comb_pricing_pro-6-3-upddom-ti-cuts/`.
  Estimate: 1 hour. Cheap insurance.

## Tier 2 — larger but still well-scoped

- **MESPPRC instance reuse across CG iterations**. We currently rebuild
  the `mespprc_instance_t` from scratch each iteration with new arc
  costs baked in. Sub-millisecond at our target sizes (≤ 30 customers,
  ≤ 6 facilities) but quadratic in N. If profiling on bigger instances
  shows it dominates, add a `mespprc_instance_set_arc_costs` entry point
  to `mespprc_native` and reuse the instance. Estimate: 2-3 days
  including re-running the equivalence tests.
- **Akca's CW / sweep / nearest-neighbor warmstart heuristics**. The
  singleton warmstart matches the Python default and is enough for the
  current instances. For harder instances the Akca code in
  `Akca Repo/.../exact-1stp-bp/InitColGenerator.c` lifts column-generation
  burden off the master in early iterations. Estimate: 3-4 days.

## Tier 3 — research-scope, out of v1

- **Branch-and-price outer loop**. v1 is column generation + final integer
  master only. The archived `branch_and_price.py` pattern (branch on
  customer-facility assignment, then on facility-opening) would close
  the LP-IP gap on harder instances.
- **Cut generation**. Subtour-elimination cuts as in
  `Akca .../LRP_Flow_DipPy_Cut.py`.
- **Alternative LP back-ends**. The current build is bound to HiGHS via
  the `mespprc_native` link path. Adding a thin solver-abstraction layer
  would let the master swap to GLPK / CPLEX / Gurobi. Not needed yet —
  HiGHS handles every instance we can build.
- **Loading the Python-dict-style instance files** under
  `Akca Repo/.../routingproblems-lrp_dip_instances_samira-96a5850bf5af/`.
  Different format from Akca `.txt`. The Python solver supports both via
  `load_instance_from_module`; the C side currently only does Akca `.txt`.

## Known minor warnings

- MSVC C4273 "inconsistent dll linkage" is reported when test executables
  link `src/instance.c` and `src/instance_io.c` directly (instead of
  through the DLL). The header has `__declspec(dllimport)` (because
  `LRSP_BUILDING_DLL` is not defined for the test exes), but the source
  files are in the same translation unit — MSVC just warns and emits the
  symbol; the link succeeds and the tests pass. Cleanest fix is to give
  the headers a separate `LRSP_API_PUBLIC` macro that respects
  in-translation-unit defines, but it's cosmetic.
- HiGHS' CMake produces a "vswhere.exe is not recognized" line at the
  start of every build. Harmless — some VS tooling that's not on PATH.

## Bug history

- **Equivalence gap (FIXED)**: in iteration 0 the C solver was reporting
  `reached_optimality` after one round on the singleton warmstart only,
  while Python ran 9 iterations and added 540 columns. Root cause:
  `mespprc_instance_create` was being called with `num_nodes = sink_id +
  2`, but `mespprc_instance_finalize` requires `node_count == num_nodes`
  exactly. The actual count we add is `1 + C + 1 = C + 2`. So
  finalize was returning `INSTANCE_INVALID`, the pricing graph build
  failed silently, and pricing returned no columns. Fixed in
  `src/pricing_graph.c` by computing `declared_node_count = C + 2`
  directly. After fix, C and Python objectives match to 1e-6.

- **Pairing-column reduced cost off by `(K-1) · vehicle_fixed_cost`
  (FIXED in C, still present in Python)**: when a Phase 2 pairing combines
  K Phase 1 routes into one column, the column's master cost is
  `K · vehicle_fixed_cost + total_travel`. The reduced-cost computation
  was adding only ONE `vehicle_fixed_cost` instead of K. This is an
  inefficiency, not a correctness bug — the master rejects non-improving
  columns when they reach it; the underestimate just admits some that
  end up at zero in the LP. Fixed in `src/pricing.c::build_pairing_column`.
  The Python `lrsp_solver/pricing_dp.py::_routes_to_pairing_column`
  still has the old formula; equivalence still holds because both
  solvers converge to the same root LP regardless.

- **Akca formulation completed**: v1 originally shipped with linking and
  min-open rows OFF "to reduce equivalence-test surface." Both are now
  ON by default in `src/api.c::lrsp_solver_config_default`, matching the
  Python defaults and Akca's dissertation formulation. The C master
  builds linking rows `Σ_{p facility=j ∧ i∈p} λ_p − y_j ≤ 0` and the
  min-open row `Σ y_j ≥ K`. Pricing graph already wired the linking
  dual; min-open dual does not enter pricing (its column coefficient
  on every λ_p is 0). To recover the simpler formulation pass
  `--no-linking` and / or `--no-min-open` to `run_lrsp.exe`.
