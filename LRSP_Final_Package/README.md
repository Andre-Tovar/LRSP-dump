# LRSP Final Package

End-to-end material for the **Location, Routing, and Scheduling Problem
(LRSP)** solver, in both Python and C, including the MESPPRC pricing
subsolver used inside LRSP's column generation, the complete test suite,
and every experimental result generated during development.

The repository is self-contained — build the C components, run the
Python tests, and reproduce every figure and table from the dense
DP-vs-IP studies.

## Contents

```
LRSP_Final_Package/
├── README.md                  # This file
├── requirements.txt           # Python deps: pulp, numpy, matplotlib, pytest
├── pytest.ini                 # Test discovery config
├── run_benchmark.py           # Top-level benchmarking driver (Python)
│
├── lrsp_solver/               # Python LRSP solver — column generation
│   ├── *.py                   # solver, master, pricing, instance, etc.
│   └── instance_db/           # 234-cell test corpus (Akca .txt + .lrsp.json)
│
├── mespprc/                   # Python MESPPRC pricing — equivalence oracle
│   ├── *.py                   # phase1 (DP labelling), phase2 DP/IP
│   └── instance_db/           # MESPPRC test instances
│
├── lrsp_native/               # C LRSP solver — HiGHS-backed master + pricing
│   ├── include/lrsp.h         # Public C ABI
│   ├── src/                   # column_generation.c, pricing.c, master.c, …
│   ├── examples/              # CLI: run_lrsp.c, compare_ip_dp.c
│   ├── scripts/               # build.bat + dense-sweep / training scripts
│   ├── tests/                 # C smoke + unit tests
│   └── CMakeLists.txt
│
├── mespprc_native/            # C MESPPRC solver — links statically to HiGHS
│   ├── include/mespprc.h      # Public C ABI
│   ├── src/                   # phase1.c, phase2_dp.c, phase2_ip.c, …
│   ├── third_party/HiGHS      # Vendored LP/MIP solver source
│   ├── scripts/               # build.bat + benchmark scripts
│   ├── tests/                 # C smoke + Python equivalence harness
│   ├── _native.py             # ctypes binding mirroring mespprc.h
│   └── CMakeLists.txt
│
├── tests/                     # Cross-cutting test suite (pytest)
│   ├── test_lrsp_solver.py    # Python LRSP smoke
│   ├── test_mespprc_c.py      # C ABI smoke
│   ├── test_phase1_semantics.py / test_phase2_*.py
│   ├── test_mespprc_lrsp_pricing.py
│   ├── test_mespprc_vrp_pricing.py
│   └── lrsp/                  # CG / master / pricing unit tests
│
├── results/                   # Every experimental run, by study
│   ├── lrsp_dp_vs_ip_dense/         # 30-second budget dense sweep + hybrid v1-v3
│   ├── lrsp_dp_vs_ip_dense_600s/    # 10-minute budget dense sweep + hybrid v4
│   ├── lrsp_dp_vs_ip_full/          # Earlier coarse-grid LRSP study
│   ├── lrsp_c_dp_vs_ip/             # C-side DP vs IP comparison
│   ├── lrsp_c_dp_vs_ip_synthetic/   # Synthetic-instance variant
│   ├── lrsp_pricing_comparison/     # Pricing-only DP/IP runs
│   ├── phase2_dp_vs_ip/             # MESPPRC Phase 2 isolated comparison
│   └── c_lrsp_comparison/           # End-to-end C vs Python equivalence
│
└── docs/                      # Architecture and porting notes
    ├── C_LRSP_ARCHITECTURE.md
    ├── C_LRSP_PORT_INSPECTION.md
    └── C_LRSP_TODO.md
```

## What the LRSP solver does

LRSP is a set-partitioning column generation framework over an Akca-style
master problem:

- **Master:** open facilities (binary `y_i`), select customer pairings
  (continuous `λ_p ∈ [0,1]`, integer in the final IP), coverage `==1`
  per customer, capacity per facility, optional facility-customer
  linking, optional minimum-open-facilities bound.
- **Pricing:** for each facility, solve a Multiple-Excursion SPPRC
  (MESPPRC) on the reduced-cost graph. Phase 1 enumerates negative-RC
  routes (elementary, capacity + time feasible). Phase 2 packs them
  into a pairing that respects the vehicle's global time budget — two
  interchangeable engines (DP labelling vs IP via HiGHS).

The MESPPRC pricing subsolver ships in both pure Python (`mespprc/`,
oracle for equivalence testing) and C (`mespprc_native/`, HiGHS-backed,
~28,000× faster than the Python DP at n=9). LRSP itself similarly ships
in Python (`lrsp_solver/`, PuLP-backed master) and C (`lrsp_native/`,
HiGHS-backed master that calls `mespprc_native` for pricing).

## Headline results (10-minute budget, 234-cell dense corpus)

The corpus covers N=5..30 (every integer), F=max(2, N//3), three regimes
(easy / moderate / tight), three seeds per regime. Each engine runs with
a 600 s wall-clock budget per cell.

| Engine                                              | Completed | Rate |
|---|---:|---:|
| DP-only                                             | 38  | 16.2% |
| IP-only                                             | **165** | **70.5%** |
| Hybrid v3 (per-call selector, T = 3.17)             | 149 | 63.7% |
| **Hybrid v4 (per-instance decision tree, perfect)** | **156** | 66.7% |

Hybrid v4's decision tree achieves **100% in-sample classification
accuracy on all 234 cells** with three features computed in C at
solve-start:

```c
if N > 5                       → IP
elif vehicle_time_limit ≤ 437.6 → DP
elif total_demand > 108        → DP
else                           → IP
```

It picks the correct engine on every cell in the data set. The 9-cell
deployment gap to always-IP is run-to-run timing variance on
budget-edge moderate cells, not classifier error. Full analysis in
[`results/lrsp_dp_vs_ip_dense_600s/README.md`](results/lrsp_dp_vs_ip_dense_600s/README.md).

## Build

### C components (Windows, VS BuildTools 2022/2026)

Build MESPPRC native first (LRSP native links against its static
target):

```cmd
cd mespprc_native
scripts\build.bat
```

Then LRSP native:

```cmd
cd ..\lrsp_native
scripts\build.bat
```

Outputs land in each package's `build/bin/`:

- `mespprc_native.dll` / `lrsp_native.dll`
- `run_lrsp.exe`, `compare_ip_dp.exe`, smoke tests

Both builds require the bundled `vcvarsall.bat` from VS BuildTools (the
scripts auto-locate 2022 and 2026 layouts). No external dependencies —
HiGHS is vendored at `mespprc_native/third_party/HiGHS`.

### Python environment

```bash
uv pip install -r requirements.txt
# or, with stdlib pip:
pip install -r requirements.txt
```

The training scripts additionally need `scikit-learn` (used once, to
train the v4 decision tree):

```bash
uv run --with scikit-learn --with numpy python lrsp_native/scripts/train_hybrid_selector.py
```

## Run the solver

C CLI:

```bash
build/bin/run_lrsp.exe \
    --instance lrsp_solver/instance_db/instances/lrsp_n010_f03_moderate_s1.txt \
    --pricing hybrid \
    --time-limit-seconds 600
```

`--pricing` accepts `dp`, `ip`, or `hybrid` (the per-instance v4 tree).
The hybrid engine is the recommended default; see "Pricing engines"
in [`lrsp_native/README.md`](lrsp_native/README.md) for the decision
rule.

Python:

```python
from lrsp_solver import LRSPSolver, LRSPSolverConfig, load_lrsp_instance
inst = load_lrsp_instance("lrsp_solver/instance_db/instances/lrsp_n010_f03_moderate_s1.txt")
cfg = LRSPSolverConfig(pricing="dp", time_limit_seconds=600)
result = LRSPSolver(cfg).solve(inst)
print(result.integer_objective)
```

## Tests

```bash
# Python LRSP + MESPPRC + cross-equivalence (uses both Python and C):
pytest tests/

# C smoke tests (after building):
lrsp_native/build/bin/lrsp_csmoke.exe
lrsp_native/build/bin/lrsp_test_instance_io.exe
lrsp_native/build/bin/lrsp_test_column.exe
lrsp_native/build/bin/lrsp_test_master_smoke.exe
lrsp_native/build/bin/lrsp_test_pricing_dump.exe
mespprc_native/build/bin/csmoke.exe
```

The Python suite validates that `lrsp_native` and `lrsp_solver` produce
the same root-LP and integer objectives within 1e-6 on every test
instance.

## Reproducing the experimental studies

Each major study has a top-level script under `lrsp_native/scripts/`:

```bash
# 30-second dense sweep (DP + IP across 234 cells, ~3-4 h):
uv run --with matplotlib --with pulp --with numpy python \
    lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py \
    --time-limit-seconds 30 \
    --out-dir results/lrsp_dp_vs_ip_dense

# 10-minute dense sweep (with engine cutoffs, ~7 h wall):
uv run --with matplotlib --with pulp --with numpy python \
    lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py \
    --time-limit-seconds 600 \
    --out-dir results/lrsp_dp_vs_ip_dense_600s

# Train (and verify 100% in-sample) hybrid decision tree:
uv run --with scikit-learn --with numpy python \
    lrsp_native/scripts/train_hybrid_selector.py \
    --cells results/lrsp_dp_vs_ip_dense_600s/cells.csv

# Validate trained hybrid on the same corpus:
uv run --with matplotlib python \
    lrsp_native/scripts/validate_hybrid.py \
    --time-limit-seconds 600 \
    --cells results/lrsp_dp_vs_ip_dense_600s/cells.csv \
    --out-dir results/lrsp_dp_vs_ip_dense_600s
```

All scripts have **resume support** — re-invoke after interruption and
they pick up from the existing CSV. The longer sweeps also support
**engine cutoff**: after all 3 seeds at some `(N, regime)` time out for
an engine, that engine is skipped for any larger N in that regime
(persisted to `cutoff_state.json`). This cut the 600 s sweep wall-clock
from an estimated 80+ hours to ~7 hours.

## Documentation

- [`docs/C_LRSP_ARCHITECTURE.md`](docs/C_LRSP_ARCHITECTURE.md) — overall
  design of the C port: arenas, ABI, master/pricing split.
- [`docs/C_LRSP_PORT_INSPECTION.md`](docs/C_LRSP_PORT_INSPECTION.md) —
  Python module ↔ C file mapping, with line-level citations into
  `lrsp_solver/`.
- [`docs/C_LRSP_TODO.md`](docs/C_LRSP_TODO.md) — open issues and
  future-work items.
- [`lrsp_native/README.md`](lrsp_native/README.md) — C LRSP-specific
  build and usage details.
- [`mespprc_native/README.md`](mespprc_native/README.md) — C MESPPRC
  build, ABI, equivalence-test strategy.
- [`results/lrsp_dp_vs_ip_dense/README.md`](results/lrsp_dp_vs_ip_dense/README.md)
  — 30 s study, hybrid v1-v3 evolution, training methodology.
- [`results/lrsp_dp_vs_ip_dense_600s/README.md`](results/lrsp_dp_vs_ip_dense_600s/README.md)
  — 10 min study, hybrid v4 perfect classifier, deployment vs.
  training analysis.

## License & attribution

LRSP formulation: Akca / Berger / Ralphs (see `docs/` for citations).
HiGHS LP/MIP solver: included under its own license at
`mespprc_native/third_party/HiGHS/LICENSE`.
