# 07 — Key Source Excerpts

Concise paraphrases of the literature and archived materials that ground this
project, with exact paths. **Do not over-quote these in the paper** — paraphrase
and cite. Some PDFs (e.g. ESPPRC) are in the repo; some named sources
(TSA, Pulse) are *not* in the repo and are listed because the paper should
still engage them.

## In-repository archived materials (the "Akca / Lehigh" line)

### Akça dissertation — the formulation source

- Path: `Akca Repo/routingproblems-zelihadissertation-82e26cec9eb8/dissert6.pdf`
  (LaTeX: `dissert6.tex`, `sections/`, bib: `proposal-ref.bib`)
- Zeliha Akça, **"Integrated Location-Routing-and-Scheduling Problems: Models
  and Algorithms"**, PhD dissertation, Lehigh University, **2009**.
- Why it matters: this is the **definition of LRSP** this project implements,
  and the source of the set-partitioning ("pairing") master formulation and the
  branch-and-price methodology. Key sections noted in `AKCA_REPO_MAP.md`:
  `sections/problemdefn-dw.tex` (Dantzig-Wolfe / pairing formulation),
  `sections/ch2-soln.tex` (branch-and-price and column generation).
- Core idea to paraphrase: a *pairing* is a set of routes one vehicle executes
  sequentially; pairing cost = route travel cost + vehicle fixed cost;
  customers are covered by pairings, not individual routes; the master couples
  facility-opening decisions to pairing selection.

### Akça, Berger, Ralphs — the LRSP paper

- Path: `Akca Repo/routingproblems-lrspaper-f27a220c8998/` —
  `lrs-paper.tex`, `lrs-paper.bib`, figures (`lrsp-defn*.png`, `pairexp2.png`).
- Working-paper form also recorded: Z. Akça, "Location Routing and Scheduling
  Problems: Models and Algorithms", COR@L Lab, Lehigh University, 2008.
- Why it matters: the peer-facing statement of the LRSP model and a
  branch-and-price algorithm for it. The project's master formulation
  (coverage =1, capacity ≤, facility-customer linking ≤, min-open ≥) is taken
  from here. **Primary citation for the problem and method.**

### Akça branch-and-price source code

- Path: `Akca Repo/routingproblems-lrspcodenew-a2985b2bf3ec/` —
  `exact-bp/`, `exact-1stp-bp/`, `heur-bp/`, `subproblem/`.
- Why it matters: the original exact LRSP branch-and-price implementation
  (C/C++, CPLEX 9.1 + MINTO). It will not compile today, but it is the
  algorithmic ancestor. `exact-1stp-bp/pricingprob.c` informed the control flow
  of this project's `lrsp_native/src/pricing.c`; `e_shortestpath.c` shows the
  two-stage `ESPRC` (route generation) + `ESPPRC_Pairing` (route combination)
  structure that the modern Phase-1/Phase-2 split mirrors.

### LRP paper (background)

- Path: `Akca Repo/routingproblems-lrpaper-bd1fa394ac55/lrp_paper.pdf` (+ `.tex`,
  `lrp.bib`).
- Why it matters: the location-routing predecessor work; useful for positioning
  LRSP as the extension of LRP that adds the scheduling layer. **Background
  citation — do not conflate LRP with LRSP in the paper.**

### Reference PDF library

- Path: `Akca Repo/routingproblems-lrsprefs-d140ed8ae936/`
- Contains, among others:
  - `espprc.pdf` — **Feillet, Dejax, Gendreau, Gueguen (2004)**, "An Exact
    Algorithm for the Elementary Shortest Path Problem with Resource
    Constraints", *Networks* 44(3):216–229. The canonical ESPPRC labeling
    algorithm — the basis of Phase 1.
  - `rcespp-RighiniSalani05.pdf`, `SymmetryhelpsRighiniSalani.pdf` — Righini &
    Salani, bidirectional / bounded label-setting for RCESPP.
  - `ShortestPathResourcek-cycle_irnich2006.pdf` — Irnich, k-cycle elimination
    in SPPRC.
  - `taillard95vrpmt.pdf` — Taillard et al., the multi-trip VRP — background
    for the "multi-trip" aspect of MESPPRC.
  - `VRPTW_desroisers.pdf` — Desrosiers et al., column generation for VRPTW.
  - `LRS-LinChow02.pdf` — an early Location-Routing-Scheduling reference.
  - `PetchSalhi03.pdf`, `PetchSalhi2007.pdf`, `AltinkemerGavish.pdf` — LRP /
    routing background.
- Why it matters: this is effectively the bibliography seed for the pricing-
  algorithm side of the paper. See `10_BIBLIOGRAPHY_SEED.md`.

## Named sources the paper should engage but are NOT in the repo

These are standard references for the algorithm class; they were named in the
packet brief and should be cited even though no PDF exists in the archive
(the archive predates them):

- **Feillet et al. (2004)** — *is* in the repo (`espprc.pdf`); the ESPPRC
  exact labeling algorithm. Phase 1's `phase1.c` is a label-setting DP of this
  family.
- **Zhu & Wilhelm — "Three-Stage Approaches" / TSA** for the multi-trip /
  scheduling-flavored SPPRC. Not in the repo. The paper should position the
  two-phase MESPPRC (route generation then pairing assembly) relative to
  staged decompositions of this kind. *Confidence that this exact citation is
  the intended one: medium — verify the precise Zhu & Wilhelm paper.*
- **Lozano & Medaglia — the "Pulse" algorithm** for the (E)SPPRC /
  constrained shortest path. Not in the repo. Relevant as an alternative exact
  pricing method; the paper can cite it as a route-generation technique the
  DP Phase 1 could be swapped for. *Confidence: medium — verify exact paper.*
- **HiGHS** — Huangfu & Hall et al., the open-source LP/MIP solver. Vendored at
  `mespprc_native/third_party/HiGHS/` (license `LICENSE.txt`). It backs both the
  LRSP master and the Phase-2 IP engine. Cite the HiGHS paper.

## How to use this file

The paper's literature review has two threads:
1. **Problem lineage** — LRP → LRSP, anchored by Akça's dissertation and the
   Akça/Berger/Ralphs paper (both in-repo).
2. **Pricing-algorithm lineage** — ESPPRC labeling (Feillet; Righini & Salani;
   Irnich), multi-trip routing (Taillard), column generation for routing
   (Desrosiers), and alternative exact pricing (Pulse; staged approaches).

Keep paraphrases short. The repository's own value-add is the *fair native-code
comparison of pricing engines inside LRSP* — the literature is context, not the
contribution.
