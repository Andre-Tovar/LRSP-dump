"""Quick Python-vs-C Phase 1 timing comparison."""

from __future__ import annotations

from pathlib import Path
import sys
from time import perf_counter

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mespprc import GeneratorConfig, Phase1Solver, generate_instance
import mespprc_native

print(f"{'n':>3} | {'py_ms':>10} | {'c_ms':>10} | {'speedup':>8} | routes")
print("-" * 55)
for n in (4, 6, 8, 10, 12):
    inst = generate_instance(
        GeneratorConfig(num_customers=n, seed=2026, gamma_duty=1.0)
    )
    t0 = perf_counter()
    py = Phase1Solver(inst).solve()
    py_ms = (perf_counter() - t0) * 1000
    t0 = perf_counter()
    nat = mespprc_native.solve_phase1(inst)
    c_ms = (perf_counter() - t0) * 1000
    speedup = py_ms / max(c_ms, 1e-9)
    print(
        f"{n:>3} | {py_ms:>10.3f} | {c_ms:>10.3f} | {speedup:>7.1f}x | {len(py.feasible_routes)}"
    )
