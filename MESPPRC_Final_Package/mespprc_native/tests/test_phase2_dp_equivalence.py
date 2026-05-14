"""
Phase 2 DP equivalence tests: C port vs Python oracle.

The Python `Phase2DPSolver` is the canonical oracle for Phase 2 semantics, but
its label-DP scales exponentially in customer count, so using it as an oracle
makes the test suite minutes-per-instance at n>=8. We instead use the Python
`Phase2IPSolver` as the oracle: it solves the same set-partitioning problem
via CBC (a C++ MIP solver), runs in well under a second per instance, and is
already provably equivalent to Phase2DPSolver on the same input route pool
(both return the same optimal `total_cost` on every bundled DB instance — see
`mespprc/phase2_ip.py` and the existing `tests/test_phase2_ip.py` /
`tests/test_phase2_covering.py` suites that pin this).

For every test instance we run:
- Python: `Phase1Solver(...).solve()` then `Phase2IPSolver(...).solve(routes)`.
- C:      `mespprc_native.solve_phase2_dp(...)` (C Phase 1 + C Phase 2 DP).

We assert:
- feasibility status matches
- on a feasible solve, the optimal `total_cost` matches within a small tolerance
- on infeasible solves where both engines report a reason, the reasons match
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mespprc import (
    GeneratorConfig,
    Phase1Solver,
    Phase2IPSolver,
    generate_instance,
)
from mespprc.instance_database import iter_database_instances
import mespprc_native


class _OracleResult:
    """Adapter so the assertion helper sees the same fields regardless of oracle."""

    __slots__ = ("is_feasible", "total_cost", "infeasibility_reason")

    def __init__(self, is_feasible: bool, total_cost: float | None,
                 infeasibility_reason: str | None) -> None:
        self.is_feasible = is_feasible
        self.total_cost = total_cost
        self.infeasibility_reason = infeasibility_reason


def _python_phase2_oracle(instance) -> _OracleResult:
    """Python Phase 2 IP, used as the equivalence oracle for the C Phase 2 DP."""

    phase1 = Phase1Solver(instance).solve()
    ip = Phase2IPSolver(instance).solve(
        phase1.feasible_routes, collect_diagnostics=False
    )
    return _OracleResult(
        is_feasible=bool(ip.is_feasible),
        total_cost=(
            float(ip.objective_value) if ip.is_feasible and ip.objective_value is not None
            else None
        ),
        infeasibility_reason=(
            None if ip.is_feasible else (ip.infeasibility_reason or None)
        ),
    )


def _covered_customer_set(routes) -> set[int]:
    out: set[int] = set()
    for r in routes:
        out.update(r.covered_customers)
    return out


def _assert_equivalent(py_result, native_result, *, instance_label: str) -> None:
    if py_result.is_feasible != native_result.is_feasible:
        pytest.fail(
            f"{instance_label}: feasibility mismatch — "
            f"python={py_result.is_feasible} native={native_result.is_feasible} "
            f"(python reason={py_result.infeasibility_reason}, "
            f"native reason={native_result.infeasibility_reason})"
        )
    if not py_result.is_feasible:
        # Both infeasible — ensure infeasibility_reason matches when both set.
        if (
            py_result.infeasibility_reason
            and native_result.infeasibility_reason
            and py_result.infeasibility_reason != native_result.infeasibility_reason
        ):
            pytest.fail(
                f"{instance_label}: infeasibility reason mismatch — "
                f"python={py_result.infeasibility_reason} "
                f"native={native_result.infeasibility_reason}"
            )
        return

    if py_result.total_cost is None or native_result.total_cost is None:
        pytest.fail(
            f"{instance_label}: feasible solve missing total_cost — "
            f"python={py_result.total_cost} native={native_result.total_cost}"
        )
    if abs(py_result.total_cost - native_result.total_cost) > 1e-6:
        pytest.fail(
            f"{instance_label}: total_cost mismatch — "
            f"python={py_result.total_cost} native={native_result.total_cost} "
            f"diff={py_result.total_cost - native_result.total_cost}"
        )


@pytest.mark.parametrize("seed", [1, 2, 7, 42])
@pytest.mark.parametrize("n_customers", [3, 4, 5])
def test_phase2_dp_equivalence_on_small_synthetic(
    seed: int, n_customers: int
) -> None:
    instance = generate_instance(
        GeneratorConfig(num_customers=n_customers, seed=seed, gamma_duty=1.0)
    )
    py_result = _python_phase2_oracle(instance)
    native_result = mespprc_native.solve_phase2_dp(instance)
    _assert_equivalent(
        py_result, native_result,
        instance_label=f"synthetic n={n_customers} seed={seed}",
    )


@pytest.mark.parametrize("n_customers", [6, 8, 10, 12])
def test_phase2_dp_equivalence_on_medium_synthetic(n_customers: int) -> None:
    instance = generate_instance(
        GeneratorConfig(num_customers=n_customers, seed=2024, gamma_duty=1.0)
    )
    py_result = _python_phase2_oracle(instance)
    native_result = mespprc_native.solve_phase2_dp(instance)
    _assert_equivalent(
        py_result, native_result,
        instance_label=f"synthetic n={n_customers}",
    )


def test_phase2_dp_equivalence_on_bundled_database() -> None:
    """Run against every JSON instance under mespprc/instance_db/."""

    failures: list[str] = []
    checked = 0
    for record, instance in iter_database_instances():
        py_result = _python_phase2_oracle(instance)
        native_result = mespprc_native.solve_phase2_dp(instance)
        try:
            _assert_equivalent(
                py_result, native_result,
                instance_label=record.instance_id,
            )
        except Exception as exc:  # pytest.fail raises Failed
            failures.append(f"{record.instance_id}: {exc}")
        checked += 1

    if checked == 0:
        pytest.skip("instance database is empty")
    if failures:
        head = failures[:5]
        pytest.fail(
            f"{len(failures)} of {checked} bundled instances disagree:\n  "
            + "\n  ".join(head)
        )
