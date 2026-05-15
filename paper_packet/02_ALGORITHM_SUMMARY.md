# 02 — Algorithm Summary

Technical description of every algorithm in the solver, at the level of detail
a paper's "Methodology" section needs. Grounded in `lrsp_native/src/`,
`mespprc_native/src/`, and `docs/C_LRSP_ARCHITECTURE.md` /
`docs/C_LRSP_PORT_INSPECTION.md`.

Notation:
- `C` = set of customers, `i ∈ C`; `F` = set of candidate facilities, `j ∈ F`.
- A **route** = an elementary depot→customers→depot trip from one facility.
- A **pairing** = a set of routes one vehicle performs in sequence under one
  shared duty-time budget (this is the "multi-trip" part).
- A **column** `p` in the master = a facility plus the customers it covers,
  with a cost; in practice a column is either a single route or a pairing.

## 1. The LRSP restricted master problem (RMP)

Implemented in `lrsp_native/src/master.c` (HiGHS C API); Python reference
`lrsp_solver/master_problem.py` (PuLP/CBC). It is the **Akca set-partitioning
formulation**:

```
variables
    y_j   ∈ [0,1]   facility-open variable, one per facility j   (binary in final IP)
    λ_p   ∈ [0,1]   column-selection variable, one per column p  (binary in final IP)

objective
    minimize   Σ_j  opening_cost_j · y_j   +   Σ_p  cost_p · λ_p

constraints
    (coverage_i)  Σ_{p : i ∈ p}  λ_p                       =  1     ∀ i ∈ C
    (capacity_j)  Σ_{p : fac(p)=j} demand_p · λ_p − Cap_j·y_j ≤ 0    ∀ j ∈ F
    (link_{i,j})  Σ_{p : fac(p)=j, i∈p} λ_p − y_j           ≤ 0     ∀ (i,j)   [optional, ON by default]
    (min_open)    Σ_j  y_j                                  ≥  K            [optional, ON by default]
```

- `cost_p` = total travel cost of the column's routes + vehicle fixed cost per
  route in the column.
- `coverage` is an equality (set partitioning): every customer covered exactly
  once. `capacity` and `link` are ≤ rows; `min_open` is a ≥ row.
- `K` = a greedy lower bound on the number of facilities needed to cover total
  demand.
- The master is solved as an **LP relaxation** during column generation; after
  the loop it is re-solved with `y_j, λ_p` integer to get the LRSP solution.
- Columns are added incrementally (`Highs_addCol`); all rows are created once.

## 2. Duals → pricing

After each RMP LP solve, four families of dual values are extracted
(`lrsp_native/src/duals.c`):

- `π_i`   — coverage-row duals (free sign);
- `μ_j`   — facility-capacity-row duals (≤ 0 when binding);
- `σ_{i,j}` — facility-customer linking-row duals (≤ 0 when binding);
- the min-open-row dual (does not enter pricing — its coefficient on every
  `λ_p` is 0).

For facility `j`, a **reduced-cost MESPPRC instance** is built
(`lrsp_native/src/pricing_graph.c`): a graph with a source (depot at `j`),
a sink (depot at `j`), and one node per customer. The reduced cost on the arc
entering customer `i` is

```
rc(arc → i) = base_travel_cost(tail, i)
              − π_i                        (coverage dual)
              − μ_j · demand_i              (capacity dual × demand)
              − σ_{i,j}                     (linking dual)
```

Arcs into the sink keep raw travel cost. Resources attached to the graph:
- **local** resource `demand` (vehicle capacity), and `travel` if a vehicle
  time limit is set;
- **global** resource `travel` if a vehicle time limit is set — this global
  resource is what enforces the **scheduling** (duty-time) constraint across
  the multiple routes of a pairing.

A constant `pairing_constant = vehicle_fixed_cost` is added to each emitted
column's reduced cost so the column's master-side reduced cost is exact.

## 3. The MESPPRC pricing subproblem

Solved per facility per iteration. Two phases (`mespprc_native/src/`).

### Phase 1 — route generation (label-setting DP)

`mespprc_native/src/phase1.c`. A standard **ESPPRC labeling dynamic program**:
labels propagate from the source along arcs; each label records accumulated
reduced cost, resource consumption, and the set of visited customers (a
bitset, enforcing *elementarity*). Dominated labels are pruned. The output is
a pool of elementary routes, each with a (possibly negative) reduced cost.
Phase 1 is **identical for both pricing engines** — it is shared code, which
is what makes the DP-vs-IP comparison clean.

### Phase 2 — pairing assembly (the two engines)

Phase 2 takes the Phase-1 route pool and selects a subset of routes that
together (a) cover a required customer set and (b) form a feasible pairing
under the global duty-time budget. This is a **set-partitioning problem over
the route pool**. Two engines solve it:

#### Phase 2 DP engine — `mespprc_native/src/phase2_dp.c`

A route-network covering dynamic program. It enumerates combinations of
**mutually compatible** routes, where two routes are compatible iff their
required-customer sets are disjoint. It carries one label per partial
selection. The label space therefore grows with the number of **antichains in
the route-pool compatibility partial order** — which is small when the pool is
small or highly overlapping, but **exponential** when the pool admits many
disjoint route pairs. Consequence: DP is extremely fast on small/tight route
pools and blows up combinatorially on large/loose ones.

#### Phase 2 IP engine — `mespprc_native/src/phase2_ip.c`

Builds an explicit set-partitioning **integer program** and hands it to the
**HiGHS** MIP solver: a binary variable per route, equality coverage rows, and
resource ≤ rows for the duty-time budget. HiGHS presolve aggregates the
equality structure; most LP relaxations are integer at the root; branch-and-
bound on the residual is fast. The cost profile is a **roughly flat per-call
overhead** (model build + presolve + simplex setup) that grows only mildly
with route count.

### When Phase 2 fires

Phase 2 runs only when (a) a vehicle time limit is set AND (b) Phase 1 returned
≥ 2 negative-reduced-cost routes for the facility — i.e. only when there is
something to combine. Otherwise pricing emits single-route columns directly.

## 4. Hybrid pricing

`lrsp_native/src/column_generation.c`, function `lrsp_hybrid_select_engine`.
A depth-3 decision tree trained on the dense-sweep `cells.csv` to predict the
faster engine from features available before solving. Deployed ("v4") rule —
resolved **once per run**:

```
if  n_customers > 5                  -> IP
elif vehicle_time_limit ≤ 437.6357   -> DP
elif total_demand > 108.0            -> DP
else                                 -> IP
```

An earlier per-call variant ("v3") chose DP/IP inside each pricing call from
that call's negative-route count vs. a threshold (`LRSP_HYBRID_PHASE1_THRESHOLD`
in `lrsp_native/include/lrsp.h`). The per-instance v4 tree is the deployed
default; v3 is retained for short-budget regimes. The paper should treat v4 as
"the hybrid" and mention v3 as a variant.

## 5. Column generation loop

`lrsp_native/src/column_generation.c`, `lrsp_run_column_generation`. Same
termination criteria as the Python reference.

## 6. Pseudocode — full LRSP solver

```
INPUT:  LRSP instance (customers, facilities, vehicle capacity,
        vehicle duty-time limit, costs); pricing method ∈ {DP, IP, HYBRID}
OUTPUT: LRSP solution (open facilities + selected pairings) and its objective

function LRSP_SOLVE(instance, method):
    if method == HYBRID:
        method ← HYBRID_SELECT_ENGINE(instance)        # depth-3 tree, once per run

    # ---- warm start -------------------------------------------------------
    pool ← BUILD_SINGLETON_WARMSTART(instance)         # one feasible (facility,
                                                       # customer) round-trip column
                                                       # each, skipping infeasible ones
    master ← BUILD_RMP(instance, pool)                 # Akca set-partitioning RMP

    # ---- column generation loop ------------------------------------------
    for iter in 1 .. max_iterations:
        (lp_obj, duals) ← SOLVE_LP(master)             # HiGHS LP relaxation
        if LP not optimal: return status = master_failed

        new_columns ← []
        for each facility j in F:
            G_j ← BUILD_PRICING_GRAPH(instance, j, duals)   # reduced-cost MESPPRC inst.
            routes ← MESPPRC_PHASE1(G_j)                    # label-setting DP
            for r in routes with reduced_cost(r) < −tol:
                new_columns.append( SINGLE_ROUTE_COLUMN(r) )
            if duty_time_limit set AND (#negative routes ≥ 2):
                if method == DP: pairing ← MESPPRC_PHASE2_DP(G_j, routes)
                else:            pairing ← MESPPRC_PHASE2_IP(G_j, routes)   # HiGHS
                if pairing improving:
                    new_columns.append( PAIRING_COLUMN(pairing) )

        added ← ADD_COLUMNS(master, new_columns)       # FNV-signature dedup
        record IterationSummary(iter, lp_obj, added, timings)

        if added == 0:        status ← lp_optimal;      break   # CG converged
        if time budget hit:   status ← time_limit;      break
    else:
        status ← iteration_limit

    # ---- finalize ---------------------------------------------------------
    (root_lp_obj, _) ← SOLVE_LP(master)                # re-solve to capture all columns
    if solve_integer_master:
        (integer_obj, solution) ← SOLVE_IP(master)     # y_j, λ_p binary
    return (status, root_lp_obj, integer_obj, solution)
```

```
function MESPPRC_PHASE1(G):                            # ESPPRC labeling DP
    labels ← { initial label at source }
    repeat:
        extend non-dominated labels along arcs, updating
        (reduced cost, capacity used, time used, visited-customer bitset);
        discard labels violating capacity / time or revisiting a customer;
        prune dominated labels
    until no label changes
    return routes reconstructed from labels reaching the sink

function MESPPRC_PHASE2_DP(G, routes):                 # route-network covering DP
    # label = a set of mutually customer-disjoint routes chosen so far
    enumerate compatible route combinations, keeping best cost per covered set,
    respecting the shared duty-time budget
    return min-cost feasible pairing (or "none improving")

function MESPPRC_PHASE2_IP(G, routes):                 # set-partitioning IP via HiGHS
    build IP: binary x_r per route; equality coverage rows;
              ≤ rows for the duty-time budget
    solve with HiGHS MIP
    return pairing from the optimal x (or "none improving")
```

The **only** difference between a DP run and an IP run on the same instance is
the `MESPPRC_PHASE2_*` call. Warm start, master, Phase 1, dual handling,
termination — all shared. This is the experimental control that makes the
comparison valid.
