# README — For the Future Research-Writing Assistant

You are about to write a **technical research paper** about this project. This
`paper_packet/` folder is a compressed, curated brief so you do **not** need to
read the entire repository. Read this file first.

## What the paper is about (the one thing to get right)

The paper's thesis is **not** "which Phase-2 algorithm is faster." It is:

> **How do design choices in the MESPPRC pricing subproblem affect the
> performance of a full LRSP solver inside a column-generation framework?**

Every section should connect a *pricing-subproblem property* (DP's
route-pool-size-driven exponential blow-up vs. IP's flat per-call overhead) to
an *LRSP-solver-level outcome* (total runtime, completion rate within a budget,
scalability in N, pricing's share of runtime). If a paragraph compares DP and
IP only as standalone MESPPRC solvers and never closes the loop back to LRSP
column-generation performance, it is off-thesis. The standalone MESPPRC
crossover is *supporting evidence*, not the headline.

## How to use this packet — reading order

1. **`00_PROJECT_OVERVIEW.md`** — what LRSP, MESPPRC, column generation, and the
   three pricing engines are. Start here.
2. **`01_RESEARCH_NARRATIVE.md`** — the project story and *why* a native-code
   reimplementation was necessary (the fairness argument). This motivates the
   whole methodology section.
3. **`02_ALGORITHM_SUMMARY.md`** — the master problem, dual flow, MESPPRC,
   the DP/IP/hybrid engines, and full pseudocode. This is your Methodology.
4. **`08_PAPER_CLAIMS_AND_EVIDENCE.md`** — **the most important file.** The
   claim table is the exact set of statements you are allowed to make, each
   with its evidence and caveats. Treat it as a contract.
5. **`04_EXPERIMENT_INVENTORY.md`** + **`05_RESULTS_TABLES/`** — the experiments
   and the cleaned data. Your Results section is built from
   `05_RESULTS_TABLES/cleaned/` and the `cells.csv` files in `raw/`.
6. **`06_FIGURE_READY_DATA.md`** — the figures to include, with axes and the
   claim each one supports.
7. **`09_LIMITATIONS_AND_FUTURE_WORK.md`** — your Limitations and Future Work
   sections, nearly verbatim.
8. **`03_CODE_ARCHITECTURE_SUMMARY.md`** — only if you need to reference the
   implementation precisely.
9. **`07_KEY_SOURCE_EXCERPTS.md`** + **`10_BIBLIOGRAPHY_SEED.md`** — the
   literature review and references.

## Most important files

- **`08_PAPER_CLAIMS_AND_EVIDENCE.md`** — defines what you may and may not claim.
- **`02_ALGORITHM_SUMMARY.md`** — the technical core.
- **`05_RESULTS_TABLES/`** — the only data you should cite numbers from.

## What NOT to claim (hard rules)

- **Do not invent or extrapolate results.** Every number must trace to
  `05_RESULTS_TABLES/` or to a `summary.md`/`README.md` cited in
  `08_PAPER_CLAIMS_AND_EVIDENCE.md`. No N>30 claims — the corpus stops at N=30.
- **Do not call this LRP or VRP.** It is LRSP. Preserve all three layers —
  **Location** (facility opening), **Routing** (elementary capacitated routes),
  **Scheduling** (a *continuous per-vehicle duty-time budget*, not discrete time
  windows or periods — there is no period count). Do not silently borrow LRP/VRP
  results as if they were LRSP results.
- **Do not declare a winner too strongly.** IP is *more reliable across this
  corpus and these budgets*; DP still wins on small/tight route pools. The
  effect is driven by route-pool size, not customer count alone.
- **Do not oversell the hybrid.** It reproduces the per-cell winner *in-sample*
  on the training corpus; it does **not** beat always-IP in deployed completion
  count at the 600 s budget (156 vs 165). No held-out generalization is shown.
- **Do not claim optimality / exactness.** The solver is column generation +
  one integer master solve — no branch-and-price, no cuts. Reported integer
  objectives are best-from-pool, not certified optima.
- **Do not present the standalone MESPPRC crossover as the main result.** It is
  a mechanism check; the main results are the in-LRSP sweeps.
- **Do not treat the Python `lrsp_pricing_comparison` data as valid runtime
  evidence** — it is language-confounded and hits iteration limits.
- The "DP label space ≈ antichains in the route-compatibility order" argument
  is a **mechanism/hypothesis** consistent with the data, not a proved bound —
  present it as such.

## Suggested paper structure

1. **Introduction** — LRSP; why it integrates Location+Routing+Scheduling; the
   research question (`00`, `01`).
2. **Related work** — LRP/LRSP lineage (Akça, Berger, Ralphs); ESPPRC and
   column generation for routing (Feillet; Desrochers; Righini & Salani)
   (`07`, `10`).
3. **Problem and formulation** — the LRSP set-partitioning master; MESPPRC as
   the pricing subproblem (`02`).
4. **Methodology** — column generation; the two-phase MESPPRC; the DP and IP
   Phase-2 engines; the hybrid selector; the native-code fairness argument
   (`01`, `02`).
5. **Experimental design** — instance corpus, regimes, sweep protocol, the
   shared-control design (`04`).
6. **Results** — DP vs IP inside LRSP: runtime, completion, scalability,
   pricing share; the route-pool-size mechanism; the hybrid (`05`, `06`, `08`).
7. **Discussion** — what the pricing-engine choice means for deploying an LRSP
   solver; when DP is still worth it.
8. **Limitations and future work** (`09`).
9. **Conclusion** — restate the connection: a pricing-subproblem design choice,
   propagated through column generation, governs LRSP solver performance.

## Final reminder

The deliverable is a *rigorous* technical paper. Rigor here means: every claim
in `08` and nothing beyond it; every number from `05`; every figure tied to a
claim per `06`; and the through-line — **MESPPRC pricing choices → LRSP
column-generation performance** — visible from the abstract to the conclusion.
