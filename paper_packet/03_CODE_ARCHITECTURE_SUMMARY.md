# 03 — Code Architecture Summary

What is active/final vs. reference/archived, with exact paths. All paths are
relative to the repository root (`LRSP-dump-1/`). Mirror copies also exist in
`LRSP_Final_Package/` (a curated handoff bundle) and `MESPPRC_Final_Package/`;
the **top-level** copies are authoritative for analysis.

## Active final C LRSP package — `lrsp_native/`

The final LRSP solver. Built shared library + CLI runners.

| Path | Role |
|---|---|
| `lrsp_native/include/lrsp.h` | Public C ABI; `lrsp_pricing_method_t` = {DP, IP, HYBRID} |
| `lrsp_native/src/solver.c` | `lrsp_solve` entry-point dispatcher |
| `lrsp_native/src/column_generation.c` | CG outer loop **and** the hybrid engine selector (`lrsp_hybrid_select_engine`) |
| `lrsp_native/src/master.c` | HiGHS-backed restricted master (Akca set-partitioning formulation) |
| `lrsp_native/src/pricing.c` | DP / IP pricing adapters; calls into `mespprc_native` |
| `lrsp_native/src/pricing_graph.c` | Per-facility reduced-cost MESPPRC instance builder (dual → arc costs) |
| `lrsp_native/src/duals.c` | Master dual extraction |
| `lrsp_native/src/singleton_warmstart.c` | Per-(facility,customer) seed columns |
| `lrsp_native/src/column.c`, `instance.c`, `instance_io.c`, `arena.c`, `results.c`, `api.c` | Column/dedup, data model, Akca `.txt` loader, allocator, result accessors, ABI helpers |
| `lrsp_native/examples/run_lrsp.c` | CLI: `--instance --pricing dp|ip|hybrid --time-limit-seconds …` |
| `lrsp_native/examples/compare_ip_dp.c` | Runs both engines on one instance, writes CSV |
| `lrsp_native/CMakeLists.txt`, `lrsp_native/scripts/build.bat` | Build system (CMake+Ninja; `add_subdirectory(../mespprc_native)`) |
| `lrsp_native/README.md` | C LRSP build/usage notes |

## Active final C MESPPRC package — `mespprc_native/`

The final pricing engine. Linked into `lrsp_native`; also usable standalone.

| Path | Role |
|---|---|
| `mespprc_native/include/mespprc.h` | Public C ABI |
| `mespprc_native/src/phase1.c` | **Phase 1** — ESPPRC label-setting DP (route generation) |
| `mespprc_native/src/phase2_dp.c` | **Phase 2 DP engine** — route-network covering DP |
| `mespprc_native/src/phase2_ip.c` | **Phase 2 IP engine** — set-partitioning IP via HiGHS |
| `mespprc_native/src/instance.c`, `arena.c`, `bitset.c`, `api.c` | Data model, allocator, customer bitset, ABI helpers |
| `mespprc_native/third_party/HiGHS/` | Vendored HiGHS 1.7.2 LP/MIP solver (built statically) |
| `mespprc_native/_native.py`, `adapters.py` | ctypes binding + Python wrappers (used by equivalence tests) |
| `mespprc_native/CMakeLists.txt`, `scripts/build.bat` | Build system |
| `mespprc_native/README.md` | C MESPPRC build/ABI/equivalence notes |

## Active Python solvers (reference oracles, still used)

Pre-date the C ports; kept as correctness oracles and analysis drivers.

| Path | Role |
|---|---|
| `lrsp_solver/` (`solver.py`, `column_generation.py`, `master_problem.py`, `pricing_dp.py`, `pricing_ip.py`, `pricing_graph.py`, `instance.py`, …) | Python LRSP solver (PuLP/CBC master). Equivalence oracle for `lrsp_native`. |
| `mespprc/` (`phase1.py`, `phase2_dp.py`, `phase2_ip.py`, `label.py`, `route.py`, …) | Python MESPPRC solver. Equivalence oracle for `mespprc_native`. |
| `mespprc_vrp/` | A VRP-flavoured pricing variant; **not wired into LRSP**. Secondary/experimental. |

## Active experiment scripts

| Path | Role |
|---|---|
| `lrsp_native/scripts/paper_lrsp_dp_vs_ip_full.py` | 180-cell "full" sweep (60 s budget); writes `results/lrsp_dp_vs_ip_full/` |
| `lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py` | 234-cell "dense" sweep (30 s or 600 s); writes `results/lrsp_dp_vs_ip_dense*/`. `--reanalyze` rebuilds figures from CSV without re-solving. |
| `lrsp_native/scripts/train_hybrid_selector.py` | Trains the hybrid decision tree on `cells.csv` |
| `lrsp_native/scripts/validate_hybrid.py` | Runs the hybrid engine across the corpus |
| `lrsp_native/scripts/validate_against_python.py` | C ↔ Python LRSP equivalence harness |
| `lrsp_native/scripts/plot_winner_by_pool_size.py` | Winner-by-route-pool-size figure |
| `mespprc_native/scripts/paper_phase2_dp_vs_ip.py` | Standalone MESPPRC Phase-2 DP-vs-IP benchmark |
| `mespprc_native/scripts/benchmark_phase2_native.py` | Apples-to-apples C Phase-2 DP vs IP timing |
| `scripts/run_lrsp_pricing_comparison.py` | Python-side LRSP IP-vs-DP comparison over discovered instances |
| `run_benchmark.py` (repo root) | MESPPRC Phase 1 / Phase 2 benchmark driver |

## Active tests

| Path | Role |
|---|---|
| `mespprc_native/tests/test_phase1_equivalence.py` | C Phase 1 vs Python `Phase1Solver` on every bundled instance (18 tests; pass) |
| `mespprc_native/tests/test_phase2_dp_equivalence.py` | C Phase 2 DP vs Python oracle |
| `mespprc_native/tests/test_foundation.py`, `csmoke.c` | ABI/foundation smoke |
| `lrsp_native/tests/csmoke.c`, `test_column.c`, `test_instance_io.c`, `test_master_smoke.c`, `test_pricing_dump.c` | C LRSP unit/smoke tests |
| `tests/lrsp/test_column_generation.py`, `test_master_problem.py`, `test_pricing_interface.py`, `test_instance_loading.py`, `test_instance_generator.py` | Python LRSP unit tests (31 pass) |
| `tests/test_phase1_semantics.py`, `test_phase2_covering.py`, `test_phase2_ip.py`, `test_label_semantics.py`, `test_instance_generator.py`, `test.py` | Python MESPPRC unit/semantics tests |
| `lrsp_native/scripts/validate_against_python.py` | Integration: C vs Python objective parity |

## Reference / archived material (NOT in the build)

| Path | What it is | Use |
|---|---|---|
| `Akca Repo/routingproblems-lrspcodenew-*/` | Akca's exact & heuristic branch-and-price C/C++ (CPLEX-bound) | Formulation + B&P methodology reference only — does not compile today |
| `Akca Repo/routingproblems-zelihadissertation-*/` | Zeliha Akça PhD dissertation (2009), LaTeX + PDF | Formulation grounding; cite |
| `Akca Repo/routingproblems-lrspaper-*/`, `…-lrpaper-*/` | LRSP / LRP papers (LaTeX + PDF + `.bib`) | Cite; bibliography source |
| `Akca Repo/routingproblems-lrsprefs-*/` | Reference PDFs (ESPPRC, RCESPP, VRP, LRP) | Bibliography source — see `07_KEY_SOURCE_EXCERPTS.md` |
| `ARCHIVED/mespprc_c/` | Earlier hand-written C MESPPRC port | Obsolete — superseded by `mespprc_native/` |
| `ARCHIVED/no-good_LRSP.Solver/` | Abandoned earlier Python LRSP (branch-and-price prototype) | Obsolete; B&P idea reference |
| `LRSP-MESPPRC-IP/` | Older full-project snapshot pre-C-ports | Historical; do not analyze |

## Documentation already in the repo (read these too)

- `docs/C_LRSP_ARCHITECTURE.md` — C↔Python module map, runtime call graph.
- `docs/C_LRSP_PORT_INSPECTION.md` — pre-port inspection: formulation, instance
  format, archive verdicts.
- `docs/C_LRSP_TODO.md` — deferred work, bug history.
- `AKCA_REPO_MAP.md` — map of the Akca archive.
- `PROJECT_RECONSTRUCTION_REPORT.md` — full repository reconstruction (the
  superset document this packet condenses).
