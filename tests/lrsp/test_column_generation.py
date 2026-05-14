from __future__ import annotations

import pytest

from lrsp_solver import (
    LRSPSolver,
    LRSPSolverConfig,
    compare_pricing_engines,
    synthetic_instance,
)


def _small_config(pricing: str) -> LRSPSolverConfig:
    return LRSPSolverConfig(
        pricing=pricing,
        max_iterations=20,
        max_columns_per_facility=5,
        solve_integer_master=True,
    )


@pytest.mark.parametrize("pricing", ["dp", "ip"])
def test_solver_finds_lp_optimum_on_small_synthetic(pricing):
    instance = synthetic_instance(customer_count=4, facility_count=2, seed=1)
    result = LRSPSolver(_small_config(pricing)).solve(instance)
    assert result.reached_optimality
    assert result.root_lp_objective is not None
    assert result.integer_objective is not None
    assert result.integer_objective + 1e-6 >= result.root_lp_objective


def test_dp_and_ip_agree_on_root_lp_for_small_instance():
    instance = synthetic_instance(customer_count=4, facility_count=2, seed=42)
    comparison = compare_pricing_engines(
        instance, base_config=LRSPSolverConfig(max_iterations=20, max_columns_per_facility=5)
    )
    assert comparison.dp_result.root_lp_objective is not None
    assert comparison.ip_result.root_lp_objective is not None
    assert abs(
        comparison.dp_result.root_lp_objective - comparison.ip_result.root_lp_objective
    ) <= 1e-3


def test_solver_reports_pricing_engine_name():
    instance = synthetic_instance(customer_count=3, facility_count=2)
    result_dp = LRSPSolver(LRSPSolverConfig(pricing="dp", max_iterations=10)).solve(instance)
    result_ip = LRSPSolver(LRSPSolverConfig(pricing="ip", max_iterations=10)).solve(instance)
    assert result_dp.pricing_engine == "mespprc_dp"
    assert result_ip.pricing_engine == "mespprc_ip"
