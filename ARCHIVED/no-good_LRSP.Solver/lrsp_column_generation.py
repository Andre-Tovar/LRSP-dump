from __future__ import annotations

from dataclasses import dataclass
from time import perf_counter
from typing import Dict, List, Sequence

from mespprc_lrsp import Route

from .akca_instance_generator import AkcaLRSPInstance, rounded_euclidean_distance
from .branching_rules import NodeConstraints
from .lrsp_column import LRSPPairingColumn, MasterDuals
from .lrsp_master import LRSPMasterSolution, MasterModelConfig, RestrictedMasterProblem
from .pricing_adapter import (
    FacilityPricingAdapter,
    PricingEngineConfig,
)


@dataclass(slots=True)
class ColumnGenerationConfig:
    pricing_phase2: str = "dp"
    phase1_label_limit: int | None = None
    max_iterations: int = 20
    max_columns_per_facility: int = 1
    improvement_tolerance: float = 1e-6
    solve_integer_master_after_cg: bool = True
    use_dual_stabilization: bool = True
    dual_stabilization_weight: float = 0.6
    use_customer_facility_linking: bool = True
    use_minimum_open_facilities_bound: bool = True
    pricing_backend: object | None = None


@dataclass(slots=True)
class FacilityPricingSummary:
    facility_id: int
    phase1_time: float
    phase2_time: float
    total_time: float
    route_count: int
    negative_route_count: int
    generated_column_count: int
    best_reduced_cost: float | None
    phase2_status: str


@dataclass(slots=True)
class ColumnGenerationIteration:
    iteration_index: int
    master_objective: float | None
    pricing_call_count: int
    new_column_count: int
    phase1_time: float
    phase2_time: float
    used_stabilized_duals: bool
    pricing_summaries: List[FacilityPricingSummary]


@dataclass(slots=True)
class ColumnGenerationResult:
    pricing_phase2: str
    status: str
    root_lp_objective: float | None
    integer_objective: float | None
    master_iterations: int
    pricing_call_count: int
    total_columns: int
    phase1_time_total: float
    phase2_time_total: float
    total_column_generation_time: float
    iterations: List[ColumnGenerationIteration]
    root_master_solution: LRSPMasterSolution | None
    integer_master_solution: LRSPMasterSolution | None
    column_pool: List[LRSPPairingColumn]


class LRSPColumnGenerationSolver:
    """
    Root-node column generation loop for the LRSP master problem.

    The restricted master problem is solved as an LP relaxation. Dual prices from the
    master are fed into facility-specific pricing calls, which return negative
    reduced-cost pairings as new columns. The pricing implementation is swappable so
    different MESPPRC variants can be plugged into the same LRSP shell.
    """

    def __init__(self, config: ColumnGenerationConfig | None = None) -> None:
        self.config = config or ColumnGenerationConfig()

    def solve(self, instance: AkcaLRSPInstance) -> ColumnGenerationResult:
        return self.solve_with_constraints(instance, node_constraints=NodeConstraints.root())

    def solve_with_constraints(
        self,
        instance: AkcaLRSPInstance,
        *,
        node_constraints: NodeConstraints,
    ) -> ColumnGenerationResult:
        start_time = perf_counter()
        master = RestrictedMasterProblem(
            instance,
            MasterModelConfig(
                use_customer_facility_linking=self.config.use_customer_facility_linking,
                use_minimum_open_facilities_bound=self.config.use_minimum_open_facilities_bound,
            ),
        )
        next_column_id = 1
        initial_columns = self._build_initial_columns(
            instance,
            column_id_start=next_column_id,
            node_constraints=node_constraints,
        )
        master.add_columns(initial_columns)
        next_column_id += len(initial_columns)

        pricing_adapter = FacilityPricingAdapter(
            PricingEngineConfig(
                phase2_method=self.config.pricing_phase2,
                phase1_label_limit=self.config.phase1_label_limit,
                max_columns_per_facility=self.config.max_columns_per_facility,
                improvement_tolerance=self.config.improvement_tolerance,
                pricing_backend=self.config.pricing_backend,
            )
        )

        iterations: List[ColumnGenerationIteration] = []
        pricing_call_count = 0
        phase1_time_total = 0.0
        phase2_time_total = 0.0
        root_master_solution: LRSPMasterSolution | None = None
        stabilized_duals: MasterDuals | None = None

        for iteration_index in range(self.config.max_iterations):
            master_solution = master.solve(relax=True, node_constraints=node_constraints)
            root_master_solution = master_solution
            if not master_solution.is_optimal or master_solution.duals is None:
                break

            pricing_duals = master_solution.duals
            used_stabilized_duals = False
            if self.config.use_dual_stabilization:
                pricing_duals = self._blend_duals(
                    raw_duals=master_solution.duals,
                    previous_duals=stabilized_duals,
                    weight=self.config.dual_stabilization_weight,
                )
                stabilized_duals = pricing_duals
                used_stabilized_duals = pricing_duals != master_solution.duals

            pricing_summaries: List[FacilityPricingSummary] = []
            new_columns: List[LRSPPairingColumn] = []
            iteration_phase1 = 0.0
            iteration_phase2 = 0.0
            iteration_pricing_call_count = 0

            (
                new_columns,
                pricing_summaries,
                iteration_phase1,
                iteration_phase2,
                iteration_pricing_call_count,
                next_column_id,
            ) = self._run_pricing_round(
                instance=instance,
                node_constraints=node_constraints,
                pricing_adapter=pricing_adapter,
                duals=pricing_duals,
                next_column_id=next_column_id,
                iteration_index=iteration_index,
            )
            pricing_call_count += iteration_pricing_call_count
            phase1_time_total += iteration_phase1
            phase2_time_total += iteration_phase2

            if not new_columns and used_stabilized_duals:
                (
                    new_columns,
                    pricing_summaries,
                    iteration_phase1,
                    iteration_phase2,
                    iteration_pricing_call_count,
                    next_column_id,
                ) = self._run_pricing_round(
                    instance=instance,
                    node_constraints=node_constraints,
                    pricing_adapter=pricing_adapter,
                    duals=master_solution.duals,
                    next_column_id=next_column_id,
                    iteration_index=iteration_index,
                )
                pricing_call_count += iteration_pricing_call_count
                phase1_time_total += iteration_phase1
                phase2_time_total += iteration_phase2
                used_stabilized_duals = False

            added_columns = master.add_columns(new_columns)
            iterations.append(
                ColumnGenerationIteration(
                    iteration_index=iteration_index,
                    master_objective=master_solution.objective_value,
                    pricing_call_count=iteration_pricing_call_count,
                    new_column_count=len(added_columns),
                    phase1_time=iteration_phase1,
                    phase2_time=iteration_phase2,
                    used_stabilized_duals=used_stabilized_duals,
                    pricing_summaries=pricing_summaries,
                )
            )
            if not added_columns:
                break

        integer_master_solution = (
            master.solve(relax=False, node_constraints=node_constraints)
            if self.config.solve_integer_master_after_cg
            else None
        )
        total_time = perf_counter() - start_time

        return ColumnGenerationResult(
            pricing_phase2=self.config.pricing_phase2,
            status=root_master_solution.status if root_master_solution else "not_solved",
            root_lp_objective=(
                root_master_solution.objective_value if root_master_solution else None
            ),
            integer_objective=(
                integer_master_solution.objective_value
                if integer_master_solution is not None
                else None
            ),
            master_iterations=len(iterations),
            pricing_call_count=pricing_call_count,
            total_columns=len(master.columns),
            phase1_time_total=phase1_time_total,
            phase2_time_total=phase2_time_total,
            total_column_generation_time=total_time,
            iterations=iterations,
            root_master_solution=root_master_solution,
            integer_master_solution=integer_master_solution,
            column_pool=master.columns,
        )

    def _run_pricing_round(
        self,
        *,
        instance: AkcaLRSPInstance,
        node_constraints: NodeConstraints,
        pricing_adapter: FacilityPricingAdapter,
        duals: MasterDuals,
        next_column_id: int,
        iteration_index: int,
    ) -> tuple[
        List[LRSPPairingColumn],
        List[FacilityPricingSummary],
        float,
        float,
        int,
        int,
    ]:
        new_columns: List[LRSPPairingColumn] = []
        pricing_summaries: List[FacilityPricingSummary] = []
        iteration_phase1 = 0.0
        iteration_phase2 = 0.0
        iteration_pricing_call_count = 0

        for facility in instance.facilities:
            pricing_result = pricing_adapter.solve_facility(
                instance,
                facility,
                duals,
                column_id_start=next_column_id,
                iteration_index=iteration_index,
                node_constraints=node_constraints,
            )
            iteration_pricing_call_count += 1
            iteration_phase1 += pricing_result.phase1_time
            iteration_phase2 += pricing_result.phase2_time

            assigned_columns: List[LRSPPairingColumn] = []
            for column in pricing_result.generated_columns:
                column.column_id = next_column_id
                next_column_id += 1
                assigned_columns.append(column)
            new_columns.extend(assigned_columns)

            pricing_summaries.append(
                FacilityPricingSummary(
                    facility_id=facility.facility_id,
                    phase1_time=pricing_result.phase1_time,
                    phase2_time=pricing_result.phase2_time,
                    total_time=pricing_result.total_time,
                    route_count=pricing_result.route_count,
                    negative_route_count=pricing_result.negative_route_count,
                    generated_column_count=len(assigned_columns),
                    best_reduced_cost=pricing_result.best_reduced_cost,
                    phase2_status=pricing_result.phase2_status,
                )
            )

        return (
            new_columns,
            pricing_summaries,
            iteration_phase1,
            iteration_phase2,
            iteration_pricing_call_count,
            next_column_id,
        )

    def _build_initial_columns(
        self,
        instance: AkcaLRSPInstance,
        *,
        column_id_start: int,
        node_constraints: NodeConstraints,
    ) -> List[LRSPPairingColumn]:
        """
        Build a feasible initial RMP using singleton pairings.

        This keeps the root LP immediately workable without pre-enumerating complex
        pairings. Every customer is coverable from every candidate facility in the
        Akca-style instances because the dissertation's distance limits were chosen so
        that customer-facility assignments are never infeasible.
        """

        columns: List[LRSPPairingColumn] = []
        next_column_id = column_id_start
        sink_node = max(customer.customer_id for customer in instance.customers) + 1

        for facility in instance.facilities:
            if node_constraints.fixed_openings.get(facility.facility_id) == 0:
                continue
            for customer in instance.customers:
                if not node_constraints.is_customer_allowed_at_facility(
                    customer.customer_id,
                    facility.facility_id,
                ):
                    continue
                round_trip = 2.0 * instance.vehicle_operating_cost * rounded_euclidean_distance(
                    facility.x,
                    facility.y,
                    customer.x,
                    customer.y,
                )
                if customer.demand > instance.vehicle_capacity:
                    continue
                if round_trip > instance.vehicle_time_limit:
                    continue
                singleton_route = Route(
                    route_id=next_column_id,
                    cost=round_trip,
                    local_resources=[customer.demand, round_trip],
                    global_resources=[round_trip],
                    covered_customers={customer.customer_id},
                    path=[0, customer.customer_id, sink_node],
                    first_customer_in_route=customer.customer_id,
                    customer_state_signature=[1],
                )
                columns.append(
                    LRSPPairingColumn(
                        column_id=next_column_id,
                        facility_id=facility.facility_id,
                        covered_customers=(customer.customer_id,),
                        pairing_cost=instance.vehicle_fixed_cost + round_trip,
                        reduced_cost=0.0,
                        total_demand=customer.demand,
                        total_duty_time=round_trip,
                        total_route_time=round_trip,
                        route_count=1,
                        constituent_routes=(singleton_route,),
                        pricing_engine="initial",
                        source_iteration=-1,
                        metadata={"kind": "singleton_seed"},
                    )
                )
                next_column_id += 1

        return columns

    def _blend_duals(
        self,
        *,
        raw_duals: MasterDuals,
        previous_duals: MasterDuals | None,
        weight: float,
    ) -> MasterDuals:
        if previous_duals is None:
            return raw_duals

        def blend_map(
            raw_map: Dict[object, float],
            previous_map: Dict[object, float],
        ) -> Dict[object, float]:
            keys = set(raw_map) | set(previous_map)
            return {
                key: weight * previous_map.get(key, 0.0)
                + (1.0 - weight) * raw_map.get(key, 0.0)
                for key in keys
            }

        def blend_scalar(raw_value: float, previous_value: float) -> float:
            return weight * previous_value + (1.0 - weight) * raw_value

        return MasterDuals(
            coverage_duals=blend_map(raw_duals.coverage_duals, previous_duals.coverage_duals),
            facility_capacity_duals=blend_map(
                raw_duals.facility_capacity_duals,
                previous_duals.facility_capacity_duals,
            ),
            facility_linking_duals=blend_map(
                raw_duals.facility_linking_duals,
                previous_duals.facility_linking_duals,
            ),
            required_assignment_duals=blend_map(
                raw_duals.required_assignment_duals,
                previous_duals.required_assignment_duals,
            ),
            forbidden_assignment_duals=blend_map(
                raw_duals.forbidden_assignment_duals,
                previous_duals.forbidden_assignment_duals,
            ),
            facility_customer_link_duals=blend_map(
                raw_duals.facility_customer_link_duals,
                previous_duals.facility_customer_link_duals,
            ),
            minimum_open_facilities_dual=blend_scalar(
                raw_duals.minimum_open_facilities_dual,
                previous_duals.minimum_open_facilities_dual,
            ),
        )
