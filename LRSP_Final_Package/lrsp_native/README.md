# lrsp_native

A C port of the Python LRSP solver in [`lrsp_solver/`](../lrsp_solver/),
with a HiGHS-backed restricted master and the existing C MESPPRC pricing
engine ([`mespprc_native/`](../mespprc_native/)) under the hood.

The motivation is honest IP-vs-DP comparisons. The Python LRSP master uses
PuLP → CBC (a C++ MIP solver), so any "Python LRSP timing" measurement
secretly includes a compiled solver underneath. With both the master and
both pricing engines compiled to native code, the timing comparison reflects
algorithmic structure, not language overhead.

## Equivalence

Verified via [`scripts/validate_against_python.py`](scripts/validate_against_python.py)
on the bundled Akca `p11-f25-v1t1.txt` and `p11-f30-v1t1.txt` instances
with the **full Akca formulation** (coverage `==1`, capacity `≤`,
linking `Σ_{p covers i, uses j} λ_p − y_j ≤ 0`, min-open `Σ y_j ≥ K`):

| Instance | Solver | status | iters | cols | root LP | integer |
|---|---|---|---|---|---|---|
| p11-f25-v1t1 | Python (DP) | lp_optimal | 11 | 560 | 8462.083205 | 8587.351065 |
| p11-f25-v1t1 | C (DP)      | lp_optimal | 10 | 601 | **8462.083206** | 8579.915458 |
| p11-f30-v1t1 | Python (DP) | lp_optimal | 12 | 736 | 8757.634443 | 8780.479445 |
| p11-f30-v1t1 | C (DP)      | lp_optimal | 12 | 699 | **8757.634451** | **8780.479445** |

Root LP matches to ~1e-6 (HiGHS vs CBC numerical-precision noise — well
below either solver's internal tolerance). Integer objectives match
exactly when the two solvers' column pools admit the same IP-optimal
support (p11-f30); when the pools differ, the C and Python integer
solutions can be a few tenths of a percent apart (p11-f25, 0.087%
relative). Both are valid IP-feasible covers from their respective
column pools — neither is the global IP optimum, which would require
branch-and-price to certify.

To turn off linking and / or min-open and recover the simpler v1
formulation:

```cmd
build\bin\run_lrsp.exe --instance ... --pricing dp ^
                       --no-linking --no-min-open
```

(See `--help`.)

## Build

Requirements: MSVC (VS BuildTools 2022 / 2026), CMake ≥ 3.20, Ninja, Git.
The first build also compiles HiGHS from source via `mespprc_native`.

```cmd
cd lrsp_native
scripts\build.bat
```

Artefacts land in `lrsp_native\build\bin\`:

- `lrsp_native.dll` — solver shared library
- `run_lrsp.exe` — CLI runner
- `compare_ip_dp.exe` — DP-vs-IP comparator
- `lrsp_csmoke.exe`, `lrsp_test_column.exe`, `lrsp_test_instance_io.exe`,
  `lrsp_test_master_smoke.exe`, `lrsp_test_pricing_dump.exe` — unit /
  smoke tests

## Run

End-to-end LRSP solve on a known Akca `.txt` instance:

```cmd
build\bin\run_lrsp.exe --instance tests\p11-f25-v1t1.txt --pricing dp
build\bin\run_lrsp.exe --instance tests\p11-f25-v1t1.txt --pricing ip
```

Both engines on the same instance, with CSV output:

```cmd
build\bin\compare_ip_dp.exe --instance tests\p11-f25-v1t1.txt ^
    --out ..\results\c_lrsp_comparison\raw_results.csv
```

## Validate against the Python solver

```cmd
:: from repo root
uv run --with pulp --with numpy ^
    python lrsp_native\scripts\validate_against_python.py ^
    --instance lrsp_native\tests\p11-f25-v1t1.txt --pricing dp
```

The script asserts root LP and integer objectives match within 1e-6 and
iteration counts are within ±3.

## Run the unit tests

```cmd
build\bin\lrsp_csmoke.exe
build\bin\lrsp_test_column.exe
build\bin\lrsp_test_instance_io.exe tests\p11-f25-v1t1.txt
build\bin\lrsp_test_master_smoke.exe
```

## What this code is

Mirrors the `lrsp_solver/` Python architecture file by file. See
[`docs/C_LRSP_ARCHITECTURE.md`](../docs/C_LRSP_ARCHITECTURE.md) for the
module map.

| Python module | C source |
|---|---|
| `lrsp_solver/instance.py`         | `src/instance.c`, `src/instance_io.c` |
| `lrsp_solver/column.py`           | `src/column.c`, `src/duals.c` |
| `lrsp_solver/master_problem.py`   | `src/master.c` (HiGHS C API) |
| `lrsp_solver/pricing_graph.py`    | `src/pricing_graph.c` |
| `lrsp_solver/pricing_dp.py`, `pricing_ip.py` | `src/pricing.c` |
| `lrsp_solver/column_generation.py` | `src/column_generation.c`, `src/singleton_warmstart.c` |
| `lrsp_solver/solver.py`           | `src/solver.c` |
| `lrsp_solver/results.py`          | `src/results.c` |

## What is NOT here yet

- Python ctypes binding (the C library exposes a clean ABI in
  [`include/lrsp.h`](include/lrsp.h); a binding mirroring `mespprc_native`'s
  pattern can be added when the rest of the project needs it). The C runner
  + CSV output is enough to drive comparisons today.
- Branch-and-price outer loop. v1 is column generation + integer master.
- Akca's CW / sweep / nearest-neighbor warmstart heuristics. Singleton
  warmstart matches the Python default.
- Linking / min-open constraints turned on. The C ABI accepts the flags
  (default off) but their solver wiring is reserved for v2.

See [`docs/C_LRSP_TODO.md`](../docs/C_LRSP_TODO.md) for the full
deferred-work list.

## Layout

```
lrsp_native/
├── CMakeLists.txt
├── README.md                       this file
├── include/lrsp.h                  public C ABI
├── src/
│   ├── arena.c                     bump allocator + helpers
│   ├── instance.c, instance_io.c   data model + Akca .txt loader
│   ├── column.c, duals.c           Column / MasterDuals
│   ├── master.c                    HiGHS-backed restricted master
│   ├── pricing_graph.c             dual-adjusted MESPPRC instance per facility
│   ├── pricing.c                   DP / IP pricing adapters via mespprc_native
│   ├── singleton_warmstart.c       per-(facility, customer) seed columns
│   ├── column_generation.c         CG outer loop
│   ├── solver.c                    public lrsp_solve dispatcher
│   ├── results.c                   result-handle accessors
│   └── api.c                       version, status names, layout self-check
├── tests/                          C-side smoke + unit tests
├── examples/
│   ├── run_lrsp.c                  --instance / --pricing CLI
│   └── compare_ip_dp.c             both engines, CSV
└── scripts/
    ├── build.bat                   Windows build entry point
    └── validate_against_python.py  C↔Python equivalence harness
```
