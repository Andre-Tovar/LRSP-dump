from __future__ import annotations

from typing import List

from lrsp_solver import (
    Column,
    MESPPRCDPPricingSolver,
    MESPPRCIPPricingSolver,
    PricingConfig,
    PricingProblem,
    PricingSolver,
    synthetic_instance,
)
from lrsp_solver.column import MasterDuals
from lrsp_solver.pricing_interface import PricingResult


def _zero_duals(instance) -> MasterDuals:
    return MasterDuals(
        coverage={c.id: 0.0 for c in instance.customers},
        facility_capacity={f.id: 0.0 for f in instance.facilities},
        facility_customer_link={
            (c.id, f.id): 0.0 for f in instance.facilities for c in instance.customers
        },
        min_open_facilities=0.0,
    )


def _high_coverage_duals(instance) -> MasterDuals:
    return MasterDuals(
        coverage={c.id: 1_000.0 for c in instance.customers},
        facility_capacity={f.id: 0.0 for f in instance.facilities},
        facility_customer_link={
            (c.id, f.id): 0.0 for f in instance.facilities for c in instance.customers
        },
        min_open_facilities=0.0,
    )


def test_pricing_solver_subclasses_implement_interface():
    assert isinstance(MESPPRCDPPricingSolver(), PricingSolver)
    assert isinstance(MESPPRCIPPricingSolver(), PricingSolver)


def test_dp_pricing_returns_no_columns_with_zero_duals():
    instance = synthetic_instance(customer_count=4, facility_count=2)
    pricing = MESPPRCDPPricingSolver()
    facility = instance.facilities[0]

    problem = PricingProblem(
        instance=instance,
        facility=facility,
        duals=_zero_duals(instance),
        iteration=0,
        next_column_id_start=1,
        config=PricingConfig(max_columns_per_facility=3),
    )
    result = pricing.solve(problem)
    assert isinstance(result, PricingResult)
    # With no duals, every elementary route has positive cost; nothing improves.
    assert result.columns == []
    assert result.pricing_time >= 0.0


def test_dp_pricing_returns_columns_with_large_coverage_duals():
    instance = synthetic_instance(customer_count=4, facility_count=2)
    pricing = MESPPRCDPPricingSolver()
    facility = instance.facilities[0]

    problem = PricingProblem(
        instance=instance,
        facility=facility,
        duals=_high_coverage_duals(instance),
        iteration=0,
        next_column_id_start=1,
        config=PricingConfig(max_columns_per_facility=3),
    )
    result = pricing.solve(problem)
    assert result.columns, "expected improving columns when duals dominate travel cost"
    for column in result.columns:
        assert isinstance(column, Column)
        assert column.facility_id == facility.id
        assert column.reduced_cost < 0


def test_ip_and_dp_pricing_agree_on_best_reduced_cost_when_duals_are_high():
    instance = synthetic_instance(customer_count=4, facility_count=2)
    duals = _high_coverage_duals(instance)
    facility = instance.facilities[0]

    dp = MESPPRCDPPricingSolver().solve(
        PricingProblem(
            instance=instance,
            facility=facility,
            duals=duals,
            iteration=0,
            next_column_id_start=1,
            config=PricingConfig(max_columns_per_facility=5),
        )
    )
    ip = MESPPRCIPPricingSolver().solve(
        PricingProblem(
            instance=instance,
            facility=facility,
            duals=duals,
            iteration=0,
            next_column_id_start=1,
            config=PricingConfig(max_columns_per_facility=5),
        )
    )
    assert dp.best_reduced_cost is not None
    assert ip.best_reduced_cost is not None
    assert abs(dp.best_reduced_cost - ip.best_reduced_cost) <= 1e-4
