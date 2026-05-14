# Akca LRSP Repo Map

## Most Relevant Akca Directories

- `Akca Repo/routingproblems-lrspcodenew-a2985b2bf3ec/exact-bp`
  - Best reference for the full exact LRSP branch-and-price implementation.
- `Akca Repo/routingproblems-lrspcodenew-a2985b2bf3ec/heur-bp`
  - Same overall structure, but tuned for heuristic branch-and-price.
- `Akca Repo/routingproblems-lrspcodenew-a2985b2bf3ec/subproblem`
  - Standalone ESPPRC/MESPPRC-style sandbox; useful for legacy label semantics, but not the full LRSP integration point.
- `Akca Repo/routingproblems-zelihadissertation-82e26cec9eb8/sections/problemdefn-dw.tex`
  - Set-partitioning pairing formulation.
- `Akca Repo/routingproblems-zelihadissertation-82e26cec9eb8/sections/ch2-soln.tex`
  - Branch-and-price and column-generation methodology.

## Akca Exact-BP Architecture

### Master problem

The core master is assembled in `exact-bp/a_mps.cpp`.

Important ingredients:

- `T_j`: facility-open variables
- `Y_{j,p}`: pairing columns
- `V_j`: explicit total-pairing counters
- customer coverage constraints
- facility capacity constraints
- facility-customer relation constraints
  - one row per `(facility, customer)`
  - this is stronger than a single aggregated linking inequality
- lower bound on the number of open facilities
- artificial variables/columns for initial feasibility

The dissertation formulation in `problemdefn-dw.tex` matches this pairing-based view:

- a pairing is a set of routes that one vehicle can execute sequentially
- pairing cost is route travel cost plus vehicle fixed cost
- customers are covered by pairings, not individual routes

### Column generation callback

The pricing integration point is `exact-bp/a_vars.cpp`.

That file:

- reads RMP duals into:
  - `pi`: customer coverage duals
  - `mu`: facility capacity duals
  - `sigma`: facility-customer relation duals
  - `v`: total-pairing-equality duals
- builds one facility-specific reduced-cost network at a time
- applies branching modifications directly to the pricing graph
- calls `Generate_Columns(...)`
- converts returned pairings into new `Y_{j,p}` columns

The reduced-cost arc construction is the key bridge:

- customer-entry arcs subtract customer and facility-assignment dual effects
- sink-return arcs keep pure travel cost
- final pairing reduced cost adds vehicle fixed cost and subtracts the facility-level pairing dual

### Pricing

Akca's pricing dispatcher is `exact-bp/pricingprob.c`.

It tries pricing in layers:

- optional Clarke-Wright heuristic pricing
- label-limited pricing
- exact pricing as fallback
- subset-customer pricing in some modes

The real pricing logic lives in `exact-bp/e_shortestpath.c`.

That file is already two-phase:

- `ESPRC(...)`
  - generates feasible elementary routes with reduced costs
- `ESPPRC_Pairing(...)`
  - combines route labels into a feasible vehicle pairing under the global time limit

So the Akca LRSP solver is not using a single monolithic route generator. It is already doing:

1. route generation
2. route combination into a pairing

That is exactly the seam where your `mespprc_lrsp` package fits best.

### Branching

The main branch-and-price support files are:

- `exact-bp/a_node.cpp`
- `exact-bp/a_divide.cpp`
- `exact-bp/a_bounds.cpp`
- `exact-bp/a_cons.cpp`

These support:

- customer/facility-style logic
- explicit assignment constraints
- Ryan-Foster-style route connectivity branching
- branch-specific pricing graph edits

## Mapping To The Current Python Code

### Already aligned well

- `lrsp_solver/akca_instance_generator.py`
  - Reconstructs Akca-style instances from Cordeau data and dissertation tables.
- `mespprc_lrsp/phase1.py`
  - Natural replacement for Akca's route-generation ESPRC.
- `mespprc_lrsp/phase2_dp.py`
  - Natural replacement for Akca's route-combination pairing phase.
- `mespprc_lrsp/phase2_ip.py`
  - Alternate exact/optimization-based pairing stage.
- `lrsp_solver/pricing_adapter.py`
  - Already acts like the modern Python equivalent of Akca's facility-level pricing wrapper.
- `lrsp_solver/lrsp_column_generation.py`
  - Already has the outer solve-RMP / get-duals / price-new-columns loop.

### Close, but still simplified vs Akca

- `lrsp_solver/lrsp_master.py`
  - Currently has:
    - customer coverage
    - facility capacity
    - one aggregated facility/pairing linking constraint
  - Akca's exact master is stronger and more structured:
    - per-`(facility, customer)` relation constraints
    - lower bound on open facilities
    - explicit total-pairing count rows

- `lrsp_solver/branching_rules.py`
  - Currently branches on customer-to-facility assignment only.
  - Akca also supports route-connectivity style branching that directly alters pricing feasibility.

## Main Gaps To Close If We Want A Faithful Akca-Style LRSP Solver

1. Strengthen the master formulation.
   - Add Akca-style facility-customer relation constraints.
   - Consider adding the lower-bound-on-open-facilities constraint.
   - Decide whether to include the explicit pairing-count rows or fold that effect cleanly into the simplified model.

2. Revisit the facility-level constant term in pricing.
   - Akca subtracts the dual of the total-pairing equality row.
   - The current Python code subtracts the dual of an aggregated linking constraint.
   - Those are not the same model artifact.

3. Expand branching support if dissertation-level fidelity is the goal.
   - Customer-facility branching is a good start.
   - Ryan-Foster or equivalent route-compatibility branching is still missing.

4. Add Akca-style heuristic pricing layers only if needed.
   - label-limit ladder
   - subset-customer pricing
   - Clarke-Wright warm-start pricing

5. Decide how much of Akca's warm-start machinery is worth porting.
   - `.init`, `.ub`, and artificial-column support are present in the C code
   - the Python solver currently uses singleton seed columns instead

## Best Integration Point For Your MESPPRC

The cleanest substitution point is not the standalone `subproblem/mespprc.c`.

The real LRSP integration seam is:

- Akca side:
  - `a_vars.cpp` builds reduced-cost facility graphs
  - `pricingprob.c` calls the pricing oracle
  - `e_shortestpath.c` generates routes and then pairings

- Python side:
  - `pricing_adapter.build_facility_pricing_instance(...)` builds the facility graph
  - `LRSPPhase1Solver` generates negative reduced-cost routes
  - `LRSPPhase2DPPricingSolver` or `LRSPPhase2IPPricingSolver` builds the final pairing column

So the best path is:

1. Keep the Python branch-and-price shell.
2. Use Akca's exact-bp code as the formulation and branching reference.
3. Use your `mespprc_lrsp` package as the pricing oracle.
4. Tighten the Python master until its dual structure matches the Akca formulation closely enough.

## Recommended Next Steps

1. Update `lrsp_solver/lrsp_master.py` to match Akca's stronger master rows before doing a deeper performance comparison.
2. Adjust `lrsp_solver/pricing_adapter.py` so the pairing constant matches the strengthened master's dual structure.
3. Keep `mespprc_lrsp` as the replacement for Akca's `ESPRC + ESPPRC_Pairing` stack rather than trying to wire into `subproblem/mespprc.c`.
4. Only port heuristic pricing layers after the exact master/pricing interface is stable.
