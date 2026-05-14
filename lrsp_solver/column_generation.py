from __future__ import annotations

from dataclasses import dataclass, field
from time import perf_counter
from typing import List

from .column import Column
from .instance import Facility, LRSPInstance
from .master_problem import MasterConfig, RestrictedMasterProblem
from .pricing_interface import PricingConfig, PricingProblem, PricingSolver
from .pricing_graph import SOURCE_NODE_ID
from .results import ColumnGenerationResult, IterationSummary, PricingFacilitySummary
from .utils import euclidean_distance


@dataclass(slots=True)
class ColumnGenerationConfig:
    max_iterations: int = 50
    improvement_tolerance: float = 1e-6
    solve_integer_master: bool = True
    seed_with_singletons: bool = True
    time_limit_seconds: float | None = None
    master_config: MasterConfig = field(default_factory=MasterConfig)
    pricing_config: PricingConfig = field(default_factory=PricingConfig)


class ColumnGenerationSolver:
    """
    Outer column-generation loop for the LRSP root LP.

    Each iteration:
    1. Solve the LP relaxation of the restricted master.
    2. Pull duals.
    3. Call the pricing engine once per facility.
    4. Add improving columns to the master.
    5. Stop when no improving column is generated, when an iteration produces no
       new columns, or when `max_iterations` is reached.

    After the loop terminates, optionally re-solve the restricted master as an
    integer program over the columns produced so far. The integer step uses the
    same model rows; only the variable category changes.
    """

    def __init__(
        self,
        pricing_solver: PricingSolver,
        config: ColumnGenerationConfig | None = None,
    ) -> None:
        self.pricing_solver = pricing_solver
        self.config = config or ColumnGenerationConfig()

    def solve(self, instance: LRSPInstance) -> ColumnGenerationResult:
        run_start = perf_counter()
        master = RestrictedMasterProblem(instance, self.config.master_config)

        if self.config.seed_with_singletons:
            seed_columns = _build_singleton_seed_columns(
                instance,
                next_id_seed=master.next_column_id,
            )
            master.add_columns(seed_columns)

        result = ColumnGenerationResult(
            pricing_engine=self.pricing_solver.name,
            instance_name=instance.name,
            status="not_solved",
        )

        time_limit = self.config.time_limit_seconds
        hit_time_limit = False
        for iteration in range(self.config.max_iterations):
            if time_limit is not None and (perf_counter() - run_start) >= time_limit:
                hit_time_limit = True
                break

            master_start = perf_counter()
            master_solution = master.solve(relax=True)
            master_elapsed = perf_counter() - master_start
            result.master_runtime += master_elapsed

            if not master_solution.is_optimal or master_solution.duals is None:
                result.failure_message = (
                    f"Master LP did not solve to optimality at iteration {iteration} "
                    f"(status={master_solution.status})."
                )
                result.final_master = master_solution
                break

            iteration_pricing_time = 0.0
            iteration_summaries: List[PricingFacilitySummary] = []
            new_columns: List[Column] = []

            for facility in instance.facilities:
                problem = PricingProblem(
                    instance=instance,
                    facility=facility,
                    duals=master_solution.duals,
                    iteration=iteration,
                    next_column_id_start=master.next_column_id(),
                    config=self.config.pricing_config,
                )
                pricing_result = self.pricing_solver.solve(problem)
                iteration_pricing_time += pricing_result.pricing_time
                result.pricing_runtime += pricing_result.pricing_time
                result.pricing_call_count += 1

                added_now = master.add_columns(pricing_result.columns)
                new_columns.extend(added_now)
                iteration_summaries.append(
                    PricingFacilitySummary(
                        facility_id=facility.id,
                        pricing_time=pricing_result.pricing_time,
                        columns_added=len(added_now),
                        best_reduced_cost=pricing_result.best_reduced_cost,
                        status=pricing_result.status,
                    )
                )

            result.iterations.append(
                IterationSummary(
                    iteration=iteration,
                    master_objective=master_solution.objective,
                    master_time=master_elapsed,
                    pricing_time=iteration_pricing_time,
                    new_column_count=len(new_columns),
                    pricing_summaries=iteration_summaries,
                )
            )
            result.final_master = master_solution

            if not new_columns:
                result.reached_optimality = True
                result.status = "lp_optimal"
                break

            if time_limit is not None and (perf_counter() - run_start) >= time_limit:
                hit_time_limit = True
                break
        else:
            result.status = "iteration_limit"
            result.failure_message = (
                f"Reached max_iterations={self.config.max_iterations} without proving "
                "LP optimality."
            )

        if hit_time_limit and not result.reached_optimality:
            result.status = "time_limit"
            result.failure_message = (
                f"Hit time_limit_seconds={self.config.time_limit_seconds} "
                "before LP optimality was proven."
            )

        # Re-solve the LP after the loop so the reported root LP reflects every
        # column that was added in the final pricing round.
        if master.column_count > 0:
            final_lp_start = perf_counter()
            final_lp = master.solve(relax=True)
            result.master_runtime += perf_counter() - final_lp_start
            if final_lp.is_optimal:
                result.final_master = final_lp

        if result.final_master is not None and result.final_master.is_optimal:
            result.root_lp_objective = result.final_master.objective
            if result.status == "not_solved":
                result.status = "lp_optimal" if result.reached_optimality else "incomplete"

        result.column_pool = master.columns

        if self.config.solve_integer_master:
            integer_solution = master.solve(relax=False)
            result.integer_master = integer_solution
            result.integer_objective = integer_solution.objective

        result.total_runtime = perf_counter() - run_start
        return result


def _build_singleton_seed_columns(
    instance: LRSPInstance,
    *,
    next_id_seed,
) -> list[Column]:
    """
    Seed the master with one trivial column per (facility, customer) pair.

    Each seed column visits one customer and returns to the facility. These
    columns are immediately feasible and guarantee the initial master LP is
    bounded. They are intentionally weak so column generation drives the cost
    down quickly.
    """

    columns: list[Column] = []
    customer_by_id = {customer.id: customer for customer in instance.customers}
    for facility in instance.facilities:
        sink_node = max(customer_by_id) + 1 if customer_by_id else SOURCE_NODE_ID + 1
        for customer in instance.customers:
            if customer.demand > instance.vehicle_capacity:
                continue
            d = euclidean_distance(facility.x, facility.y, customer.x, customer.y)
            travel_cost = 2.0 * instance.vehicle_operating_cost * d
            if (
                instance.vehicle_time_limit is not None
                and travel_cost > instance.vehicle_time_limit
            ):
                continue
            cid = next_id_seed()
            columns.append(
                Column(
                    column_id=cid,
                    facility_id=facility.id,
                    covered_customers=(customer.id,),
                    pairing_cost=instance.vehicle_fixed_cost + travel_cost,
                    reduced_cost=0.0,
                    total_demand=customer.demand,
                    total_travel_cost=travel_cost,
                    routes=((SOURCE_NODE_ID, customer.id, sink_node),),
                    pricing_engine="seed",
                    iteration=-1,
                    metadata={"kind": "seed_singleton"},
                )
            )
    return columns
