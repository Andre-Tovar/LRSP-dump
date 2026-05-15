# C LRSP Port — Repository Inspection Report

This is the deliverable required before any C code is written: a written record
of what was inspected, what we plan to port from Python, what we plan to reuse
from the Asta/Akca/Samija archive, what we plan to write fresh in C, and the
unresolved assumptions.

The inspection covered three things:

1. The current Python LRSP solver (`lrsp_solver/`) — the canonical reference
   for the architecture we are porting.
2. The archived Asta/Akca/Samija LRSP material — the source-code and
   dissertation reference.
3. The existing C MESPPRC solver (`mespprc_native/`) — the pricing engine the
   new C LRSP solver will call into.

## 1. Python LRSP solver — files inspected

`lrsp_solver/` is 13 files / 2,007 LOC. Every file was read.

| File | LOC | Role |
|------|-----|------|
| [`solver.py`](../lrsp_solver/solver.py) | 80 | Entry point. `LRSPSolver` orchestrates `ColumnGenerationSolver`. Selects pricing engine via `LRSPSolverConfig.pricing` ∈ {"dp","ip"}. |
| [`column_generation.py`](../lrsp_solver/column_generation.py) | 222 | Outer CG loop. Singleton warmstart. Per-iteration master + per-facility pricing. Termination on no-improving-column / time / iteration limit. |
| [`master_problem.py`](../lrsp_solver/master_problem.py) | 237 | Restricted master. PuLP/CBC. Akca set-partitioning formulation. Coverage `==`, capacity `<=`, optional linking `<=`, optional min-open `>=`. |
| [`instance.py`](../lrsp_solver/instance.py) | 396 | `LRSPInstance`, `Customer`, `Facility`. Loaders for Akca `.txt`, Python module dict, raw dicts. `synthetic_instance()` for tests. |
| [`pricing_graph.py`](../lrsp_solver/pricing_graph.py) | 191 | Per-facility reduced-cost graph builder. Constructs a `mespprc.MESPPRCInstance` whose arc costs are dual-adjusted. |
| [`pricing_dp.py`](../lrsp_solver/pricing_dp.py) | 259 | DP pricing adapter. Phase 1 → singleton columns; optionally Phase 2 DP → pairing column. |
| [`pricing_ip.py`](../lrsp_solver/pricing_ip.py) | 151 | IP pricing adapter. Same Phase 1 path; Phase 2 uses set-partitioning IP via PuLP/CBC. |
| [`pricing_interface.py`](../lrsp_solver/pricing_interface.py) | 86 | Abstract `PricingSolver`. `PricingProblem` (input) and `PricingResult` (output) data classes. |
| [`column.py`](../lrsp_solver/column.py) | 74 | `Column` (one master variable) and `MasterDuals`. |
| [`results.py`](../lrsp_solver/results.py) | 90 | `ColumnGenerationResult`, `IterationSummary`, `PricingFacilitySummary`. |
| [`experiment_runner.py`](../lrsp_solver/experiment_runner.py) | 117 | DP-vs-IP comparison harness. Runs both engines on the same instance, formats a side-by-side table. |
| [`__init__.py`](../lrsp_solver/__init__.py) | 97 | Public re-exports. |
| [`utils.py`](../lrsp_solver/utils.py) | 7 | `euclidean_distance`. |

Tests (under `tests/`):

| File | Role |
|------|------|
| [`tests/lrsp/test_instance_loading.py`](../tests/lrsp/test_instance_loading.py) | Akca `.txt` and dict-module loaders. |
| [`tests/lrsp/test_master_problem.py`](../tests/lrsp/test_master_problem.py) | Master feasibility, integer solve, deduplication. |
| [`tests/lrsp/test_column_generation.py`](../tests/lrsp/test_column_generation.py) | CG loop mechanics on synthetic instances. |
| [`tests/lrsp/test_pricing_interface.py`](../tests/lrsp/test_pricing_interface.py) | `PricingProblem` / `PricingResult` validation. |

> Note (package cleanup): the original inspection also listed
> `tests/test_lrsp_solver.py`, `tests/test_mespprc_lrsp_pricing.py`, and the
> `tests/lrsp_dp_vs_ip_benchmark_chart.py` harness. Those targeted a pre-port
> solver architecture (`mespprc_lrsp`, `lrsp_solver.pricing_adapter`,
> `build_akca_style_instance`) that no longer exists; they were removed from
> this package. Current LRSP coverage lives under `tests/lrsp/`, and the
> DP-vs-IP studies are driven by `lrsp_native/scripts/paper_lrsp_dp_vs_ip*.py`.

## 2. Master problem — formulation as we will port it

The Python master ([`master_problem.py:51-225`](../lrsp_solver/master_problem.py#L51-L225)):

```
variables
    y_j  for each facility j   ∈ [0,1]   (continuous in LP, binary in IP)
    λ_p  for each column p     ∈ [0,1]   (continuous in LP, binary in IP)

objective
    min  Σ_j  opening_cost_j * y_j  +  Σ_p  pairing_cost_p * λ_p

rows
    coverage_i      :  Σ_{p : i ∈ p}  λ_p           ==  1                for each customer i
    capacity_j      :  Σ_{p : f(p)=j}  d_p * λ_p   −  Cap_j * y_j  ≤ 0   for each facility j
    link_{i,j}      :  Σ_{p : f(p)=j ∧ i ∈ p}  λ_p −  y_j         ≤ 0   for (i,j) when linking is on
    min_open        :  Σ_j  y_j  ≥  K                                    when min-open bound is on
```

K = `LRSPInstance.minimum_required_open_facilities()` ([`instance.py:71-89`](../lrsp_solver/instance.py#L71-L89)) = greedy lower bound on facilities needed to cover total demand.

Dual extraction ([`master_problem.py:195-212`](../lrsp_solver/master_problem.py#L195-L212)) returns four dicts (one per row family) into `MasterDuals`. PuLP returns the duals directly in `==` / `<=` / `>=` sign convention; we will need to verify HiGHS does the same (see Risks).

## 3. Pricing — reduced-cost formula

[`pricing_graph.py:34-167`](../lrsp_solver/pricing_graph.py#L34-L167). For each facility j:

- Source = artificial node id 0 representing the depot at j.
- Sink = `max(customer_id) + 1`, also at j.
- Customer nodes between.
- One arc for every (source → c), (c → sink), and (c → c′) pair.

Reduced cost on the arc into customer i (head):

```
rc(arc → i) = base_travel_cost(tail, i)
            − coverage_dual[i]
            − facility_capacity_dual[j] * demand[i]
            − facility_customer_link_dual[(i, j)]
```

Reduced cost on the arc into the sink: just `base_travel_cost(tail, sink)`.

Resources:
- Local: `[demand_increment]`, plus `[base_cost]` if `vehicle_time_limit` is set.
- Global: `[base_cost]` if `vehicle_time_limit` is set, else empty. (This is how we
  enforce duty-time across multi-trip pairings.)

Pairing constant ([`pricing_graph.py:158`](../lrsp_solver/pricing_graph.py#L158)):
`pairing_constant = vehicle_fixed_cost − min_open_facilities * 0.0` (the second
term is currently a placeholder). The LRSP layer adds this constant to each
column's reduced cost from Phase 1 / Phase 2.

Both `pricing_dp.py` and `pricing_ip.py` apply Phase 1 first; Phase 2 only fires
when `vehicle_time_limit is not None` AND Phase 1 returned ≥ 2 negative-reduced-cost
routes ([`pricing_dp.py:176-181`](../lrsp_solver/pricing_dp.py#L176-L181), [`pricing_ip.py:119-126`](../lrsp_solver/pricing_ip.py#L119-L126)).

## 4. CG loop and stopping criteria

[`column_generation.py:52-175`](../lrsp_solver/column_generation.py#L52-L175):

```
1. Build singleton warmstart columns (one per (facility, customer) pair, feasible).
2. for iter in 0..max_iterations:
       solve master LP (relax=True) → objective, duals
       if not optimal: failure
       for each facility:
           build PricingProblem(instance, facility, duals, iter, ...)
           result = pricing_solver.solve(problem)
           added = master.add_columns(result.columns)
       record IterationSummary
       if no new columns added: reached_optimality, break
       if time_limit hit: break
   else: status = "iteration_limit"
3. Re-solve LP with the final pool to capture all added columns.
4. If solve_integer_master is on: solve(relax=False) for the integer optimum.
```

Singleton warmstart ([`column_generation.py:178-222`](../lrsp_solver/column_generation.py#L178-L222)):
one column per (facility, customer) where the column visits that customer and
returns. Skipped if `demand > vehicle_capacity` or if the round-trip travel cost
exceeds `vehicle_time_limit`.

## 5. Akca `.txt` instance format

[`instance.py:104-226`](../lrsp_solver/instance.py#L104-L226). Lines (whitespace separated):

```
1: <num_facilities> <num_customers>
2: <opening_cost_1> ... <opening_cost_F>
3: <vehicle_fixed_cost> <num_vehicles_per_facility>
4: <vehicle_capacity> <facility_capacity> <vehicle_time_limit>
5..(4+num_customers): <id> <x> <y> <service_time> <demand>
(4+num_customers+1)..(4+num_customers+num_facilities): <id> <x> <y> 0 0
```

Files without the `vehicle_time_limit` field on line 4 are explicitly rejected
([`instance.py:164-169`](../lrsp_solver/instance.py#L164-L169)) so a VRP/LRP
instance can never silently pass through.

## 6. Archive — files inspected

| Path | Verdict |
|------|---------|
| `ARCHIVED/no-good_LRSP.Solver/{lrsp_column_generation,master,column,branch_and_price}.py` | Earlier Python ancestor of `lrsp_solver/`. Same architecture, less polish. **Useful as cross-reference only.** Nothing to copy. |
| `Akca Repo/.../exact-1stp-bp/*.c, *.cpp` | Akca's exact branch-and-price. Bound to CPLEX 9.1 + Minto 10-07; 32-bit Linux build. **Will not compile in 2026 on Windows**, no usable libraries. **Algorithmic patterns only.** |
| `Akca Repo/.../exact-1stp-bp/pricingprob.c` | Pattern: facility-wise pricing with adaptive label-limit fallback. Useful as a control-flow reference for `src/pricing.c`. |
| `Akca Repo/.../exact-1stp-bp/InitColGenerator.c` | Clark-Wright / nearest-neighbor / sweep heuristics for warm-start. **Out of scope for v1**; logged as a future upgrade in `C_LRSP_TODO.md`. |
| `Akca Repo/.../routingproblems-lrp_dip_samira-26d8a14d22cf/*.py` | DipPy / COIN-OR LRP solvers. DipPy is unmaintained; not a build target for us. |
| `Akca Repo/.../routingproblems-lrp_dip_instances_samira-96a5850bf5af/{Lit,Random}_instances/` | Python-dict instance files. Not the format we use; we use Akca `.txt`. **Skip unless v2 adds a second loader.** |
| `Akca Repo/.../routingproblems-lrspcode-39e47f81716c/comb_pricing_pro-6/*.txt` | Akca `.txt` instance files. **Useful as additional smoke-test inputs.** Already loadable by our existing parser. |
| `Akca Repo/.../zelihadissertation-82e26cec9eb8/dissert6.pdf`, `lrpaper-bd1fa394ac55/lrp_paper.pdf` | LRP/LRSP formulation and B&P strategy. **Cite for `C_LRSP_ARCHITECTURE.md` background.** |
| `ARCHIVED/mespprc_c/` | Earlier C MESPPRC port, **superseded by `mespprc_native/`**. Skip. |

Languages roughly: ~227K LOC C, ~80 C++ files, ~90 Python files, plus PDFs.
None of the C/C++ is reusable as-is because of the CPLEX/Minto coupling and the
32-bit Linux Makefiles.

## 7. C MESPPRC ABI — what's available, what's missing

`mespprc_native/include/mespprc.h` exposes:

- Instance lifecycle: `mespprc_instance_create`, `set_local_limits`, `set_global_limits`, `add_node`, `add_arc`, `finalize`, `destroy`.
- Phase 1: `mespprc_solve_phase1`. Result handle exposes route count + per-route cost / path / local resources / global resources / customer-state signature / first-customer.
- Phase 2 DP: `mespprc_solve_phase2_dp`. Status, infeasibility reason, total cost, selected route indices into the Phase 1 result.
- Phase 2 IP: `mespprc_solve_phase2_ip`. Same accessors plus original/reduced route counts.

**Gap for pricing.** There is no setter for arc costs after `finalize`, no
dual-offset entry point. The decision (confirmed) is: **rebuild the
`mespprc_instance_t` from scratch each CG iteration**, baking the dual-adjusted
arc costs directly into the `add_arc` calls. For the LRSP instance sizes we
care about (≤ 30 customers, ≤ 6 facilities, ≤ 50 CG iterations), the
per-iteration setup is sub-millisecond and won't dominate. If profiling later
shows otherwise, the planned upgrade is a `mespprc_instance_set_arc_costs`
entry point, tracked in `C_LRSP_TODO.md`.

## 8. What will be ported directly from Python

Listed in the same order as the original implementation plan:

| Python | New C location | Notes |
|--------|----------------|-------|
| `instance.py` data classes | `src/instance.c` + `include/lrsp.h` | `LRSPInstance`, `Customer`, `Facility` as POD structs. |
| `instance.py::load_lrsp_instance` | `src/instance_io.c` | Akca `.txt` parser; reject files lacking `vehicle_time_limit`. |
| `column.py` | `src/column.c`, `src/duals.c` | Signature = sorted-customer-id tuple → 64-bit FNV hash for dedup. |
| `master_problem.py` | `src/master.c` | HiGHS C API. Same row structure, same sign convention to be verified. |
| `pricing_interface.py` | `include/lrsp.h` (interface) + `src/pricing.c` | `lrsp_pricing_method_t` enum, single dispatch. |
| `pricing_graph.py` | `src/pricing_graph.c` | Reduced-cost arc formula above, demand + time as MESPPRC resources. |
| `pricing_dp.py` | `src/pricing.c::lrsp_price_dp` | Phase 1 → columns; optional Phase 2 DP for pairings. |
| `pricing_ip.py` | `src/pricing.c::lrsp_price_ip` | Same Phase 1 path; Phase 2 IP for pairings. |
| `column_generation.py` | `src/column_generation.c` | Outer CG loop, exact same termination criteria. |
| `column_generation.py::_build_singleton_seed_columns` | `src/singleton_warmstart.c` | Per-(facility, customer) singleton columns with the same skip rules. |
| `solver.py` | `src/solver.c` | Public entry `lrsp_solve`. |
| `results.py` | `src/results.c` | Result struct + CSV writers. |

## 9. What will be reused from the archive

- **Algorithmic patterns only** — no archive C/C++ links into our build.
- `Akca .../exact-1stp-bp/pricingprob.c` — control-flow reference for facility-wise pricing in `src/pricing.c`.
- `Akca .../comb_pricing_pro-6/*.txt` — additional smoke-test instance files for the existing Akca `.txt` parser.
- Dissertation citations in `C_LRSP_ARCHITECTURE.md`.

## 10. What will be newly implemented in C (no Python equivalent)

- A bump-allocator arena (or vendored from `mespprc_native/src/arena.c`) for per-solve scratch.
- Per-iteration sub-arena reset to avoid quadratic memory growth.
- A dynamic column array with FNV signature dedup.
- HiGHS dynamic-LP wiring with `Highs_addCols` for new columns and
  `Highs_changeColsIntegralityByRange` for the integer master step.
- ctypes binding (`python/_native.py` + `python/adapters.py`) so Python tests
  can drive the C solver during equivalence validation.
- A CLI runner (`examples/run_lrsp.c`) and a side-by-side comparator
  (`examples/compare_ip_dp.c`) that write CSV outputs to
  `results/c_lrsp_comparison/`.

## 11. Solver dependencies

Build dependencies:

- MSVC (VS 2022 / 2026 BuildTools) for Windows — already verified working for `mespprc_native/`.
- CMake ≥ 3.20 (bundled with VS BuildTools).
- Ninja (bundled with VS BuildTools).
- HiGHS 1.7.2 — already vendored at `mespprc_native/third_party/HiGHS/` and built statically.
- `mespprc_native` — built as a dependency target.

Runtime dependencies (C side): none. The shared library and HiGHS link
statically. Ctypes binding is optional and adds no runtime dependency on the
C library itself.

Runtime dependencies (Python validation script): the existing `lrsp_solver/`
plus `pulp`, `numpy`, `matplotlib` — already on the developer machine.

## 12. Unresolved assumptions

These are flagged for verification once code starts being written, not blockers.

- **HiGHS dual sign convention.** The Python pricing graph subtracts coverage
  duals from arc costs assuming PuLP's `== 1` row sign. HiGHS may report duals
  with a different sign convention. To be verified in the master smoke test
  (Step 5 of the implementation plan) before wiring CG.
- **CBC vs HiGHS at the integer master.** Symmetry-rich LRSP IPs can have
  multiple optima with the same objective. The equivalence test compares the
  objective, not the selected support. Documented in `C_LRSP_TODO.md`.
- **Per-iteration MESPPRC instance rebuild cost.** Sub-millisecond at our
  target instance sizes, but will be measured during the first CG smoke run.
  Upgrade path (a new `mespprc_instance_set_arc_costs` entry point) is logged
  in `C_LRSP_TODO.md` if profiling demands it.
- **Linking and min-open constraints.** Optional in the Python solver,
  defaulted on. The C v1 will port both but leave them off by default to
  reduce equivalence-test surface, then turn them on once parity holds.
- **Akca's archived heuristics.** Not ported in v1. If the singleton warmstart
  is too weak on harder Akca instances we lift the Clark-Wright / sweep code
  from `Akca .../exact-1stp-bp/InitColGenerator.c`. Logged in `C_LRSP_TODO.md`.

## 13. v1 scope boundary

In scope:
- Full LRSP — location, routing, AND scheduling (vehicle time limit as a global
  resource enforced across multi-trip pairings).
- Column generation root LP + integer master.
- Both pricing engines (DP via `mespprc_solve_phase2_dp`, IP via
  `mespprc_solve_phase2_ip`).
- Akca `.txt` instance loading.
- Singleton warmstart.
- CLI runner + DP-vs-IP comparator.
- Validation script against the Python solver.

Out of scope for v1 (logged in `C_LRSP_TODO.md`):
- Branch-and-price outer loop.
- Akca's CW / sweep / nearest-neighbor warmstart heuristics.
- Alternative LP back-ends beyond HiGHS.
- Loading the Python-dict-style instance files from
  `Akca Repo/.../routingproblems-lrp_dip_instances_samira-96a5850bf5af/`.
- Subtour-elimination cuts (`Akca .../LRP_Flow_DipPy_Cut.py`).

## 14. Acceptance criteria

The C port is "done" when:

1. `lrsp_native/scripts/build.bat` produces `lrsp_native.dll` cleanly on
   Windows, linking statically against HiGHS and `mespprc_native`.
2. Every C test under `lrsp_native/tests/` passes.
3. `examples/run_lrsp.exe --instance <Akca.txt> --pricing dp` and
   `--pricing ip` both produce a feasible LRSP solution and the same
   integer objective (within 1e-6) on at least one shared Akca instance.
4. `scripts/validate_against_python.py` runs the same Akca instance through
   `lrsp_solver` (Python) and `lrsp_native` (C) and asserts root LP and
   integer objectives match within 1e-6.
5. The DP-vs-IP comparison script writes a CSV under
   `results/c_lrsp_comparison/` showing pricing-engine timings for a
   sweep of instances.
