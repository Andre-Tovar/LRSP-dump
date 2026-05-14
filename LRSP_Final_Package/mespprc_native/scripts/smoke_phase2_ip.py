"""Phase 2 IP smoke: Python (CBC via PuLP) vs C (HiGHS) on a few small instances."""

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
    Phase2IPSolver,
    generate_instance,
)
import mespprc_native

print(f"{'n':>3} | {'py_p1_ms':>9} | {'py_ip_ms':>9} | {'c_total_ms':>10} | {'speedup':>8} | py_routes | match")
print("-" * 80)
for n in (3, 4, 5, 6, 7, 8, 10, 12):
    inst = generate_instance(GeneratorConfig(num_customers=n, seed=2026, gamma_duty=1.0))
    t0 = perf_counter()
    p1 = Phase1Solver(inst).solve()
    py_p1_ms = (perf_counter() - t0) * 1000
    t0 = perf_counter()
    pyip = Phase2IPSolver(inst).solve(p1.feasible_routes, collect_diagnostics=False)
    py_ip_ms = (perf_counter() - t0) * 1000
    py_total = py_p1_ms + py_ip_ms

    t0 = perf_counter()
    nat = mespprc_native.solve_phase2_ip(inst)
    c_ms = (perf_counter() - t0) * 1000

    py_obj = pyip.objective_value if pyip.is_feasible else None
    c_obj = nat.total_cost
    if py_obj is None and c_obj is None:
        match = "ok (both inf)"
    elif py_obj is None or c_obj is None:
        match = f"MISMATCH py={py_obj} c={c_obj}"
    else:
        match = "ok" if abs(py_obj - c_obj) < 1e-6 else f"MISMATCH py={py_obj} c={c_obj}"
    speedup = py_total / max(c_ms, 1e-9)
    print(
        f"{n:>3} | {py_p1_ms:>9.3f} | {py_ip_ms:>9.3f} | {c_ms:>10.3f} | {speedup:>7.1f}x | "
        f"{len(p1.feasible_routes):>9} | {match}"
    )
