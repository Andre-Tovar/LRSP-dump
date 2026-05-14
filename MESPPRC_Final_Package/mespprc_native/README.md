# mespprc_native

A C port of the MESPPRC solver in [`mespprc/`](../mespprc/), with a thin
Python binding so the existing test suite and benchmark harness can call it
in place of the pure-Python implementation.

The motivation is fairness in DP-vs-IP comparisons. The Python `Phase2IPSolver`
delegates to CBC (a C++ MIP solver) through PuLP, while the Python
`Phase2DPSolver` is pure Python. Any DP-vs-IP timing taken from the Python
side measures the language gap as much as the algorithmic gap, and the IP
wins by default at every size. With both phases compiled to native code, the
comparison reflects algorithmic structure, not language overhead.

The Python `mespprc/` package is unchanged and serves as the equivalence oracle.

## What is in here

| Path | Purpose |
|------|---------|
| [`include/mespprc.h`](include/mespprc.h) | Public C ABI |
| [`src/arena.c`](src/arena.c), [`src/bitset.c`](src/bitset.c) | Per-solve scratch allocator + customer bitset |
| [`src/instance.c`](src/instance.c) | Instance lifecycle + CSR adjacency |
| [`src/phase1.c`](src/phase1.c) | Phase 1 ESPPRC labeling DP |
| [`src/phase2_dp.c`](src/phase2_dp.c) | Phase 2 route-network covering DP |
| [`src/phase2_ip.c`](src/phase2_ip.c) | Phase 2 set-partitioning IP via HiGHS |
| [`src/api.c`](src/api.c) | `mespprc_struct_sizes()` + `mespprc_version()` |
| [`third_party/HiGHS/`](third_party/HiGHS/) | Vendored HiGHS source (built statically) |
| [`_native.py`](_native.py) | ctypes binding mirroring the public C ABI |
| [`adapters.py`](adapters.py) | High-level Python wrappers |
| [`scripts/build.bat`](scripts/build.bat) | Windows build entry point (locates VS Build Tools, drives CMake + Ninja) |
| [`scripts/smoke_phase2_ip.py`](scripts/smoke_phase2_ip.py) | C vs Python Phase 2 IP smoke timing |
| [`scripts/benchmark_phase2_native.py`](scripts/benchmark_phase2_native.py) | Apples-to-apples C Phase 2 DP vs C Phase 2 IP timing |
| [`scripts/timing_phase2_dp.py`](scripts/timing_phase2_dp.py) | C vs Python Phase 2 DP timing |
| [`tests/`](tests/) | Foundation + Phase 1 + Phase 2 DP equivalence tests |

## Build

The build is CMake + Ninja, driven by `scripts/build.bat` on Windows. The
script finds VS Build Tools at standard install paths and forwards the rest
to CMake.

```bat
scripts\build.bat              :: configure (if needed) and build
scripts\build.bat reconfigure  :: drop CMakeCache.txt and reconfigure
scripts\build.bat clean        :: rm -rf build/
```

The configured artefact is `build\bin\mespprc_native.dll`. The ctypes loader
in [`_native.py`](_native.py) finds it without environment trickery as long
as the build directory sits next to the package; `MESPPRC_NATIVE_LIB`
overrides the search if you need it.

The first build runs HiGHS through its own CMake project (~3 minutes on a
typical workstation). Subsequent incremental builds touch only mespprc_native
sources.

### Build dependencies

- A recent MSVC toolchain (VS 2022 / 2026 BuildTools is what the build script
  is wired for; any MSVC 19.x with CMake support works)
- CMake ≥ 3.20 (bundled with VS BuildTools)
- Ninja (bundled with VS BuildTools)
- Git, for the vendored HiGHS source

The C side is C11. The C++ side (HiGHS) is C++17. mespprc_native exports a
C ABI; nothing C++ is visible to the binding.

## Public C ABI in one paragraph

[`include/mespprc.h`](include/mespprc.h) declares opaque handles for the
instance and for each solver result, plus length / accessor functions for
every field a caller might want. Memory ownership is one-way: the library
owns every handle returned by an entry point, and the caller releases it via
the matching `*_destroy` function. There are no library-side pointers into
caller-owned memory after a call returns. A startup self-check
(`mespprc_struct_sizes`) reports `sizeof()` for every struct mirrored on the
Python side, and the binding asserts these match at import time so layout
drift between C and Python surfaces immediately rather than corrupting
memory mid-solve.

## Public Python surface

```python
import mespprc_native

# Phase 1 only — returns a list[NativeRoute].
routes = mespprc_native.solve_phase1(instance)

# End-to-end Phase 1 + Phase 2 DP, all in C.
dp = mespprc_native.solve_phase2_dp(instance)

# End-to-end Phase 1 + Phase 2 IP, all in C (HiGHS-backed).
ip = mespprc_native.solve_phase2_ip(instance)

# Apples-to-apples DP vs IP: Phase 1 once, then both Phase 2 solvers
# fed the same C-side Phase 1 result handle. Returns ms-resolution timings.
tim = mespprc_native.time_phase2_dp_vs_ip(instance)
```

`instance` is the same `mespprc.MESPPRCInstance` the Python solver consumes.
`build_native_instance` is the canonical bridge; the `solve_*` helpers call
it for you.

## Equivalence

| C entry point | Python oracle | Verified via |
|---------------|---------------|--------------|
| [`mespprc_solve_phase1`](src/phase1.c) | [`Phase1Solver`](../mespprc/phase1.py) | [`tests/test_phase1_equivalence.py`](tests/test_phase1_equivalence.py) — 17 synthetic + every JSON in [`mespprc/instance_db/`](../mespprc/instance_db/) |
| [`mespprc_solve_phase2_dp`](src/phase2_dp.c) | [`Phase2IPSolver`](../mespprc/phase2_ip.py) (provably equivalent to `Phase2DPSolver` and far faster as an oracle) | [`tests/test_phase2_dp_equivalence.py`](tests/test_phase2_dp_equivalence.py) |
| [`mespprc_solve_phase2_ip`](src/phase2_ip.c) | [`Phase2IPSolver`](../mespprc/phase2_ip.py) | [`scripts/smoke_phase2_ip.py`](scripts/smoke_phase2_ip.py) — n=3..12 objective parity verified to 1e-6 |

The Phase 2 IP equivalence tolerance is on objective value, not on the
selected route set. HiGHS and CBC pick different optimal vertices on
degenerate problems (both correct, but the support of the optimal solution
isn't unique in general).

## DP-vs-IP timing, both in C

Run with [`scripts/benchmark_phase2_native.py`](scripts/benchmark_phase2_native.py).
Phase 1 is run once per replicate inside the C library and the resulting
handle is fed to both Phase 2 solvers, so the only thing that varies between
the two timings is the Phase 2 algorithm. Sample run (3 replicates each,
seed 12345 + 1000·n + r):

| n | Phase 1 | Phase 2 DP | Phase 2 IP | Winner |
|---|---------|------------|------------|--------|
| 3 | 0.04 ms | 0.09 ms | 1.51 ms | DP |
| 4 | 0.07 ms | 0.15 ms | 2.55 ms | DP |
| 5 | 0.18 ms | 0.70 ms | 1.57 ms | DP |
| 6 | 0.43 ms | 8.46 ms | 2.95 ms | **IP (crossover)** |
| 7 | 0.79 ms | 138 ms | 6.0 ms | IP, 23× |
| 8 | 1.7 ms | 3,694 ms | 6.3 ms | IP, 588× |
| 9 | 2.9 ms | 147,471 ms | 5.4 ms | IP, ~28,000× |

The DP-vs-IP crossover for Phase 2 lives between **n=5 and n=6**. Past that
point the labeling DP scales exponentially in customer count while the IP
grows mildly with route count and is essentially flat through n=9.

## Caveats

- Windows is the only platform the build script knows about. The CMake
  project itself is portable; on Linux/macOS run `cmake -G Ninja -S . -B
  build && cmake --build build`.
- HiGHS 1.7.2 is pinned via the vendored source. Bumping the version
  requires re-checking that the C API surface used by [`src/phase2_ip.c`](src/phase2_ip.c)
  (`Highs_create`, `Highs_passMip`, `Highs_run`, `Highs_getSolution`,
  `Highs_getModelStatus`, `Highs_getObjectiveValue`, the `kHighs*` constants)
  is unchanged.
- `Phase2IPDiagnostics` from the Python side is intentionally not ported.
  The C path returns the operationally important fields — status, total
  cost, selected route indices, original/reduced route counts — and skips
  the 30+ auxiliary diagnostic fields used only by the Python research
  scripts.
