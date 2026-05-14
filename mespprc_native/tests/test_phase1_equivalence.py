"""
Phase 1 equivalence tests: C port vs Python oracle.

The C `mespprc_native.solve_phase1` must return the same set of routes as the
Python `mespprc.Phase1Solver.solve()` for every instance in this test set.

Equivalence is defined by canonical route signatures:
    (cost,
     tuple(path),
     tuple(sorted(covered_customers)),
     tuple(customer_state_signature))

Both ordering and per-route metadata must match. If a route exists in one set
and not the other, the test fails with a diff.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mespprc import GeneratorConfig, Phase1Solver, generate_instance
from mespprc.instance_database import iter_database_instances, list_database_instances
from mespprc.instance_io import load_instance_json
import mespprc_native


# ---------- Equivalence helper ----------


def _python_route_signatures(py_routes) -> list[tuple]:
    sigs = []
    for r in py_routes:
        sigs.append(
            (
                round(float(r.cost), 9),
                tuple(int(p) for p in r.path),
                tuple(sorted(int(c) for c in r.covered_customers)),
                tuple(int(s) for s in r.customer_state_signature),
            )
        )
    return sigs


def _native_route_signatures(native_routes) -> list[tuple]:
    sigs = []
    for r in native_routes:
        sigs.append(
            (
                round(float(r.cost), 9),
                tuple(int(p) for p in r.path),
                tuple(sorted(int(c) for c in r.covered_customers)),
                tuple(int(s) for s in r.customer_state_signature),
            )
        )
    return sigs


def _assert_equivalent(py_routes, native_routes, *, instance_label: str) -> None:
    py_sigs = _python_route_signatures(py_routes)
    nat_sigs = _native_route_signatures(native_routes)

    py_set = set(py_sigs)
    nat_set = set(nat_sigs)
    if py_set == nat_set and len(py_sigs) == len(nat_sigs):
        return

    only_py = py_set - nat_set
    only_nat = nat_set - py_set
    msg = [
        f"Phase 1 mismatch on {instance_label}",
        f"  python routes: {len(py_sigs)}, native routes: {len(nat_sigs)}",
    ]
    if only_py:
        msg.append(f"  routes in PYTHON but not NATIVE ({len(only_py)} sample below):")
        for sig in list(sorted(only_py))[:5]:
            msg.append(f"    {sig}")
    if only_nat:
        msg.append(f"  routes in NATIVE but not PYTHON ({len(only_nat)} sample below):")
        for sig in list(sorted(only_nat))[:5]:
            msg.append(f"    {sig}")
    pytest.fail("\n".join(msg))


# ---------- Tests ----------


@pytest.mark.parametrize("seed", [1, 2, 3, 7, 42])
@pytest.mark.parametrize("n_customers", [3, 4, 5])
def test_equivalence_on_small_synthetic(seed: int, n_customers: int) -> None:
    """Tiny synthetic instances. If anything is wrong with the C port, this
    fails fast with a small diff."""

    instance = generate_instance(
        GeneratorConfig(num_customers=n_customers, seed=seed, gamma_duty=1.0)
    )
    py_result = Phase1Solver(instance).solve()
    native_routes = mespprc_native.solve_phase1(instance)
    _assert_equivalent(
        py_result.feasible_routes,
        native_routes,
        instance_label=f"synthetic n={n_customers} seed={seed}",
    )


@pytest.mark.parametrize("n_customers", [6, 8])
def test_equivalence_on_medium_synthetic(n_customers: int) -> None:
    instance = generate_instance(
        GeneratorConfig(num_customers=n_customers, seed=2024, gamma_duty=1.0)
    )
    py_result = Phase1Solver(instance).solve()
    native_routes = mespprc_native.solve_phase1(instance)
    _assert_equivalent(
        py_result.feasible_routes,
        native_routes,
        instance_label=f"synthetic n={n_customers}",
    )


def test_equivalence_on_bundled_database() -> None:
    """Run against every JSON instance under mespprc/instance_db/."""

    failures: list[str] = []
    checked = 0
    for record, instance in iter_database_instances():
        py_result = Phase1Solver(instance).solve()
        native_routes = mespprc_native.solve_phase1(instance)
        py_sigs = _python_route_signatures(py_result.feasible_routes)
        nat_sigs = _native_route_signatures(native_routes)
        if set(py_sigs) != set(nat_sigs) or len(py_sigs) != len(nat_sigs):
            failures.append(
                f"{record.instance_id}: python={len(py_sigs)} native={len(nat_sigs)} "
                f"only_py={len(set(py_sigs) - set(nat_sigs))} "
                f"only_native={len(set(nat_sigs) - set(py_sigs))}"
            )
        checked += 1

    if checked == 0:
        pytest.skip("instance database is empty")

    if failures:
        head = failures[:5]
        pytest.fail(
            f"{len(failures)} of {checked} bundled instances disagree:\n  "
            + "\n  ".join(head)
        )
