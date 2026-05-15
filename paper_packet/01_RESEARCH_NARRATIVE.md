# 01 — Research Narrative

How the project came to be, in chronological order, reconstructed from the
repository (source files, READMEs, `docs/`, results). Where a claim is an
inference rather than a documented fact, it is marked *(inferred)*.

## Origin: the Akca line of LRSP research

The LRSP itself, and the column-generation / branch-and-price methodology used
to solve it, come from prior Lehigh University research — principally the PhD
dissertation of **Zeliha Akça, "Integrated Location-Routing-and-Scheduling
Problems: Models and Algorithms" (2009)**, and the working paper / paper with
**Berger and Ralphs**. That archived material (C/C++ branch-and-price code
bound to CPLEX, dissertation LaTeX, instances) is included in the repository
under `Akca Repo/` and is the **formulation and methodology reference** for
this project. It is *not* part of the build — it targets a CPLEX 9.1 / MINTO /
32-bit-Linux toolchain that will not compile today. See
`07_KEY_SOURCE_EXCERPTS.md`.

This project re-implements that LRSP solver in modern code and uses it as a
testbed for a focused algorithmic question about the pricing subproblem.

## Stage 1 — Python prototypes

The project was first built in **Python**, in two packages:

- `mespprc/` — the MESPPRC pricing solver: Phase 1 label-setting DP, Phase 2
  DP, Phase 2 IP (the IP via the PuLP modeling layer onto the CBC MIP solver).
- `lrsp_solver/` — the LRSP solver: restricted master (PuLP/CBC), per-facility
  pricing-graph construction, column-generation loop, both pricing adapters.

Python was the right choice for getting the formulation correct and for rapid
iteration. These packages still exist and are still used — as **equivalence
oracles** that the C ports are checked against, and as experiment/analysis
drivers.

## Stage 2 — the fairness problem with Python timings

A measurement problem then surfaced, and it is the pivot of the whole project.
In the Python implementation:

- Phase 2 **IP** delegates to **CBC**, a compiled C++ MIP solver, through PuLP.
- Phase 2 **DP** is **pure Python**.

So any DP-vs-IP timing taken from the Python implementation conflates the
**algorithmic** difference with the **language/runtime** difference. The IP
"wins" partly because it is secretly running compiled code while the DP runs
interpreted Python. This makes the Python numbers unusable for answering the
research question honestly. (Documented in `mespprc_native/README.md`.)

## Stage 3 — port MESPPRC to C

To remove the language confound, the MESPPRC solver was ported to **C**
(`mespprc_native/`): Phase 1, Phase 2 DP, and Phase 2 IP, with the IP backed by
the **HiGHS** LP/MIP solver (vendored and statically linked). Now **both**
Phase-2 engines are compiled native code, so a DP-vs-IP timing reflects
algorithmic structure, not interpreter overhead. The Python `mespprc/` package
was kept as the correctness oracle: the C results are checked against it
(Phase 1 on every bundled instance; Phase 2 objective parity to 1e-6).

A standalone MESPPRC benchmark at this stage established a **DP-vs-IP crossover
between n = 5 and n = 6 customers**: below it the tiny-route-pool DP beats
HiGHS's fixed overhead; above it the DP's label space explodes.

## Stage 4 — port LRSP to C

The same fairness argument applies one level up: the Python LRSP **master**
also runs on CBC via PuLP, so a "Python LRSP timing" again hides a compiled
solver. The LRSP solver was therefore ported to C (`lrsp_native/`): a
HiGHS-backed restricted master, plus pricing adapters that call into
`mespprc_native`. The C LRSP solver links `lrsp_native → mespprc_native →
HiGHS`. It was validated against the Python `lrsp_solver` (root-LP and integer
objectives match to ~1e-6 on shared Akca instances).

Now the entire stack — master and both pricing engines — is native code, and a
DP-vs-IP comparison *inside the full LRSP solver* is finally a fair,
apples-to-apples measurement.

## Stage 5 — the controlled experiment

With the fair instrument in hand, the project ran a controlled campaign:

- A synthetic but LRSP-faithful instance generator produces instances spanning
  customer count N, facility count F, and three **difficulty regimes**
  (`easy`, `moderate`, `tight`) that tighten vehicle-capacity, facility-
  capacity, and time-budget slack.
- Each instance is solved twice — once with DP pricing, once with IP pricing —
  from identical warm starts and an identical master, so the **only** thing
  that varies is the Phase-2 engine.
- Sweeps were run at three scopes: a coarse 180-cell "full" sweep (60 s
  budget), a 234-cell "dense" sweep (30 s budget), and the same 234-cell
  corpus at a 600 s budget.

## Stage 6 — the hybrid selector

The dense sweep produced a per-cell feature table (`cells.csv`) labeled with
the winning engine. This was used to train a **hybrid pricing selector**: a
small decision tree that picks DP or IP from cheap instance features. A first
version selected per pricing call (v3); the deployed version selects once per
instance (v4) and achieves 100% in-sample winner-prediction accuracy on the
234-cell corpus. The hybrid is wired into the C LRSP solver as
`--pricing hybrid`.

## Why comparing pricing engines *inside* LRSP matters

A standalone MESPPRC benchmark answers "which Phase-2 algorithm is faster on
one pricing instance." That is necessary but not sufficient, because inside
LRSP the pricing oracle behaves differently than in isolation:

1. **It is called repeatedly.** Pricing runs once per facility per
   column-generation iteration — typically dozens to hundreds of calls per
   solve. A per-call cost difference is amplified by the iteration count.

2. **Its input distribution is generated by the algorithm itself.** The
   reduced-cost graph handed to pricing depends on the current master duals,
   which depend on the columns generated so far. The route pool the DP must
   cope with is an emergent property of the column-generation trajectory, not
   a fixed instance property.

3. **The relevant outcome is end-to-end.** A practitioner deploying an LRSP
   solver cares about total wall-clock and whether the instance finishes
   within a budget — not about isolated Phase-2 microseconds. An engine that is
   slightly slower per call but never blows up can be the better deployment
   choice.

4. **The mechanism transfers but the threshold shifts.** The standalone
   crossover (n ≈ 6) and the in-LRSP completion cliff are the *same*
   phenomenon — the DP label space exploding once the route pool is large
   enough — but inside LRSP the regime knob moves that threshold around in N.
   You can only see that by measuring inside the full solver.

So the project's contribution is not "DP vs IP on MESPPRC." It is a fair,
native-code, fully-controlled study of **how a pricing-subproblem design choice
propagates into LRSP column-generation performance**, plus a deployable
selector that operationalizes the finding. That is the thesis the future paper
should defend.
