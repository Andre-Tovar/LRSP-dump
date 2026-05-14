"""Compare Phase 2 DP timings between Python and C."""

from __future__ import annotations

import sys
from pathlib import Path
from time import perf_counter

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mespprc import (
    GeneratorConfig,
    Phase1Solver,
    Phase2DPSolver,
    generate_instance,
)
import mespprc_native

print(f"{'n':>3} | {'py_p1_ms':>10} | {'py_p2_ms':>10} | {'c_total_ms':>10} | {'speedup':>8} | py_routes", flush=True)
print("-" * 75, flush=True)
for n in (4, 5, 6, 7, 8):
    inst = generate_instance(GeneratorConfig(num_customers=n, seed=2026, gamma_duty=1.0))
    t0 = perf_counter()
    py1 = Phase1Solver(inst).solve()
    py_p1_ms = (perf_counter() - t0) * 1000
    t0 = perf_counter()
    py2 = Phase2DPSolver(inst).solve(py1.feasible_routes)
    py_p2_ms = (perf_counter() - t0) * 1000
    py_total_ms = py_p1_ms + py_p2_ms

    t0 = perf_counter()
    nat = mespprc_native.solve_phase2_dp(inst)
    c_ms = (perf_counter() - t0) * 1000

    speedup = py_total_ms / max(c_ms, 1e-9)
    py_obj = py2.total_cost
    nat_obj = nat.total_cost
    match = "ok" if py_obj is not None and nat_obj is not None and abs(py_obj - nat_obj) < 1e-6 else "MISMATCH"
    print(
        f"{n:>3} | {py_p1_ms:>10.3f} | {py_p2_ms:>10.3f} | {c_ms:>10.3f} | {speedup:>7.1f}x | "
        f"py_routes={len(py1.feasible_routes)} match={match}",
        flush=True,
    )
