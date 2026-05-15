# 10 — Bibliography Seed

Likely sources for the future paper. Entries marked **[in repo]** have a PDF or
LaTeX/`.bib` source inside the repository; verify and complete details from
those before citing. Entries marked **[not in repo]** are standard references
the paper should still engage — confirm exact citation details independently.

A rich BibTeX source already exists in the repo:
`Akca Repo/routingproblems-lrspaper-f27a220c8998/lrs-paper.bib` (the LRSP
paper's own bibliography), plus `…/routingproblems-lrpaper-bd1fa394ac55/lrp.bib`
and `…/routingproblems-zelihadissertation-82e26cec9eb8/proposal-ref.bib`. A
future writer should harvest exact entries from those files.

## Problem definition — LRSP / LRP lineage

- **[in repo]** Z. Akça. *Integrated Location-Routing-and-Scheduling Problems:
  Models and Algorithms.* PhD dissertation, Lehigh University, 2009.
  Path: `Akca Repo/routingproblems-zelihadissertation-82e26cec9eb8/dissert6.pdf`.
  — Primary source of the LRSP definition and the pairing/set-partitioning
  formulation.
- **[in repo]** Z. Akça, R. T. Berger, T. K. Ralphs. *A Branch-and-Price
  Algorithm for Combined Location and Routing Problems* / LRSP working paper.
  COR@L Lab, Lehigh University, 2008. Paths:
  `Akca Repo/routingproblems-lrspaper-f27a220c8998/lrs-paper.tex`,
  bib key `Akca2`/`Akca3` in `lrs-paper.bib`. — Primary method citation.
  *(Verify the final published venue/title from the `.tex`/`.bib`.)*
- **[in repo]** R. T. Berger. *Location-Routing Models for Distribution System
  Design.* PhD dissertation, Northwestern University, 1997.
  (bib key `Berger` in `lrs-paper.bib`.) — Location-routing predecessor.
- **[in repo]** LRP paper. `Akca Repo/routingproblems-lrpaper-bd1fa394ac55/lrp_paper.pdf`.
  — Location-routing background; do not conflate LRP with LRSP.
- **[not in repo]** A survey of Location-Routing (e.g. Nagy & Salhi 2007;
  Prodhon & Prins 2014) — for positioning LRSP within the LRP literature.

## Pricing subproblem — (E)SPPRC and column generation

- **[in repo]** D. Feillet, P. Dejax, M. Gendreau, C. Gueguen. *An Exact
  Algorithm for the Elementary Shortest Path Problem with Resource Constraints:
  Application to Some Vehicle Routing Problems.* Networks 44(3):216–229, 2004.
  Path: `Akca Repo/routingproblems-lrsprefs-d140ed8ae936/espprc.pdf`.
  — Canonical ESPPRC labeling algorithm; basis of Phase 1.
- **[in repo]** M. Desrochers. *An Algorithm for the Shortest Path Problem with
  Resource Constraints.* GERAD Technical Report G-88-27, 1988. (bib key
  `Desrochers`.) — Foundational SPPRC labeling.
- **[in repo]** G. Righini, M. Salani. Bounded bidirectional dynamic
  programming for the RCESPP, 2005–2008. Paths:
  `…/lrsprefs-d140ed8ae936/rcespp-RighiniSalani05.pdf`,
  `SymmetryhelpsRighiniSalani.pdf`.
- **[in repo]** S. Irnich. k-cycle elimination in the SPPRC, 2006.
  Path: `…/lrsprefs-d140ed8ae936/ShortestPathResourcek-cycle_irnich2006.pdf`.
- **[in repo]** J. Desrosiers et al. Column generation for the VRPTW.
  Path: `…/lrsprefs-d140ed8ae936/VRPTW_desroisers.pdf`.
- **[not in repo]** D. Feillet. *A tutorial on column generation and
  branch-and-price for vehicle routing problems* (2010) — useful CG/B&P tutorial.
- **[not in repo]** L. Lozano, A. L. Medaglia. *The Pulse algorithm* for the
  constrained/elementary shortest path problem (≈2013). — Alternative exact
  pricing method; cite as a Phase-1 alternative. *Verify exact title/year.*
- **[not in repo]** Zhu & Wilhelm — staged ("three-stage") approaches to
  resource-constrained routing/scheduling subproblems. — Cite to position the
  two-phase MESPPRC decomposition. *Verify the exact Zhu & Wilhelm paper; the
  packet brief named "TSA" but the precise reference must be confirmed.*

## Multi-trip routing and scheduling

- **[in repo]** É. Taillard, G. Laporte, M. Gendreau. *Vehicle routing with
  multiple use of vehicles*, 1995/96. Paths:
  `…/lrsprefs-d140ed8ae936/taillard95vrpmt.pdf`, `TaillardLaporte96.pdf`.
  — The multi-trip VRP; background for the "multi-trip pairing" aspect.
- **[in repo]** Lin & Chow. Location-Routing-Scheduling, 2002.
  Path: `…/lrsprefs-d140ed8ae936/LRS-LinChow02.pdf`. — Early LRS reference.

## Branch-and-price / decomposition methodology

- **[in repo]** T. K. Ralphs, L. Kopman, W. R. Pulleyblank, L. E. Trotter.
  *On the Capacitated Vehicle Routing Problem.* Mathematical Programming
  94, 2003. (bib key `Ralphs`.)
- **[in repo]** C. Barnhart et al. *Branch-and-price: column generation for
  solving huge integer programs.* (bib key `Barnhart` in `lrs-paper.bib`.)
- **[not in repo]** Desaulniers, Desrosiers, Solomon (eds.), *Column
  Generation*, Springer, 2005 — standard reference text.

## Solver / software

- **[in repo, as vendored source]** Q. Huangfu, J. A. J. Hall et al. *HiGHS*
  open-source LP/MIP solver. Path: `mespprc_native/third_party/HiGHS/`
  (version 1.7.2; license `LICENSE.txt`). — Backs the LRSP master and the
  Phase-2 IP engine. Cite the HiGHS paper (Huangfu & Hall, *Math. Prog.
  Computation*, 2018, and the HiGHS software).
- **[supporting]** PuLP / CBC — used by the Python reference solvers (the
  equivalence oracles), not by the C solver under study.

## Harvesting note

The most efficient path for the future writer: open
`Akca Repo/routingproblems-lrspaper-f27a220c8998/lrs-paper.bib` and reuse its
entries directly — it already contains Feillet, Desrochers, Berger, Ralphs,
Barnhart, and dozens of routing/LRP references in BibTeX form. Add HiGHS,
Pulse, and the chosen Zhu & Wilhelm reference, which postdate that file.
