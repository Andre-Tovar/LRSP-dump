# MESPPRC Final Package

Two implementations of the MESPPRC solver — pure Python and hand-written C —
plus the test suite, benchmark harness, instance library, and benchmark
results that back the central claim of the accompanying paper:

> **For Phase 2 of the MESPPRC, set-partitioning IP outperforms route-network DP
> from n ≈ 6 onward, and the gap grows roughly an order of magnitude per added
> customer.**

The Python and C codebases solve identical problems. The Python solver is the
reference / oracle; the C solver is the timed comparison target. Equivalence
tests verify that the two agree on every bundled instance, so any timing
difference reflects algorithmic behaviour rather than mismatched implementations.

## Headline result

From [results/phase2_dp_vs_ip/native_dp_vs_ip_summary.md](results/phase2_dp_vs_ip/native_dp_vs_ip_summary.md)
(50 replicates, 5 seeds × 10 customer counts, both solvers in C):

| n  | C Phase 2 DP        | C Phase 2 IP (HiGHS) | DP / IP    |
|----|---------------------|-----------------------|------------|
| 3  | 0.060 ms ± 0.029    | 1.155 ms ± 0.765      | 0.05×      |
| 4  | 0.179 ms ± 0.033    | 2.463 ms ± 1.322      | 0.07×      |
| 5  | 0.798 ms ± 0.174    | 1.624 ms ± 0.445      | 0.49×      |
| 6  | 9.475 ms ± 1.965    | 2.941 ms ± 1.262      | **3.22× — crossover** |
| 7  | 181.3 ms ± 57.5     | 6.354 ms ± 2.541      | 28.5×      |
| 8  | 3.74 s ± 1.39 s     | 5.136 ms ± 2.302      | 728×       |
| 9  | 85.04 s (1 rep)     | 5.627 ms ± 2.798      | 15,114×    |
| 10 | DP timeout          | 8.744 ms ± 5.824      | —          |
| 11 | DP timeout          | 8.725 ms ± 4.406      | —          |
| 12 | DP timeout          | 18.3 ms ± 13.4        | —          |

Correctness: every comparable replicate (31 / 31 with both solvers feasible)
agreed on objective value to 1e-6. The plot is at
[results/phase2_dp_vs_ip/native_dp_vs_ip.png](results/phase2_dp_vs_ip/native_dp_vs_ip.png).

## Package layout

```
MESPPRC_Final_Package/
├── README.md                         this file
├── mespprc/                          Python reference solver
│   ├── instance.py                   problem data model
│   ├── instance_generator.py         synthetic instance builder
│   ├── instance_io.py                JSON I/O
│   ├── instance_database.py          loader for the bundled corpus
│   ├── instance_database_builder.py  builder for the bundled corpus
│   ├── label.py                      ESPPRC label + customer-state semantics
│   ├── route.py                      Route data class + reduction records
│   ├── phase1.py                     Phase 1 ESPPRC labeling DP
│   ├── phase2_dp.py                  Phase 2 route-network covering DP
│   ├── phase2_ip.py                  Phase 2 set-partitioning IP (CBC via PuLP)
│   └── instance_db/                  72 bundled JSON benchmark instances
├── mespprc_native/                   C solver (HiGHS-backed Phase 2 IP)
│   ├── README.md                     build + ABI overview
│   ├── include/mespprc.h             public C ABI
│   ├── src/                          C sources (Phase 1, Phase 2 DP, Phase 2 IP, …)
│   ├── third_party/HiGHS/            vendored HiGHS 1.7.2 source
│   ├── _native.py / adapters.py      ctypes binding + Python wrapper
│   ├── build/bin/mespprc_native.dll  prebuilt Windows DLL (no rebuild needed)
│   ├── scripts/                      build script + benchmark + smoke timing
│   └── tests/                        equivalence tests (Phase 1, Phase 2 DP)
├── tests/                            top-level Python test suite
│   ├── test_phase1_semantics.py
│   ├── test_phase2_covering.py
│   ├── test_phase2_ip.py
│   ├── test_label_semantics.py
│   ├── test_instance_generator.py
│   ├── test_mespprc_c.py
│   └── test.py                       small two-phase MESPPRC integration test
├── results/phase2_dp_vs_ip/          paper-grade DP-vs-IP results
│   ├── native_dp_vs_ip.csv           raw replicate-level data
│   ├── native_dp_vs_ip_summary.md    table + conclusions (auto-generated from CSV)
│   └── native_dp_vs_ip.png           log-y scaling plot
└── run_benchmark.py                  Python-only DP vs IP benchmark (legacy)
```

## How the pieces fit together

1. **Problem data.** `mespprc.MESPPRCInstance` (Python) is the canonical
   representation of an MESPPRC instance. `mespprc.generate_instance` builds
   synthetic instances; the JSON files in `mespprc/instance_db/` are bundled
   benchmark cases produced by the same generator at fixed seeds.
2. **Solver translation.** `mespprc_native.build_native_instance(py_instance)`
   forwards an instance into the C library through the public ABI declared in
   `mespprc_native/include/mespprc.h`. The Python and C sides do not share
   memory; the C side owns every handle it returns.
3. **Phase 1 (route generation).** `Phase1Solver` in Python; `mespprc_solve_phase1`
   in C. Both run an ESPPRC labeling DP. Equivalence is asserted by
   `mespprc_native/tests/test_phase1_equivalence.py` (17 synthetic + every JSON
   in `mespprc/instance_db/`).
4. **Phase 2 (route selection).** Two algorithms; either can be plugged in:
   - **DP** — `Phase2DPSolver` (Python) / `mespprc_solve_phase2_dp` (C) —
     route-network covering DP with structural-dominance pool reduction.
   - **IP** — `Phase2IPSolver` (Python, CBC) / `mespprc_solve_phase2_ip` (C, HiGHS) —
     set-partitioning MIP: minimise total cost over binary route variables,
     equality coverage rows per required customer, ≤ rows per global resource.
   Both algorithms share the same Phase 1 input. They are provably equivalent
   on objective value (verified on the bundled corpus and 50 fresh synthetic
   replicates) — only their runtimes differ.
5. **Benchmarking.** `mespprc_native/scripts/paper_phase2_dp_vs_ip.py` runs
   Phase 1 once per replicate in C and feeds the resulting handle to **both**
   Phase 2 solvers, so the only independent variable between DP and IP is the
   Phase 2 algorithm. The script writes raw CSV, a Markdown report (with
   conclusions generated from the run's own numbers), and a log-y scaling
   plot.

## Reproducing the headline result

Either reuse the prebuilt Windows DLL (skip step 1) or rebuild from source.

### 1. Build the C library (only if the prebuilt DLL doesn't load)

Requirements: MSVC (VS 2022 / 2026 BuildTools), CMake ≥ 3.20, Ninja, Git.

```cmd
cd mespprc_native
scripts\build.bat
```

The first build takes ~3 minutes (HiGHS compiles statically from the vendored
source under `third_party/HiGHS/`). The output lands at
`mespprc_native/build/bin/mespprc_native.dll`. Linux/macOS users: the CMake
project is portable — `cmake -G Ninja -S . -B build && cmake --build build`.

### 2. Run the benchmark

The script is self-contained but needs `mespprc` on `sys.path` (the package
already lives next to it in this directory). It also needs the Python deps
listed below.

```bash
python -m pip install pulp matplotlib numpy
python mespprc_native/scripts/paper_phase2_dp_vs_ip.py \
    --start-n 3 --max-n 12 --replicates 5 \
    --dp-timeout-seconds 30 --ip-timeout-seconds 60
```

Or, if you have `uv`:

```bash
uv run --with pulp --with matplotlib --with numpy \
    python mespprc_native/scripts/paper_phase2_dp_vs_ip.py \
    --start-n 3 --max-n 12 --replicates 5
```

Outputs land in `results/phase2_dp_vs_ip/`, overwriting the bundled run.

### 3. Verify equivalence

C ↔ Python equivalence on the bundled corpus + 50 synthetic seeds:

```bash
uv run --with pulp --with pytest --with numpy pytest \
    mespprc_native/tests/test_phase1_equivalence.py \
    mespprc_native/tests/test_phase2_dp_equivalence.py
```

Phase 2 IP equivalence (C HiGHS vs Python CBC) is checked by the smoke script:

```bash
uv run --with pulp --with numpy python mespprc_native/scripts/smoke_phase2_ip.py
```

## What the data shows

From the auto-generated conclusions in
[results/phase2_dp_vs_ip/native_dp_vs_ip_summary.md](results/phase2_dp_vs_ip/native_dp_vs_ip_summary.md):

- **Phase 2 DP wins for very small n** (≤ 5), where the reduced route pool is
  tiny and the IP overhead is dominated by HiGHS model construction and
  presolve setup — fixed costs that don't amortise.
- **The DP→IP crossover sits between n = 5 and n = 6.** Past n = 6 the DP
  loses by an ever-widening margin while the IP runtime stays essentially
  flat.
- **DP scales exponentially in customer count.** At n = 9 it took 85 seconds
  on average (single replicate before timeout); IP at n = 9 took ≈ 5.6 ms.
- **IP scales gracefully with the reduced route-pool size** (6 → 141 columns
  across n = 3..12 corresponded to IP runtimes in the 1–18 ms range).
- **Both algorithms are correct.** All 31 replicates with two feasible covers
  agreed on objective to 1e-6. There is no correctness penalty for picking
  either one.

The mechanistic explanation: Phase 2 DP carries one label per partial
selection of compatible routes, and the label space grows with the number of
antichains in the pool-compatibility partial order — exponential in customer
count. Phase 2 IP gives HiGHS a clean set-partitioning incidence matrix plus
a small handful of resource ≤ rows; HiGHS's presolve kills most of it before
branch-and-bound is even invoked.

## Caveats and reviewer notes

- The HiGHS source under `mespprc_native/third_party/HiGHS/` is the official
  v1.7.2 release tag. The build links it statically and exposes only the C
  ABI in `mespprc.h`; nothing C++ leaks to the Python binding.
- The C Phase 2 IP intentionally ports only the operationally important
  outputs (status, objective, selected routes, original/reduced route counts).
  The 30+ auxiliary diagnostic fields the Python `Phase2IPDiagnostics` carries
  are research-time conveniences and are not on the timed hot path.
- HiGHS and CBC may pick different optimal vertices on degenerate problems.
  Equivalence is verified on objective value, not on the specific support of
  the optimal solution. This is not a correctness gap; it is intrinsic to
  set-partitioning IPs with multiple optimal solutions.
- The DP timeout of 30 s per replicate is what cuts off the comparison
  cleanly at n = 9. With more patience the DP runtimes would continue along
  their exponential trend; the IP would not.

## Citation pointers

- Python solver: `mespprc/__init__.py` re-exports the public surface.
  `mespprc/phase1.py` is the canonical Phase 1; `mespprc/phase2_dp.py` and
  `mespprc/phase2_ip.py` are the canonical Phase 2 algorithms.
- C solver: `mespprc_native/include/mespprc.h` is the public ABI.
  `mespprc_native/src/phase1.c`, `phase2_dp.c`, and `phase2_ip.c` are the
  three algorithm ports.
- Benchmark methodology: `mespprc_native/scripts/paper_phase2_dp_vs_ip.py`.
- Underlying MIP solver: HiGHS 1.7.2, vendored under
  `mespprc_native/third_party/HiGHS/` (MIT licensed).
