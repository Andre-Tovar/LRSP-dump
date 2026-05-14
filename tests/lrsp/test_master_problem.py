from __future__ import annotations

from lrsp_solver import (
    Column,
    MasterConfig,
    RestrictedMasterProblem,
    synthetic_instance,
)
from lrsp_solver.pricing_graph import SOURCE_NODE_ID


def _seed_columns(instance, master: RestrictedMasterProblem) -> list[Column]:
    """Build one trivial covering column per (facility, customer) pair."""

    columns: list[Column] = []
    sink_id = max(c.id for c in instance.customers) + 1
    for facility in instance.facilities:
        for customer in instance.customers:
            cid = master.next_column_id()
            columns.append(
                Column(
                    column_id=cid,
                    facility_id=facility.id,
                    covered_customers=(customer.id,),
                    pairing_cost=10.0 + customer.demand,
                    reduced_cost=0.0,
                    total_demand=customer.demand,
                    total_travel_cost=10.0,
                    routes=((SOURCE_NODE_ID, customer.id, sink_id),),
                    pricing_engine="test_seed",
                )
            )
    return columns


def test_restricted_master_with_singleton_columns_is_feasible():
    instance = synthetic_instance(customer_count=3, facility_count=2)
    master = RestrictedMasterProblem(instance, MasterConfig())
    master.add_columns(_seed_columns(instance, master))

    solution = master.solve(relax=True)
    assert solution.is_optimal
    assert solution.objective is not None
    assert solution.duals is not None
    # Coverage duals exist for every customer.
    for customer in instance.customers:
        assert customer.id in solution.duals.coverage


def test_restricted_master_integer_solution_picks_columns():
    instance = synthetic_instance(customer_count=3, facility_count=2)
    master = RestrictedMasterProblem(instance, MasterConfig())
    master.add_columns(_seed_columns(instance, master))

    integer_solution = master.solve(relax=False)
    assert integer_solution.is_optimal
    selected_customers = {
        cust_id
        for column in integer_solution.selected_columns
        for cust_id in column.covered_customers
    }
    assert selected_customers == {c.id for c in instance.customers}


def test_master_dedupes_columns_by_signature():
    instance = synthetic_instance(customer_count=2, facility_count=1)
    master = RestrictedMasterProblem(instance, MasterConfig())
    seeds = _seed_columns(instance, master)
    master.add_columns(seeds)
    # Re-add the same columns with different ids; signatures should collide.
    duplicate = [
        Column(
            column_id=master.next_column_id(),
            facility_id=col.facility_id,
            covered_customers=col.covered_customers,
            pairing_cost=col.pairing_cost,
            reduced_cost=col.reduced_cost,
            total_demand=col.total_demand,
            total_travel_cost=col.total_travel_cost,
            routes=col.routes,
            pricing_engine="duplicate",
        )
        for col in seeds
    ]
    added = master.add_columns(duplicate)
    assert added == []
    assert master.column_count == len(seeds)
