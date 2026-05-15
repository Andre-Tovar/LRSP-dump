# PROJECT RECONSTRUCTION REPORT

**Repository:** `LRSP-dump-1` (Location, Routing and Scheduling Problem research project)
**Reconstructed:** 2026-05-15
**Purpose of this document:** Recover lost project context from the files
themselves after a computer switch destroyed the original Claude-chat history.
This report is written so a future research assistant or LLM can read it and
immediately understand what the repository contains, what is final, what is
reference material, and what remains to be done before charting / paper writing.

> **Method note.** Every claim below is grounded in repository evidence
> (source files, READMEs, `docs/`, result CSVs, build artefacts). Where the
> evidence is incomplete or ambiguous, the text says so explicitly. This was an
> exploration-only pass: **no solver code was modified.**

---

## 1. Executive Summary

### What this repository is

This is a research codebase studying **how the choice of MESPPRC pricing
algorithm affects the performance of a full LRSP solver inside a column
generation framework**. It contains complete, working solvers for two problems:

- **LRSP** — the *Location, Routing and Scheduling Problem*. All three layers
  are preserved: facility **location** (open/close decisions), vehicle
  **routing** (elementary routes), and **scheduling** (a vehicle time limit
  enforced as a global resource across multi-trip pairings).
- **MESPPRC** — the *Multi-Trip Elementary Shortest Path Problem with Resource
  Constraints*, used as the **pricing subproblem** inside LRSP column generation.

### What problem it solves

The LRSP solver uses **Dantzig-Wolfe column generation**: a restricted master
problem (set-partitioning over routes/pairings plus facility-opening variables)
is solved as an LP, its duals feed a per-facility MESPPRC pricing problem, and
negative-reduced-cost columns are added until LP optimality, followed by a
final integer master solve. The MESPPRC pricing is itself two-phase: **Phase 1**
generates elementary routes via label-setting DP; **Phase 2** combines routes
into a feasible multi-trip vehicle pairing — and Phase 2 can be solved either
by a **DP** (route-network covering) or by an **IP** (set-partitioning via
HiGHS). The central experiment compares those two Phase 2 engines.

### Current final deliverables

| Deliverable | Location | State |
|---|---|---|
| Final C LRSP solver | [`lrsp_native/`](lrsp_native/) | Built, validated against Python |
| Final C MESPPRC solver/pricing engine | [`mespprc_native/`](mespprc_native/) | Built, validated against Python |
| Python LRSP solver (reference oracle) | [`lrsp_solver/`](lrsp_solver/) | Active — equivalence oracle + experiment driver |
| Python MESPPRC solver (reference oracle) | [`mespprc/`](mespprc/) | Active — equivalence oracle |
| Curated handoff bundle (LRSP) | [`LRSP_Final_Package/`](LRSP_Final_Package/) | Snapshot for distribution |
| Curated handoff bundle (MESPPRC) | [`MESPPRC_Final_Package/`](MESPPRC_Final_Package/) | Snapshot for distribution |
| Experiment outputs | [`results/`](results/) | Several complete sweeps with figures |

### Key locations at a glance

- **Main C LRSP solver:** [`lrsp_native/src/`](lrsp_native/src/) — entry point [`lrsp_native/src/solver.c`](lrsp_native/src/solver.c), CG loop [`lrsp_native/src/column_generation.c`](lrsp_native/src/column_generation.c).
- **Main C MESPPRC solver:** [`mespprc_native/src/`](mespprc_native/src/) — [`phase1.c`](mespprc_native/src/phase1.c), [`phase2_dp.c`](mespprc_native/src/phase2_dp.c), [`phase2_ip.c`](mespprc_native/src/phase2_ip.c).
- **IP pricing engine:** Phase 2 IP in [`mespprc_native/src/phase2_ip.c`](mespprc_native/src/phase2_ip.c) (HiGHS-backed set partitioning); LRSP-side adapter in [`lrsp_native/src/pricing.c`](lrsp_native/src/pricing.c).
- **DP pricing engine:** Phase 2 DP in [`mespprc_native/src/phase2_dp.c`](mespprc_native/src/phase2_dp.c) (route-network covering DP); LRSP-side adapter in [`lrsp_native/src/pricing.c`](lrsp_native/src/pricing.c).
- **Hybrid pricing:** **Yes, it exists.** `LRSP_PRICING_HYBRID` in [`lrsp_native/include/lrsp.h`](lrsp_native/include/lrsp.h); per-instance decision-tree selector in [`lrsp_native/src/column_generation.c`](lrsp_native/src/column_generation.c) (`lrsp_hybrid_select_engine`).
- **Tests:** C tests in [`lrsp_native/tests/`](lrsp_native/tests/) and [`mespprc_native/tests/`](mespprc_native/tests/); Python tests in [`tests/`](tests/) and [`tests/lrsp/`](tests/lrsp/).
- **Experiment outputs:** [`results/`](results/) (8 sub-folders) and [`Test Results/`](Test%20Results/).

### What the repository is ready to do next

The solvers are **built and validated**; multiple full-sweep experiments have
**already been run** with CSVs and figures on disk. The repository is ready for
the **analysis / charting / paper-writing phase**, not further solver
development. The largest existing data set ([`results/lrsp_dp_vs_ip_dense_600s/`](results/lrsp_dp_vs_ip_dense_600s/),
234-cell sweep) is rich enough to produce research charts immediately.

> **Update (2026-05-15): merge conflicts RESOLVED.** A repo-wide sweep found
> **152 files** with unresolved git merge-conflict markers — the entire
> `mespprc/instance_db/` corpus (72 instance JSONs + manifest), the
> `results/phase2_dp_vs_ip/` CSV + summary, `tests/test_mespprc_c.py`, and the
> mirrored copies under `LRSP_Final_Package/`. In **every** file both conflict
> sides were byte-identical (modulo line endings), so the markers were stripped
> losslessly, keeping the HEAD side. Verified: all 72 instance JSONs + manifest
> parse; `native_dp_vs_ip.csv` parses; `mespprc_native` Phase 1 equivalence
> tests pass 18/18; LRSP Python tests pass 31/31. One residual *pre-existing*
> issue unrelated to conflicts: `tests/test_mespprc_c.py` imports the obsolete
> `ARCHIVED/mespprc_c` package, which has a broken relative import and is not a
> Python package — that test was never runnable. See Section 8 / Section 12.

---

## 2. Repository Map

The repository root holds **two complete generations of the project**:
the **current top-level project** (with the C ports `lrsp_native/` /
`mespprc_native/`) and a **nested earlier snapshot** [`LRSP-MESPPRC-IP/`](LRSP-MESPPRC-IP/)
that predates the native C ports.

| Folder | Contents | Classification | In active solver? | Use for future analysis? |
|---|---|---|---|---|
| [`lrsp_native/`](lrsp_native/) | Final **C LRSP** solver: CG loop, HiGHS master, pricing adapters, CLI runners, validation/plotting scripts. | **Final / active** | Yes — the LRSP solver | Yes — primary |
| [`mespprc_native/`](mespprc_native/) | Final **C MESPPRC** solver: Phase 1 DP, Phase 2 DP, Phase 2 IP, HiGHS vendored. Linked into `lrsp_native`. | **Final / active** | Yes — the pricing engine | Yes — primary |
| [`lrsp_solver/`](lrsp_solver/) | Python LRSP solver (column generation, master, DP/IP pricing). Pre-dates the C port. | **Active — reference** | Used as equivalence oracle + experiment driver | Yes — oracle + drivers |
| [`mespprc/`](mespprc/) | Python MESPPRC solver (Phase 1, Phase 2 DP, Phase 2 IP). Pre-dates the C port. | **Active — reference** | Equivalence oracle for `mespprc_native` | Yes — oracle |
| [`mespprc_vrp/`](mespprc_vrp/) | A VRP-flavoured variant of the MESPPRC pricing package ("pricing-focused two-phase solver"). | **Experimental / secondary** | No (not linked into LRSP) | Maybe — pricing-variant reference only |
| [`lrsp_native/tests/`](lrsp_native/tests/) | C smoke/unit tests + bundled Akca `.txt` instances. | Final / active | Test only | Validation evidence |
| [`mespprc_native/tests/`](mespprc_native/tests/) | C smoke test + Python equivalence tests for the native MESPPRC. | Final / active | Test only | Validation evidence |
| [`tests/`](tests/), [`tests/lrsp/`](tests/lrsp/) | Python test suite for both Python solvers and the C bindings. | Active | Test only | Validation evidence |
| [`scripts/`](scripts/) | Python experiment runners / discovery / pricing-comparison drivers (operate on the Python solver). | Active | Experiment tooling | Yes — drivers |
| [`results/`](results/) | All experiment outputs: 8 sub-folders of CSVs, figures, summaries. | **Results — final** | No | **Yes — the data set** |
| [`Test Results/`](Test%20Results/) | A small early DP-vs-IP runtime CSV + one PNG. | Results — early/partial | No | Minor — superseded |
| [`docs/`](docs/) | C-LRSP architecture, port-inspection report, TODO list. **High-value context.** | Documentation | No | **Yes — read first** |
| [`LRSP_Final_Package/`](LRSP_Final_Package/) | Curated, self-contained handoff bundle of the LRSP project (Python + C + tests + results). | **Final — distribution snapshot** | No (duplicate) | Reference snapshot |
| [`MESPPRC_Final_Package/`](MESPPRC_Final_Package/) | Curated handoff bundle of the MESPPRC project only. | **Final — distribution snapshot** | No (duplicate) | Reference snapshot |
| [`Akca Repo/`](Akca%20Repo/) | 13 archived Mercurial-style exports of Lehigh routing-problems research (Akca / Samira / Zeliha dissertation, LRP/LRSP papers, instances). | **Archived reference** | No | Yes — formulations, instances, citations |
| [`ARCHIVED/mespprc_c/`](ARCHIVED/mespprc_c/) | An earlier C MESPPRC port (hand-translated). Superseded by `mespprc_native/`. | **Obsolete** | No | Historical only |
| [`ARCHIVED/no-good_LRSP.Solver/`](ARCHIVED/no-good_LRSP.Solver/) | An earlier Python LRSP solver ("no-good" = abandoned) with branch-and-price + branching rules. | **Obsolete / experimental** | No | Reference for B&P ideas |
| [`ARCHIVED/LMSA/`](ARCHIVED/LMSA/) | Unrelated web project (HTML/CSS/JS, images). | **Unrelated — ignore** | No | No |
| [`LRSP-MESPPRC-IP/`](LRSP-MESPPRC-IP/) | A **nested earlier snapshot of the whole project** before the native C ports (has `mespprc_c/`, `mespprc_vrp/`, no `lrsp_native/`/`mespprc_native/`). Carries its own `Akca Repo/`, `ARCHIVED/`, `results/`. | **Archived earlier snapshot** | No | Historical only — do not analyse |
| [`build/`](build/) | Stray top-level CMake/MSVC build tree (Visual Studio generator artefacts). | Build artefact | No | No |
| `run_benchmark.py` | Top-level Python benchmark driver for the **MESPPRC** package (Phase 1 / Phase 2 DP / IP timing). | Active | Experiment tooling | Yes — driver |
| `setup_mespprc_c.py` | Setup helper referencing the old `mespprc_c` port. | Obsolete | No | No |
| `chromedump.*`, `stderr*.txt`, `stdout*.txt`, `chrometest.txt` | Empty / stray scratch files. | Junk | No | No — can be deleted |
| `AKCA_REPO_MAP.md`, `README.md`, `REASSEMBLY.md` | Root docs (see below). | Documentation | No | Yes |

### Notes on the root documentation files

- [`README.md`](README.md) — **mislabelled at the root**: its content is actually
  the `mespprc_native` README (it opens "# mespprc_native"). Useful, but it
  documents the C MESPPRC package, not the whole repo.
- [`REASSEMBLY.md`](REASSEMBLY.md) — explains that three >100 MB files were split
  into `.part-*` chunks for GitHub: the two HiGHS static libs and the `.git.bak`
  pack file. **Reassembly is required before a from-scratch C build that reuses
  the prebuilt libs** (or just rebuild HiGHS from the vendored source).
- [`AKCA_REPO_MAP.md`](AKCA_REPO_MAP.md) — a detailed map of the archived Akca
  LRSP code and how it maps onto this project's Python modules. **Essential
  reading for the archive section.**

### Redundancy / generations summary

There are effectively **three copies** of much of the code:

1. **Top-level current project** — authoritative, has the native C ports.
2. **`LRSP_Final_Package/` + `MESPPRC_Final_Package/`** — curated handoff
   snapshots of (1), self-contained.
3. **`LRSP-MESPPRC-IP/`** — an older full-project snapshot before the C ports.

For all future analysis, **use the top-level current project**. The Final
Packages are faithful subsets; `LRSP-MESPPRC-IP/` is historical.

---

## 3. Final C MESPPRC Package

**Location:** [`mespprc_native/`](mespprc_native/)

### Source and header files

| File | Role |
|---|---|
| [`include/mespprc.h`](mespprc_native/include/mespprc.h) | Public C ABI — opaque handles, accessors |
| [`src/internal.h`](mespprc_native/src/internal.h) | Shared internal struct definitions |
| [`src/arena.c`](mespprc_native/src/arena.c) | Per-solve bump allocator |
| [`src/bitset.c`](mespprc_native/src/bitset.c) | Customer-state bitset |
| [`src/instance.c`](mespprc_native/src/instance.c) | Instance lifecycle + CSR adjacency |
| [`src/phase1.c`](mespprc_native/src/phase1.c) | **Phase 1** — ESPPRC label-setting DP (route generation), ~55 KB |
| [`src/phase2_dp.c`](mespprc_native/src/phase2_dp.c) | **Phase 2 DP** — route-network covering DP, ~62 KB |
| [`src/phase2_ip.c`](mespprc_native/src/phase2_ip.c) | **Phase 2 IP** — set-partitioning IP via HiGHS, ~30 KB |
| [`src/api.c`](mespprc_native/src/api.c) | `mespprc_struct_sizes()`, `mespprc_version()` |
| [`_native.py`](mespprc_native/_native.py) | ctypes binding mirroring the C ABI |
| [`adapters.py`](mespprc_native/adapters.py) | High-level Python wrappers |
| [`third_party/HiGHS/`](mespprc_native/third_party/HiGHS/) | Vendored HiGHS 1.7.2 (built statically) |

### Algorithmic variants

- **Phase 1 (route generation):** elementary shortest-path with resource
  constraints, solved by a label-setting dynamic program. Shared by both
  Phase 2 engines.
- **Phase 2 DP** ([`phase2_dp.c`](mespprc_native/src/phase2_dp.c)): covers the
  required customers by combining Phase 1 routes; carries one label per partial
  selection of compatible (customer-disjoint) routes. Label space is
  exponential in route-pool size.
- **Phase 2 IP** ([`phase2_ip.c`](mespprc_native/src/phase2_ip.c)): hands HiGHS
  a set-partitioning matrix (binary route variables, equality coverage rows,
  resource `≤` rows). Flat per-call overhead, scales mildly with route count.

### IP vs DP — where each lives, how they differ

- **IP engine:** [`mespprc_native/src/phase2_ip.c`](mespprc_native/src/phase2_ip.c),
  exposed as `mespprc_solve_phase2_ip`. Uses the HiGHS C API
  (`Highs_passMip`, `Highs_run`, …).
- **DP engine:** [`mespprc_native/src/phase2_dp.c`](mespprc_native/src/phase2_dp.c),
  exposed as `mespprc_solve_phase2_dp`.
- **Difference:** both solve the same Phase 2 set-partitioning problem over the
  Phase 1 route pool; DP scales exponentially with pool size, IP nearly flat.
  The standalone DP-vs-IP crossover is **between n=5 and n=6** (see
  [`README.md`](README.md) timing table).

### Build

CMake + Ninja, driven on Windows by [`mespprc_native/scripts/build.bat`](mespprc_native/scripts/build.bat):

```bat
cd mespprc_native
scripts\build.bat                :: configure + build
scripts\build.bat reconfigure    :: drop CMakeCache and reconfigure
scripts\build.bat clean          :: rm -rf build/
```

Artefact: `mespprc_native/build/bin/mespprc_native.dll`. First build also
compiles HiGHS from source (~3 min). On Linux/macOS:
`cmake -G Ninja -S . -B build && cmake --build build`.

### Run

Through the Python binding (`import mespprc_native`):

```python
routes = mespprc_native.solve_phase1(instance)
dp     = mespprc_native.solve_phase2_dp(instance)
ip     = mespprc_native.solve_phase2_ip(instance)
tim    = mespprc_native.time_phase2_dp_vs_ip(instance)   # ms-resolution
```

Standalone C smoke test: `mespprc_native/build/bin/mespprc_csmoke.exe`.

### Inputs / outputs

- **Input:** an `mespprc.MESPPRCInstance` (same object the Python solver
  consumes), bridged into C by `build_native_instance`.
- **Output:** route lists / Phase 2 status, total cost, selected route indices,
  original/reduced route counts.

### Tests

[`mespprc_native/tests/`](mespprc_native/tests/) — `csmoke.c`,
`test_foundation.py`, `test_phase1_equivalence.py`,
`test_phase2_dp_equivalence.py`. Phase 1 verified against the Python
`Phase1Solver` on 17 synthetic instances + every JSON in `mespprc/instance_db/`;
Phase 2 verified against `Phase2IPSolver`.

### Standalone or tied into LRSP?

**Both.** `mespprc_native` is a self-contained library with its own ABI and
test suite, **and** it is pulled into `lrsp_native` via
`add_subdirectory(../mespprc_native)` to serve as the LRSP pricing engine.

---

## 4. Final C LRSP Package

**Location:** [`lrsp_native/`](lrsp_native/)

### Source and header files

| File | Role |
|---|---|
| [`include/lrsp.h`](lrsp_native/include/lrsp.h) | Public C ABI; defines `lrsp_pricing_method_t` {`DP`,`IP`,`HYBRID`} |
| [`src/internal.h`](lrsp_native/src/internal.h) | Shared internal types |
| [`src/arena.c`](lrsp_native/src/arena.c) | Arena allocator, FNV hash, euclidean distance |
| [`src/instance.c`](lrsp_native/src/instance.c) | `LRSPInstance` / `Customer` / `Facility` lifecycle |
| [`src/instance_io.c`](lrsp_native/src/instance_io.c) | Akca `.txt` loader (rejects non-LRSP files) |
| [`src/column.c`](lrsp_native/src/column.c) | `Column` + FNV-1a 64-bit signature for dedup |
| [`src/duals.c`](lrsp_native/src/duals.c) | `MasterDuals` lifecycle |
| [`src/master.c`](lrsp_native/src/master.c) | HiGHS-backed restricted master (full Akca formulation) |
| [`src/pricing_graph.c`](lrsp_native/src/pricing_graph.c) | Per-facility dual-adjusted MESPPRC instance builder |
| [`src/pricing.c`](lrsp_native/src/pricing.c) | DP / IP pricing adapters; calls `mespprc_native` |
| [`src/singleton_warmstart.c`](lrsp_native/src/singleton_warmstart.c) | Per-(facility,customer) seed columns |
| [`src/column_generation.c`](lrsp_native/src/column_generation.c) | CG outer loop + **hybrid engine selector** |
| [`src/solver.c`](lrsp_native/src/solver.c) | Public `lrsp_solve` dispatcher |
| [`src/results.c`](lrsp_native/src/results.c) | Result-handle accessors |
| [`src/api.c`](lrsp_native/src/api.c) | Version, status names, `lrsp_solver_config_default` |
| [`examples/run_lrsp.c`](lrsp_native/examples/run_lrsp.c) | CLI runner |
| [`examples/compare_ip_dp.c`](lrsp_native/examples/compare_ip_dp.c) | DP-vs-IP comparator (CSV output) |

### Build

```cmd
cd lrsp_native
scripts\build.bat
```

Artefacts in `lrsp_native/build/bin/`: `lrsp_native.dll`, `run_lrsp.exe`,
`compare_ip_dp.exe`, and five test executables. `lrsp_native` links
`lrsp_native.dll → mespprc_native.dll → highs.lib`. **The build is already
done** — those artefacts are present on disk.

### Run

```cmd
build\bin\run_lrsp.exe --instance tests\p11-f25-v1t1.txt --pricing dp
build\bin\run_lrsp.exe --instance tests\p11-f25-v1t1.txt --pricing ip
build\bin\run_lrsp.exe --instance tests\p11-f25-v1t1.txt --pricing hybrid
build\bin\compare_ip_dp.exe --instance tests\p11-f25-v1t1.txt --out ..\results\c_lrsp_comparison\raw_results.csv
```

CLI flags (from [`run_lrsp.c`](lrsp_native/examples/run_lrsp.c)):
`--instance`, `--pricing dp|ip|hybrid`, `--max-iterations N`,
`--max-cols-per-facility N`, `--no-integer`, `--no-linking`, `--no-min-open`,
`--time-limit-seconds X`, `--verbose`.

### Master problem representation

HiGHS-backed restricted master, **full Akca set-partitioning formulation**
(see [`docs/C_LRSP_ARCHITECTURE.md`](docs/C_LRSP_ARCHITECTURE.md)):

```
variables:  y_j ∈ [0,1] facility-open;  λ_p ∈ [0,1] pairing/route columns
objective:  min Σ opening_cost_j·y_j + Σ pairing_cost_p·λ_p
rows:
  coverage_i :  Σ_{p covers i} λ_p == 1                      per customer
  capacity_j :  Σ_{f(p)=j} d_p·λ_p − Cap_j·y_j ≤ 0           per facility
  link_{i,j} :  Σ_{f(p)=j, i∈p} λ_p − y_j ≤ 0                per (i,j), optional
  min_open   :  Σ_j y_j ≥ K                                  optional
```

All rows added once at create time; columns added incrementally via
`Highs_addCol`. Linking and min-open are **ON by default** (matching the
dissertation formulation); disable with `--no-linking` / `--no-min-open`.

### Column generation

[`src/column_generation.c::lrsp_run_column_generation`](lrsp_native/src/column_generation.c):
build singleton warmstart columns → loop {solve master LP → extract duals →
for each facility call pricing → add columns} → terminate on no-new-columns
(`lp_optimal`) / iteration cap / time cap / master failure → re-solve LP →
optional integer master solve.

### Duals into pricing

[`src/pricing_graph.c`](lrsp_native/src/pricing_graph.c) builds a fresh
`mespprc_instance_t` per facility per iteration with arc costs already adjusted:

```
rc(arc into customer i) = base_travel
                          − coverage_dual[i]
                          − facility_capacity_dual[j]·demand[i]
                          − facility_customer_link_dual[(i,j)]
```

Arcs into the sink keep raw travel cost. A pairing constant
(`vehicle_fixed_cost`) is added to each emitted column's reduced cost.

### How DP / IP / Hybrid pricing are selected

- **DP / IP:** `lrsp_solver_config_t.pricing = LRSP_PRICING_DP | LRSP_PRICING_IP`;
  [`src/pricing.c`](lrsp_native/src/pricing.c) dispatches Phase 2 accordingly.
- **Hybrid:** `LRSP_PRICING_HYBRID`. **Resolved once per run** in
  [`column_generation.c::lrsp_hybrid_select_engine`](lrsp_native/src/column_generation.c)
  — a depth-3 decision tree (see code excerpt in Section 13) that picks DP or
  IP from instance features (`num_customers`, `vehicle_time_limit`,
  `total_demand`). Trained on the dense sweep for 100% in-sample accuracy.
  > Note: `include/lrsp.h` also documents an *older* per-call hybrid keyed on
  > `LRSP_HYBRID_PHASE1_THRESHOLD` (the v3 selector). The **deployed** hybrid
  > is the v4 per-instance tree in `column_generation.c`. This is a minor
  > inconsistency to be aware of — see Section 12.

### Instance formats supported

Akca `.txt` only (see Section 7 for the format). The Python-dict instance
format is **not** loadable by the C side (logged as deferred work in
[`docs/C_LRSP_TODO.md`](docs/C_LRSP_TODO.md)).

### Tests

[`lrsp_native/tests/`](lrsp_native/tests/): `csmoke.c`, `test_column.c`,
`test_instance_io.c`, `test_master_smoke.c`, `test_pricing_dump.c`, plus two
bundled Akca instances `p11-f25-v1t1.txt`, `p11-f30-v1t1.txt`.

### Experiment scripts

[`lrsp_native/scripts/`](lrsp_native/scripts/): `validate_against_python.py`
(C↔Python equivalence), `paper_lrsp_dp_vs_ip.py`,
`paper_lrsp_dp_vs_ip_dense.py`, `paper_lrsp_dp_vs_ip_full.py` (the full sweeps),
`validate_hybrid.py`, `train_hybrid_selector.py`,
`plot_winner_by_pool_size.py`.

---

## 5. Python Solver History

The Python solvers came **first** and are still active as **equivalence
oracles** and **experiment drivers** — they are not obsolete.

### Python LRSP solver — [`lrsp_solver/`](lrsp_solver/)

13 modules / ~2,000 LOC (per [`docs/C_LRSP_PORT_INSPECTION.md`](docs/C_LRSP_PORT_INSPECTION.md)):
`solver.py`, `column_generation.py`, `master_problem.py` (PuLP/CBC),
`instance.py`, `pricing_graph.py`, `pricing_dp.py`, `pricing_ip.py`,
`pricing_interface.py`, `column.py`, `results.py`, `experiment_runner.py`,
`instance_generator.py`, `instance_io.py`, `instance_database*.py`, `utils.py`.

### Python MESPPRC solver — [`mespprc/`](mespprc/)

`instance.py`, `phase1.py`, `phase2_dp.py`, `phase2_ip.py`, `label.py`,
`route.py`, `instance_generator.py`, `instance_io.py`, `instance_database*.py`.

### Python → C mapping

The C ports are **deliberate, file-by-file faithful ports**. The module map is
documented in [`docs/C_LRSP_ARCHITECTURE.md`](docs/C_LRSP_ARCHITECTURE.md) and
[`mespprc_native/README.md`](mespprc_native/README.md). Equivalence is
**verified numerically**: C and Python root-LP / integer objectives match to
~1e-6 (see Section 12). So yes — the C version is a faithful port, and the
Python version is intentionally kept as the **oracle**.

### Are the Python scripts still relevant?

Yes. The Python solver is still used by:
- the C-vs-Python validation harness
  ([`lrsp_native/scripts/validate_against_python.py`](lrsp_native/scripts/validate_against_python.py));
- the experiment/plotting scripts in [`scripts/`](scripts/) and `run_benchmark.py`;
- the pytest suite.

### `mespprc_vrp/` — a third Python variant

[`mespprc_vrp/`](mespprc_vrp/) is a "pricing-focused two-phase solver"
(`Phase1Solver`, `Phase2DPPricingSolver`, `Phase2IPPricingSolver`). It is a
VRP-flavoured sibling of `mespprc/`, **not wired into the LRSP solver**.
Treat it as an experimental pricing variant; verify its role before relying
on it for paper claims.

---

## 6. Archived Reference Material

### `Akca Repo/` — Lehigh routing-problems research archive

Thirteen archived exports (Mercurial-style hash-suffixed folder names). Sizes
and roles:

| Folder | ~Size | Contents / why it matters |
|---|---|---|
| `routingproblems-lrspcodenew-a2985b2bf3ec` | 25 MB | **Most relevant.** Exact & heuristic LRSP branch-and-price C/C++ code: `exact-bp/`, `exact-1stp-bp/`, `heur-bp/`, `subproblem/`. The formulation + pricing reference for this project. |
| `routingproblems-lrspcode-39e47f81716c` | 38 MB | Older LRSP code; `comb_pricing_pro-6/*.txt` Akca instances loadable by the current parser. |
| `routingproblems-zelihadissertation-82e26cec9eb8` | 1.8 MB | **Zeliha Akca's PhD dissertation** (LaTeX + PDF). Set-partitioning pairing formulation (`problemdefn-dw.tex`), B&P methodology (`ch2-soln.tex`). **Cite in paper.** |
| `routingproblems-lrspaper-f27a220c8998` | 348 KB | LRSP paper LaTeX. **Cite.** |
| `routingproblems-lrpaper-bd1fa394ac55` | 1.0 MB | LRP paper LaTeX/PDF. Background citation. |
| `routingproblems-lrsprefs-d140ed8ae936` | 20 MB | Reference PDFs / bibliography for the LRSP line of work. |
| `routingproblems-lrsptables-dee71af66de1` | 549 KB | Result tables from the prior LRSP papers — useful as benchmark comparison targets. |
| `routingproblems-lrspporta-b40d29a4aba0` | 4.0 MB | PORTA polyhedral-analysis material. |
| `routingproblems-lrp_dip_samira-26d8a14d22cf` | 119 KB | Samira's DipPy/COIN-OR LRP solvers (Python). |
| `routingproblems-lrp_dip_instances_samira-96a5850bf5af` | 484 KB | Python-dict-format LRP instances (`Lit_`/`Random_`). Different format — not loaded by the current solver. |
| `routingproblems-lrp_dip_talk_samira-6b675c2704c0` | 1.8 MB | Talk slides / DipPy material. |
| `routingproblems-vrpinforms05-f4c7d7f84c23` | 589 KB | INFORMS 2005 VRP material. |
| `routingproblems-vrplib-4111f7c5a8bf` | 677 KB | VRPLIB benchmark instances. |

> **"Asta"** in the prompt most likely refers to **Akca / Zeliha Akca's**
> dissertation work (the `Akca Repo`). **"Semiha / Samija"** corresponds to
> **Samira** (`...samira...` folders). No separate folders literally named
> "Asta" / "Semiha" / "Samija" exist — this is flagged as a naming
> uncertainty; the archived names are *Akca*, *Samira*, *Zeliha*.

**What was reused:** per [`docs/C_LRSP_ARCHITECTURE.md`](docs/C_LRSP_ARCHITECTURE.md)
and [`AKCA_REPO_MAP.md`](AKCA_REPO_MAP.md), **algorithmic patterns only** — no
archived C/C++ links into the build (it is bound to CPLEX 9.1 + MINTO and a
32-bit Linux toolchain; will not compile in 2026). Specifically:
`exact-1stp-bp/pricingprob.c` informed the control flow of `pricing.c`;
`exact-1stp-bp/InitColGenerator.c` is logged as a future warmstart upgrade;
Akca `.txt` instances are reused as test inputs; the dissertation/papers are
the **formulation grounding and citation source**.

### `ARCHIVED/`

- [`ARCHIVED/mespprc_c/`](ARCHIVED/mespprc_c/) — an earlier C MESPPRC port
  (hand-translated `.c` alongside the `.py` originals). **Superseded by
  `mespprc_native/`.** Historical only.
- [`ARCHIVED/no-good_LRSP.Solver/`](ARCHIVED/no-good_LRSP.Solver/) — an earlier
  Python LRSP attempt (folder name self-describes it as abandoned). Contains
  `branch_and_price.py`, `branching_rules.py`, `lrsp_column_generation.py`,
  `akca_instance_generator.py`, and a nested `mespprc_lrsp/` pricing package.
  Useful **only** as a reference for branch-and-price ideas not yet in v1.
- [`ARCHIVED/LMSA/`](ARCHIVED/LMSA/) — an **unrelated web project**. Ignore.

### Should these be cited in the paper?

- **Cite:** the Zeliha Akca dissertation, the LRSP paper, the LRP paper — they
  define the formulation this project ports and extends.
- **Discuss as prior art:** Akca's exact/heuristic branch-and-price code as the
  baseline LRSP solver this project modernises.
- **Do not cite:** `ARCHIVED/`, `LRSP-MESPPRC-IP/`, `mespprc_c` — internal
  history.

---

## 7. LRSP Instance Data

### Primary corpus — [`lrsp_solver/instance_db/`](lrsp_solver/instance_db/)

- `instances/` holds **369 LRSP instances**, each in **two formats**:
  - `*.txt` — Akca `.txt` format (369 files)
  - `*.lrsp.json` — JSON form (369 files)
- `manifest.json` indexes the corpus.
- Naming: `lrsp_n{NNN}_f{FF}_{regime}_s{seed}` — e.g.
  `lrsp_n005_f02_easy_s1`. Customers **N = 5..30**, facilities
  **F ∈ {2,3,4,5,6,8,10}**, regimes **{easy, moderate, tight}**, seeds **1–3**.
- **These are full LRSP instances**, generated by this project's
  `lrsp_solver/instance_generator.py` (`GeneratorConfig`). They include
  **location** (facility coordinates + opening costs + capacities),
  **routing** (customer coordinates, demands, vehicle capacity), and
  **scheduling** (`vehicle_time_limit`) data.

### Akca `.txt` format (from [`docs/C_LRSP_PORT_INSPECTION.md`](docs/C_LRSP_PORT_INSPECTION.md))

```
line 1: <num_facilities> <num_customers>
line 2: <opening_cost_1> ... <opening_cost_F>
line 3: <vehicle_fixed_cost> <num_vehicles_per_facility>
line 4: <vehicle_capacity> <facility_capacity> <vehicle_time_limit>
lines 5..(4+N):       <id> <x> <y> <service_time> <demand>     (customers)
following F lines:    <id> <x> <y> 0 0                          (facilities)
```

Files lacking the `vehicle_time_limit` field on line 4 are **explicitly
rejected** by both the Python (`instance.py`) and C (`instance_io.c`) loaders —
this guarantees a VRP/LRP instance can never silently be treated as LRSP. The
scheduling layer is therefore mandatory.

### Bundled C-side instances

[`lrsp_native/tests/p11-f25-v1t1.txt`](lrsp_native/tests/p11-f25-v1t1.txt) and
`p11-f30-v1t1.txt` — Akca instances used in the equivalence tests.

### Akca-origin instances

Additional Akca `.txt` instances live under
`Akca Repo/routingproblems-lrspcode*/.../comb_pricing_pro-6/` and
`exact-1stp-bp/` (e.g. `p01-f25-v1t1.txt`) — loadable directly.

### Provenance summary

- The 369-instance corpus is **synthetic, generated by this project** with
  `lrsp_solver.GeneratorConfig` regime knobs (β_v, β_f, γ_t).
- A handful of `p*-f*-v*t*.txt` instances are **from the Akca archive**.
- The Python-dict LRP instances under
  `Akca Repo/...lrp_dip_instances_samira.../` are **LRP, not LRSP**, and are
  not used.

### MESPPRC instances

[`mespprc/instance_db/instances/`](mespprc/instance_db/) — **73 JSON files**,
named `mespprc_n{NNN}_{regime}_s{seed}.json` (N=4..~12). Used for MESPPRC
standalone tests / Phase 1 equivalence.

---

## 8. Experiment and Test Results

All experiment outputs live under [`results/`](results/) (8 sub-folders) and
[`Test Results/`](Test%20Results/). Detailed inventory below; see also
`PROJECT_EXPERIMENT_INVENTORY.md`.

| Folder | What it is | Key files | Compares | Complete? |
|---|---|---|---|---|
| [`results/lrsp_dp_vs_ip_full/`](results/lrsp_dp_vs_ip_full/) | **Full sparse sweep** — 180 cells (20 (N,F) × 3 regimes × 3 seeds), 60 s budget | `raw_results.csv` (161 rows), `summary.md`, 8 figures | DP vs IP | Yes — complete with figures |
| [`results/lrsp_dp_vs_ip_dense/`](results/lrsp_dp_vs_ip_dense/) | **Dense sweep** — 234 cells (N=5..30 each, F=max(2,N//3)), 30 s budget; hybrid-selector training set | `raw_results.csv` (193 rows), `cells.csv` (234 rows, 25+ feature cols), 7 figures, `hybrid_*` | DP vs IP vs Hybrid | Yes |
| [`results/lrsp_dp_vs_ip_dense_600s/`](results/lrsp_dp_vs_ip_dense_600s/) | **Dense sweep at 600 s budget** — same 234 cells; the definitive long-budget data | `raw_results.csv` (203 rows), `cells.csv`, `hybrid_validation*`, 6 figures, `README.md`, `summary.md` | DP vs IP vs Hybrid v3/v4 | Yes — richest data set |
| [`results/lrsp_dp_vs_ip_dense/`] (dense, 30s) hybrid files | hybrid validation/selector | `hybrid_validation*.csv/.md`, `hybrid_selector.md` | Hybrid | Yes |
| [`results/lrsp_c_dp_vs_ip/`](results/lrsp_c_dp_vs_ip/) | C LRSP DP-vs-IP on the Akca `p11-*` instances | `raw_results.csv`, `scaling.png`, `summary.md` | DP vs IP | Small / smoke |
| [`results/lrsp_c_dp_vs_ip_synthetic/`](results/lrsp_c_dp_vs_ip_synthetic/) | C LRSP DP-vs-IP on synthetic instances | `raw_results.csv`, `scaling.png`, `summary.md` | DP vs IP | Small |
| [`results/lrsp_c_comparison/`](results/c_lrsp_comparison/) (`c_lrsp_comparison`) | Output of `compare_ip_dp.exe` | `raw_results.csv` | DP vs IP | Single-instance |
| [`results/lrsp_pricing_comparison/`](results/lrsp_pricing_comparison/) | Python LRSP pricing comparison over discovered Akca instances | `comparison_results.csv/.json`, `comparison_table.txt`, `instance_inventory.json`, `synthetic/` microbench | IP vs DP | Partial (iteration_limit on some) |
| [`results/phase2_dp_vs_ip/`](results/phase2_dp_vs_ip/) | **Standalone MESPPRC** Phase 2 DP-vs-IP (native) | `native_dp_vs_ip.csv` (50 rows), `.png`, `_summary.md` | DP vs IP | Conflict markers removed 2026-05-15; CSV parses cleanly |
| [`Test Results/`](Test%20Results/) | Early DP-vs-IP runtime CSV + PNG | `lrsp_dp_dp_vs_dp_ip_runtime.csv`, `.png` | DP variants | Superseded |

### Metrics in the result CSVs

`raw_results.csv` (full/dense sweeps) columns:
`instance, pricing, customers, facilities, regime, seed, status, iterations,
pricing_calls, total_columns, seed_columns, phase1_route_columns,
phase2_pairing_columns, max_routes_per_column, avg_routes_per_pairing,
reached_optimality, total_seconds, master_seconds, pricing_seconds,
root_lp_objective, integer_objective, open_facilities`.

`cells.csv` (dense sweeps) is the **paired** table: one row per cell with
~25 instance/regime **features** + per-engine **outcomes** (dp_*/ip_* timings,
columns, objectives) + training labels (`winner`, `dp_useful`, `ip_useful`,
`speedup_ip_over_dp`, `log_speedup`, `avg_pool_per_call`). This is purpose-built
for charting and for training the hybrid selector.

### Headline findings already on disk (from the `summary.md` files)

- **180-cell full sweep:** IP usefully solves 57% of cells, DP 12%, both time
  out on 31%. IP is the more reliable engine at every regime.
- **234-cell dense 600 s sweep:** IP completes 165/234, DP 38, Hybrid v4 156.
  DP collapses exponentially around N≈8–10; IP degrades gracefully.
- **Standalone MESPPRC:** DP-vs-IP Phase 2 crossover between n=5 and n=6.
- **Hybrid:** v4 per-instance decision tree achieves 100% in-sample winner
  prediction; deployed as `--pricing hybrid`.

### Are there enough results to chart?

**Yes.** The dense sweeps (`cells.csv` with features + outcomes + labels) are
already in the exact shape needed for runtime-vs-N, speedup, completion-rate,
winner-by-pool-size, and IP/DP/hybrid comparison charts. Figures already exist;
they can be regenerated/restyled. Gaps: no large-N (>30) data; the Python
`lrsp_pricing_comparison` runs hit `iteration_limit` and are not clean.
(The standalone phase2 CSV's merge-conflict markers were resolved 2026-05-15.)

---

## 9. Metrics Available for Future Charts

Available **directly** in `raw_results.csv` / `cells.csv`:

| Metric | Column | Notes |
|---|---|---|
| Total runtime | `total_seconds`, `dp_total_seconds`, `ip_total_seconds` | ✓ |
| Master runtime | `master_seconds`, `*_master_seconds` | ✓ |
| Pricing runtime | `pricing_seconds`, `*_pricing_seconds` | ✓ |
| CG iterations | `iterations`, `*_iterations` | ✓ |
| Pricing calls | `pricing_calls`, `*_pricing_calls` | ✓ |
| Columns generated | `total_columns`, `phase1_route_columns`, `phase2_pairing_columns`, `seed_columns` | ✓ broken down by type |
| Avg pool per call | `avg_pool_per_call`, `*_avg_pool_per_call` | ✓ best DP/IP predictor |
| Objective value | `integer_objective`, `*_integer` | ✓ |
| LP bound (root) | `root_lp_objective`, `*_root_lp` | ✓ |
| Termination / failure status | `status`, `reached_optimality`, `*_completed` | ✓ |
| Instance size | `customers`/`n`, `facilities`/`f` | ✓ |
| Regime / difficulty knobs | `regime`, `bv_factor`, `bf_factor`, `gt_factor`, `gt_slack`, `bv_slack` | ✓ |
| Demand structure | `total_demand`, `mean/max/std_demand`, `demand_to_capacity_ratio` | ✓ (cells.csv) |
| Geometry | `mean/max_cust_nearest_fac_dist`, `mean_cust_cust_dist` | ✓ (cells.csv) |
| Method label | `pricing` / `winner` / `dp_useful` / `ip_useful` | ✓ |
| Speedup | `speedup_ip_over_dp`, `log_speedup` | ✓ |
| Open facilities | `open_facilities` | ✓ |

**Not directly available** (would need re-instrumentation): per-iteration
pricing-time series, max single-call pricing time (only the Python
`lrsp_pricing_comparison` CSV has `max_pricing_time_in_iteration`), scheduling-
period count (the model uses a single vehicle time limit, not discrete periods),
explicit driver/vehicle counts beyond `num_vehicles_per_facility`.

### Charts the existing data supports

- Total / pricing / master runtime vs N, faceted by regime (DP vs IP vs Hybrid).
- DP/IP speedup ratio vs N and vs route-pool size.
- Completion rate vs N per engine (the "DP cliff").
- Columns generated vs N, stacked by type (warmstart / Phase 1 routes / Phase 2 pairings).
- CG iteration count by method.
- Objective agreement: IP vs DP `integer_objective` and `root_lp` parity.
- Winner-by-pool-size scatter (the mechanistic chart).
- Hybrid completion / runtime vs pure IP and pure DP.
- Pricing fraction of total runtime vs N.
- Failure-rate / status breakdown by method and regime.

Most of these **already exist as PNGs** in the result folders — the next phase
is consolidation and restyling, not generation from scratch.

---

## 10. Build and Run Instructions

> Verified from `scripts/build.bat`, the READMEs, and the presence of build
> artefacts on disk. The native libraries **are already built**
> (`lrsp_native/build/bin/lrsp_native.dll`, `run_lrsp.exe`, etc. are present).

### Build the C MESPPRC solver

```cmd
cd mespprc_native
scripts\build.bat
```

### Build the C LRSP solver (also builds mespprc_native + HiGHS)

```cmd
cd lrsp_native
scripts\build.bat
```

If reusing the prebuilt HiGHS libs, first reassemble per
[`REASSEMBLY.md`](REASSEMBLY.md):

```bash
cat lrsp_native/build/bin/highs.lib.part-*   > lrsp_native/build/bin/highs.lib
cat mespprc_native/build/bin/highs.lib.part-* > mespprc_native/build/bin/highs.lib
```

Otherwise the build recompiles HiGHS from `mespprc_native/third_party/HiGHS/`.

### Run unit / smoke tests

```cmd
:: C side
lrsp_native\build\bin\lrsp_csmoke.exe
lrsp_native\build\bin\lrsp_test_column.exe
lrsp_native\build\bin\lrsp_test_instance_io.exe lrsp_native\tests\p11-f25-v1t1.txt
lrsp_native\build\bin\lrsp_test_master_smoke.exe
mespprc_native\build\bin\mespprc_csmoke.exe

:: Python side (from repo root)
pytest
```

### Run IP / DP / hybrid pricing

```cmd
lrsp_native\build\bin\run_lrsp.exe --instance lrsp_native\tests\p11-f25-v1t1.txt --pricing dp
lrsp_native\build\bin\run_lrsp.exe --instance lrsp_native\tests\p11-f25-v1t1.txt --pricing ip
lrsp_native\build\bin\run_lrsp.exe --instance lrsp_native\tests\p11-f25-v1t1.txt --pricing hybrid
```

### Run full-sweep experiments

```bash
python lrsp_native/scripts/paper_lrsp_dp_vs_ip_full.py  --time-limit-seconds 60
python lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py --time-limit-seconds 30
python lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py --time-limit-seconds 600 \
    --out-dir results/lrsp_dp_vs_ip_dense_600s
```

Each sweep script supports `--reanalyze` to rebuild figures/summary from the
existing CSV **without** re-running the solver — useful for the charting phase.

### Validate C against Python

```cmd
uv run --with pulp --with numpy python lrsp_native\scripts\validate_against_python.py ^
    --instance lrsp_native\tests\p11-f25-v1t1.txt --pricing dp
```

### Honesty notes

- The build commands are Windows/MSVC-specific (`build.bat`). The CMake project
  is portable but the Linux/macOS path is untested here.
- A from-scratch build was **not** performed during this reconstruction; the
  commands are taken from the build scripts and the presence of correct
  artefacts on disk, not from a fresh run.

---

## 11. Dependency Summary

### C build

- **Compiler:** MSVC 19.x (VS 2022 / 2026 BuildTools). C11 for the project
  code, C++17 for HiGHS.
- **Build system:** CMake ≥ 3.20 + Ninja (both bundled with VS BuildTools).
- **Git** — needed for the vendored HiGHS source.

### LP / IP solver

- **HiGHS 1.7.2** — vendored at
  [`mespprc_native/third_party/HiGHS/`](mespprc_native/third_party/HiGHS/),
  built **statically**. Used by both the C MESPPRC Phase 2 IP and the C LRSP
  master. **Not optional** for the C path.
- **CBC** (via **PuLP**) — used by the **Python** LRSP master and Python
  Phase 2 IP. The motivation for the C port was precisely to remove the
  CBC-vs-pure-Python language bias from DP-vs-IP timing.
- No GLPK / CPLEX / Gurobi / SCIP dependency in the active code. (The archived
  Akca code depends on CPLEX 9.1 + MINTO — that is why it cannot be built.)

### Python

`requirements.txt`: `matplotlib>=3.7`, `numpy>=1.24`, `pulp>=3.3`,
`pytest>=7.0`. The experiment scripts also use `uv run --with ...`.

### Path / machine configuration

- The ctypes loader in `mespprc_native/_native.py` finds the DLL next to the
  package automatically; `MESPPRC_NATIVE_LIB` overrides it.
- `build.bat` locates VS BuildTools at standard install paths.
- No hard-coded absolute machine paths were observed in the active solver code.
  The Python `lrsp_pricing_comparison` CSV stores relative `instance_path`
  values into `Akca Repo/...` — harmless but repo-relative.

---

## 12. Validation Status

### Tests that exist

- **C MESPPRC:** `csmoke.c`; Python equivalence tests
  `test_phase1_equivalence.py`, `test_phase2_dp_equivalence.py`,
  `test_foundation.py`.
- **C LRSP:** `csmoke.c`, `test_column.c`, `test_instance_io.c`,
  `test_master_smoke.c`, `test_pricing_dump.c`.
- **Python:** `tests/lrsp/` (instance loading, master problem, CG, pricing
  interface, instance generator), plus `tests/test_lrsp_solver.py`,
  `test_mespprc_*`, `test_phase1_semantics.py`, `test_phase2_*.py`,
  `test_label_semantics.py`.

### What appears validated

- **C MESPPRC ↔ Python MESPPRC:** Phase 1 verified on 17 synthetic + all
  `mespprc/instance_db/` instances; Phase 2 verified against the Python IP
  oracle (objective parity to 1e-6). Documented as passing in
  [`mespprc_native/README.md`](mespprc_native/README.md).
- **C LRSP ↔ Python LRSP:** `validate_against_python.py` on `p11-f25-v1t1` and
  `p11-f30-v1t1` — root LP matches to ~1e-6; integer objective matches exactly
  when both column pools admit the same support, else within ~0.1%. Documented
  in [`lrsp_native/README.md`](lrsp_native/README.md).
- **Both DP and IP pricing are exercised** end-to-end (every sweep runs both).
- **Hybrid is validated** — `hybrid_validation*.csv/.md` and the
  600 s README document v3 (per-call) vs v4 (per-instance tree) behaviour.

### Test types

Unit tests (column signature, instance IO), integration tests (master smoke,
CG mechanics, end-to-end solve), cross-language equivalence tests, and
full-sweep experiments. The full sweeps are experiments, not pass/fail tests.

### Validation gaps / issues

1. **Merge conflicts — RESOLVED 2026-05-15.** 152 files (the whole
   `mespprc/instance_db/` corpus, the `phase2_dp_vs_ip` CSV/summary,
   `tests/test_mespprc_c.py`, and `LRSP_Final_Package/` mirrors) had markers;
   all sides were byte-identical, so they were stripped losslessly.
   **Separately**, `tests/test_mespprc_c.py` still cannot run: it imports the
   obsolete `ARCHIVED/mespprc_c` package, whose `__init__.py` has a broken
   relative import (`from ...mespprc_c import instance`) and `ARCHIVED/` has no
   `__init__.py`. This test was never runnable — a pre-existing dead test
   against superseded archived code, not a conflict artefact. It should either
   be deleted or rewritten to target `mespprc_native`.
2. **`.git.bak` history is unavailable** — the pack file is split into
   `.part-*` chunks and not reassembled, so prior commit history could not be
   inspected. The live `.git` has a single commit ("Full workspace dump").
3. **Two hybrid definitions coexist** — the `LRSP_HYBRID_PHASE1_THRESHOLD`
   per-call selector documented in `lrsp.h` vs the deployed per-instance
   decision tree in `column_generation.c`. Not a bug, but a documentation
   inconsistency to reconcile before the paper.
4. No comparison of LRSP results against the **published Akca benchmark
   tables** (`Akca Repo/...lrsptables...`) — the equivalence is C-vs-Python,
   not C-vs-literature.
5. C results were **not re-run** during this reconstruction; validation status
   is taken from the READMEs and the on-disk artefacts.

---

## 13. Research Narrative Reconstruction

### The problem

The **LRSP** integrates three decision layers usually studied separately:
**where to open facilities** (location), **how to route vehicles** from those
facilities to customers (routing), and **how to schedule** multi-trip vehicle
duties under a time limit (scheduling). This project deliberately keeps all
three — the Akca `.txt` loader rejects any instance missing the vehicle time
limit, so the problem can never degrade into a plain LRP or VRP.

### Why MESPPRC is the pricing problem

LRSP is solved by **Dantzig-Wolfe column generation**: the master is a
set-partitioning model over **vehicle pairings** (sequences of routes one
vehicle executes), plus facility-opening variables. Generating an improving
pairing for a facility is exactly a **Multi-Trip Elementary Shortest Path
Problem with Resource Constraints** — elementary (no customer repeated),
resource-constrained (vehicle capacity + the global time limit that encodes
scheduling), multi-trip (a pairing combines several routes). MESPPRC is the
pricing oracle; the LRSP solver's performance is bounded by it.

### Why two pricing engines

MESPPRC pricing here is two-phase: Phase 1 generates routes (label-setting DP),
Phase 2 combines them into a pairing. Phase 2 is itself a small
set-partitioning problem, solvable by **DP** (enumerate compatible route
combinations) or **IP** (hand it to HiGHS). The DP is the "classical" route-
network approach; the IP is the modern MIP-solver approach. **The research
question is not "which standalone Phase 2 solver is faster"** — it is **how that
choice propagates through the full LRSP column-generation loop**, where the
pricing oracle is called once per facility per iteration, dozens of times.

### What this project adds beyond prior MESPPRC work

- A **native C implementation** of both the LRSP solver and the MESPPRC pricing
  engine, so DP-vs-IP timing reflects **algorithmic structure, not language
  overhead** (the prior Python solver compared pure-Python DP against
  CBC-backed IP — an unfair fight the IP always won).
- A **fair, controlled experimental design**: 234 instances spanning N, F, and
  three difficulty regimes, each solved by both engines from identical
  warmstarts and identical masters, so the **only** variable is Phase 2
  dispatch.
- A **hybrid pricing strategy** — a trained per-instance decision tree
  (`lrsp_hybrid_select_engine`) that picks DP or IP from cheap instance
  features:

  ```c
  if (instance->num_customers > 5)             return LRSP_PRICING_IP;
  if (instance->vehicle_time_limit <= 437.64)  return LRSP_PRICING_DP;
  return (total_demand > 108.0) ? LRSP_PRICING_DP : LRSP_PRICING_IP;
  ```

### What the data already shows empirically

From the on-disk `summary.md` / `README.md` files:

- **IP is the reliable default** inside LRSP column generation — it usefully
  solves ~57% of the 180-cell sweep and degrades gracefully with N.
- **DP is fragile and bimodal** — it either finishes in microseconds (tiny
  route pools) or times out entirely. Its cost is driven by the **route-pool
  size**, which the difficulty regime controls more than N does.
- **The crossover is mechanistic:** Phase 2 DP's label space grows with the
  number of antichains in the route-compatibility order — exponential when
  Phase 1 emits many disjoint routes. Phase 2 IP has flat per-call overhead.
- **Hybrid v4** matches IP on the cells IP wins and recovers the handful of
  small-instance cells DP wins; at long budgets the remaining gap is
  timing variance, not selector error.

### What still needs analysis before a paper

- Consolidate the sweeps into one clean, conflict-free data set and one set of
  publication-quality figures.
- Quantify **where in the CG loop** the DP cost lands (pricing fraction vs N)
  and tie it to pairing-column counts.
- Decide the paper's framing of the hybrid result honestly — it is a *correct
  classifier* but not a *wall-clock win* at long budgets.
- Position the result against Akca's published LRSP B&P tables.

This narrative is the bridge from code to paper: the repository's contribution
is **a fair, native-code, fully-controlled study of pricing-engine choice
inside an LRSP column-generation solver**, plus a deployable hybrid selector.

---

## 14. What to Do Next

Prioritised; the next phase is **analysis and writing, not solver development**.

1. **Merge conflicts — DONE (2026-05-15).** All 152 affected files were
   resolved losslessly (both sides identical). Remaining follow-up: decide
   whether to delete or rewrite `tests/test_mespprc_c.py` (it targets the
   obsolete `ARCHIVED/mespprc_c` and is not runnable).
2. **Verify the final experiment outputs.** Confirm
   `results/lrsp_dp_vs_ip_dense_600s/` is the canonical data set; spot-check
   row counts (`raw_results.csv` 203, `cells.csv` 234) and that `cells.csv`
   feature columns are populated.
3. **Standardise result columns.** Produce one unified, documented CSV schema
   across `lrsp_dp_vs_ip_full`, `_dense`, `_dense_600s` so every chart reads
   the same columns. Document the regime knobs and the `winner` label encoding.
4. **Regenerate publication-quality charts** with the `--reanalyze` flag on the
   sweep scripts (no re-solve needed): runtime vs N, speedup, completion rate,
   winner-by-pool-size, columns-by-type, pricing fraction, hybrid vs IP/DP.
5. **Generate per-instance comparison reports** — a table per (N, F, regime)
   showing DP vs IP vs Hybrid total time, status, objective parity.
6. **Summarise IP vs DP vs Hybrid trends** into 4–6 stated findings with the
   mechanistic explanation (pool-size-driven DP exponential).
7. **Identify missing experiments** — e.g. N > 30 scalability, an LRSP-vs-Akca-
   benchmark comparison, per-iteration pricing-time instrumentation if a
   "pricing time series" chart is wanted.
8. **Reconcile the two hybrid definitions** (per-call threshold vs per-instance
   tree) and document the deployed one.
9. **Prepare a paper-oriented technical outline** — problem statement, LRSP
   formulation, MESPPRC pricing, column-generation framework, DP vs IP engines,
   experimental design, results, the hybrid selector, conclusions.
10. **(Optional) Reassemble `.git.bak`** to recover commit history if the
    project timeline matters for the writeup.

---

## 15. Important Files Index

| File path | Category | Status | Purpose | Notes |
|---|---|---|---|---|
| [`lrsp_native/src/solver.c`](lrsp_native/src/solver.c) | Final C LRSP | Final/active | `lrsp_solve` public dispatcher | Thin |
| [`lrsp_native/src/column_generation.c`](lrsp_native/src/column_generation.c) | Final C LRSP | Final/active | CG outer loop + hybrid selector | `lrsp_hybrid_select_engine` here |
| [`lrsp_native/src/master.c`](lrsp_native/src/master.c) | Final C LRSP | Final/active | HiGHS restricted master, Akca formulation | |
| [`lrsp_native/src/pricing.c`](lrsp_native/src/pricing.c) | IP+DP pricing | Final/active | DP/IP pricing adapters into mespprc_native | |
| [`lrsp_native/src/pricing_graph.c`](lrsp_native/src/pricing_graph.c) | IP+DP pricing | Final/active | Dual-adjusted per-facility MESPPRC instance | |
| [`lrsp_native/src/column_generation.c`](lrsp_native/src/column_generation.c) | Hybrid testing | Final/active | Per-instance hybrid decision tree | |
| [`lrsp_native/include/lrsp.h`](lrsp_native/include/lrsp.h) | Final C LRSP | Final/active | Public ABI; `lrsp_pricing_method_t` | Documents older v3 hybrid threshold |
| [`lrsp_native/examples/run_lrsp.c`](lrsp_native/examples/run_lrsp.c) | Final C LRSP | Final/active | CLI runner (`--pricing dp/ip/hybrid`) | |
| [`lrsp_native/examples/compare_ip_dp.c`](lrsp_native/examples/compare_ip_dp.c) | Experiments | Final/active | DP-vs-IP comparator, CSV output | |
| [`mespprc_native/src/phase1.c`](mespprc_native/src/phase1.c) | Final C MESPPRC | Final/active | Phase 1 route generation (label DP) | |
| [`mespprc_native/src/phase2_dp.c`](mespprc_native/src/phase2_dp.c) | DP pricing | Final/active | Phase 2 DP engine | |
| [`mespprc_native/src/phase2_ip.c`](mespprc_native/src/phase2_ip.c) | IP pricing | Final/active | Phase 2 IP engine (HiGHS) | |
| [`mespprc_native/include/mespprc.h`](mespprc_native/include/mespprc.h) | Final C MESPPRC | Final/active | Public ABI | |
| [`lrsp_solver/`](lrsp_solver/) (13 `.py`) | Python LRSP | Active/reference | Python LRSP solver — equivalence oracle | |
| [`mespprc/`](mespprc/) (`.py`) | Python MESPPRC | Active/reference | Python MESPPRC solver — equivalence oracle | |
| [`mespprc_vrp/`](mespprc_vrp/) | Python MESPPRC | Experimental | VRP-flavoured pricing variant | Not in LRSP build |
| [`tests/`](tests/), [`tests/lrsp/`](tests/lrsp/) | Tests | Active | Python test suite | `test_mespprc_c.py` not runnable (targets obsolete `ARCHIVED/mespprc_c`) |
| [`lrsp_native/tests/`](lrsp_native/tests/) | Tests | Final/active | C LRSP smoke/unit tests + instances | |
| [`mespprc_native/tests/`](mespprc_native/tests/) | Tests | Final/active | C MESPPRC tests + equivalence | |
| [`lrsp_native/scripts/paper_lrsp_dp_vs_ip_full.py`](lrsp_native/scripts/paper_lrsp_dp_vs_ip_full.py) | Experiments | Active | 180-cell full sweep driver | |
| [`lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py`](lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py) | Experiments | Active | 234-cell dense sweep driver | `--reanalyze` rebuilds figures |
| [`lrsp_native/scripts/validate_against_python.py`](lrsp_native/scripts/validate_against_python.py) | Tests | Active | C↔Python equivalence harness | |
| [`lrsp_native/scripts/train_hybrid_selector.py`](lrsp_native/scripts/train_hybrid_selector.py) | Hybrid testing | Active | Trains the hybrid threshold/tree | |
| [`lrsp_native/scripts/plot_winner_by_pool_size.py`](lrsp_native/scripts/plot_winner_by_pool_size.py) | Plotting | Active | Winner-by-pool-size figure | |
| [`scripts/run_lrsp_pricing_comparison.py`](scripts/run_lrsp_pricing_comparison.py) | Experiments | Active | Python LRSP IP-vs-DP comparison | |
| [`run_benchmark.py`](run_benchmark.py) | Experiments | Active | MESPPRC Phase 1/2 benchmark driver | |
| [`results/lrsp_dp_vs_ip_dense_600s/`](results/lrsp_dp_vs_ip_dense_600s/) | Results | Final | 234-cell 600 s sweep — richest data | Primary data set |
| [`results/lrsp_dp_vs_ip_dense/cells.csv`](results/lrsp_dp_vs_ip_dense/cells.csv) | Results | Final | Paired feature+outcome table | Hybrid training set |
| [`results/lrsp_dp_vs_ip_full/`](results/lrsp_dp_vs_ip_full/) | Results | Final | 180-cell sweep + 8 figures | |
| [`results/phase2_dp_vs_ip/native_dp_vs_ip.csv`](results/phase2_dp_vs_ip/native_dp_vs_ip.csv) | Results | Final | Standalone MESPPRC Phase 2 DP-vs-IP | Conflict markers resolved 2026-05-15 |
| [`lrsp_solver/instance_db/`](lrsp_solver/instance_db/) | Instances | Final | 369 LRSP instances (`.txt` + `.lrsp.json`) | + `manifest.json` |
| [`mespprc/instance_db/`](mespprc/instance_db/) | Instances | Final | 73 MESPPRC JSON instances | |
| [`docs/C_LRSP_ARCHITECTURE.md`](docs/C_LRSP_ARCHITECTURE.md) | Documentation | Reference | C↔Python module map, call graph | Read first |
| [`docs/C_LRSP_PORT_INSPECTION.md`](docs/C_LRSP_PORT_INSPECTION.md) | Documentation | Reference | Pre-port inspection report | Read first |
| [`docs/C_LRSP_TODO.md`](docs/C_LRSP_TODO.md) | Documentation | Reference | Deferred work / bug history | |
| [`AKCA_REPO_MAP.md`](AKCA_REPO_MAP.md) | Documentation | Reference | Map of the Akca archive | |
| [`README.md`](README.md) | Documentation | Reference | (Actually the mespprc_native README) | Mislabelled at root |
| [`REASSEMBLY.md`](REASSEMBLY.md) | Documentation | Reference | How to reassemble split >100 MB files | |
| [`LRSP_Final_Package/`](LRSP_Final_Package/) | Build/distribution | Final snapshot | Curated LRSP handoff bundle | Duplicate of top-level |
| [`MESPPRC_Final_Package/`](MESPPRC_Final_Package/) | Build/distribution | Final snapshot | Curated MESPPRC handoff bundle | Duplicate |
| [`Akca Repo/routingproblems-lrspcodenew-a2985b2bf3ec/`](Akca%20Repo/routingproblems-lrspcodenew-a2985b2bf3ec/) | Archived Akca reference | Archived | Akca exact/heuristic LRSP B&P code | Formulation reference; won't build |
| [`Akca Repo/routingproblems-zelihadissertation-82e26cec9eb8/`](Akca%20Repo/routingproblems-zelihadissertation-82e26cec9eb8/) | Archived Akca reference | Archived | Zeliha Akca PhD dissertation | **Cite in paper** |
| [`Akca Repo/routingproblems-lrspaper-f27a220c8998/`](Akca%20Repo/routingproblems-lrspaper-f27a220c8998/) | Archived Akca reference | Archived | LRSP paper LaTeX | **Cite in paper** |
| [`ARCHIVED/mespprc_c/`](ARCHIVED/mespprc_c/) | Archived reference | Obsolete | Earlier C MESPPRC port | Superseded by mespprc_native |
| [`ARCHIVED/no-good_LRSP.Solver/`](ARCHIVED/no-good_LRSP.Solver/) | Archived reference | Obsolete | Abandoned earlier Python LRSP + B&P | B&P idea reference only |
| [`LRSP-MESPPRC-IP/`](LRSP-MESPPRC-IP/) | Archived reference | Archived snapshot | Older full-project snapshot pre-C-ports | Do not analyse |
| [`build/`](build/), `chromedump.*`, `std*.txt` | Build system / junk | Obsolete | Stray build tree / empty scratch files | Safe to ignore |

---

## Unresolved Questions / Open Items

1. **"Asta / Semiha / Samija" naming.** The archive contains *Akca*, *Zeliha*,
   and *Samira* — no literal "Asta/Semiha/Samija" folders. Assumed to be the
   same people (Akca ≈ "Asta", Samira ≈ "Semiha/Samija"). **Confirm with the
   project owner.**
2. **Merge conflicts** — resolved 2026-05-15 (all sides were identical, no
   decision needed). Open follow-up: delete or rewrite the non-runnable
   `tests/test_mespprc_c.py`.
3. **Two hybrid selectors** — is the per-call `LRSP_HYBRID_PHASE1_THRESHOLD`
   version deprecated in favour of the per-instance tree? The code suggests
   yes; `lrsp.h` comments still describe the old one.
4. **Canonical data set** — is `lrsp_dp_vs_ip_dense_600s` the final sweep, or
   is another run planned? Affects which CSV the charts read.
5. **`.git.bak` history** is not reassembled, so the development timeline could
   not be recovered from commits — only from file content.
6. **`mespprc_vrp/`** — its exact research role (a VRP control case? an
   abandoned branch?) is not documented anywhere found.
7. No C-vs-published-Akca-benchmark validation exists yet — is that wanted for
   the paper?
