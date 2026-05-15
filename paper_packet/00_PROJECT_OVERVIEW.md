# 00 — Project Overview

Plain-language summary of the project, for a research-writing assistant.

## What this project is

This is an operations-research codebase that studies **how the choice of
pricing algorithm inside a column-generation solver affects the solver's
overall performance** — specifically, for the **Location, Routing and
Scheduling Problem (LRSP)**.

It contains complete, working, validated solvers for two problems, each
implemented twice (Python first, then C):

- an **LRSP solver** (the outer problem), and
- an **MESPPRC solver** (the pricing subproblem used inside LRSP).

It also contains a controlled experimental campaign comparing two
interchangeable pricing engines — and a learned hybrid of them — across a
234-cell instance corpus.

## The problems

### LRSP — Location, Routing and Scheduling Problem

LRSP integrates **three decision layers** that are usually studied separately.
All three must be preserved; this is **not** an LRP or a VRP:

1. **Location** — which candidate facilities (depots) to open. Each facility
   has an opening cost and a capacity.
2. **Routing** — how vehicles based at the open facilities visit customers on
   elementary routes (no customer visited twice), subject to vehicle capacity.
3. **Scheduling** — each vehicle has a **duty-time budget**
   (`vehicle_time_limit`). A vehicle may perform several routes ("a multi-trip
   pairing"), but the total time of the pairing must respect the budget. In
   this codebase the scheduling layer is a **continuous duty-time constraint**,
   not a set of discrete time periods. (This matters for terminology — see
   `04_EXPERIMENT_INVENTORY.md` and the `n_periods` note in
   `05_RESULTS_TABLES/`.)

The objective minimizes total cost = facility opening costs + vehicle fixed
costs + routing (travel) costs.

### MESPPRC — Multi-Trip Elementary Shortest Path Problem with Resource Constraints

MESPPRC is the **pricing subproblem** of LRSP. Given a facility and a set of
dual prices from the LRSP master problem, MESPPRC finds minimum-reduced-cost
vehicle work:

- **elementary** — no customer repeated on a route;
- **resource-constrained** — vehicle capacity (a local resource) and duty time
  (a global resource);
- **multi-trip** — the answer is not a single route but a *pairing*: a set of
  routes one vehicle executes in sequence under one shared duty-time budget.

MESPPRC is solved in **two phases**:

- **Phase 1** — generate individual elementary routes with negative reduced
  cost, via a label-setting dynamic program (an ESPPRC labeling DP).
- **Phase 2** — combine Phase-1 routes into a feasible multi-trip pairing.
  Phase 2 is a small **set-partitioning problem over the Phase-1 route pool**,
  and it is exactly here that the two competing engines live.

## Column generation

LRSP is solved by **Dantzig-Wolfe column generation**:

1. A **restricted master problem (RMP)** is a set-partitioning LP whose
   variables are facility-open variables and route/pairing "columns".
2. Solving the RMP LP yields **dual prices** for its constraints.
3. The duals are pushed into the **pricing subproblem** (MESPPRC, one per
   facility), which searches for new columns with negative reduced cost.
4. New columns are added to the RMP; the loop repeats until no
   negative-reduced-cost column exists (LP optimality), then a final integer
   master solve produces the integer solution.

Column generation is the standard exact framework for routing problems with an
exponential number of route variables. The pricing subproblem is the
computational bottleneck, which is why the pricing-engine choice is the object
of study.

## The two pricing engines (and the hybrid)

Both engines run the **same Phase 1**. They differ only in **Phase 2**:

- **DP pricing** — Phase 2 is solved by a **route-network covering dynamic
  program**. It carries one label per partial selection of mutually
  compatible (customer-disjoint) routes. Its cost grows with the **number of
  antichains in the route-compatibility order** — i.e. roughly exponentially in
  the size of the Phase-1 route pool.

- **IP pricing** — Phase 2 is solved as an explicit **set-partitioning
  integer program** handed to the HiGHS MIP solver. It has a roughly flat
  per-call overhead (model build + presolve + simplex) and scales mildly with
  the route-pool size.

- **Hybrid pricing** — a **trained per-instance decision tree** that, at the
  start of a solve, inspects cheap instance features (`n_customers`,
  `vehicle_time_limit`, `total_demand`) and commits the whole run to DP or IP.
  The tree was trained on the experiment corpus for 100% in-sample
  winner-prediction accuracy. (An earlier per-call variant, "v3", also exists;
  the deployed engine is the per-instance "v4" tree.)

## The main research question

> **How do design choices in the MESPPRC pricing subproblem affect the
> performance of a full LRSP solver inside a column-generation framework?**

This is deliberately **not** the narrower question "which standalone Phase-2
algorithm is faster." Inside LRSP, the pricing oracle is invoked once per
facility per column-generation iteration — dozens to hundreds of times per
solve. The research interest is how an algorithmic property of the pricing
engine (DP's exponential blow-up vs. IP's flat overhead) **propagates through
the column-generation loop** into end-to-end solver behavior: total runtime,
completion rate within a time budget, scalability in customer count, and the
fraction of work spent in pricing vs. the master.

A secondary question follows naturally: **can a cheap, instance-feature-based
selector capture the best of both engines?** — which the hybrid addresses.

## One-paragraph result preview (do not over-state — see `08_PAPER_CLAIMS_AND_EVIDENCE.md`)

Across the controlled corpus, the IP pricing engine is substantially more
reliable inside LRSP than the DP engine: it completes far more instances within
a fixed time budget and degrades gracefully with customer count, whereas DP is
bimodal — very fast on tiny route pools, then failing to finish once the pool
crosses an instance-dependent threshold. The hybrid selector reproduces the
per-cell winner on the training corpus but, at long time budgets, does not beat
always-IP in deployed completion count. The mechanistic explanation is that
DP's Phase-2 cost is governed by the Phase-1 route-pool size, which the
instance "regime" controls as much as the customer count does.
