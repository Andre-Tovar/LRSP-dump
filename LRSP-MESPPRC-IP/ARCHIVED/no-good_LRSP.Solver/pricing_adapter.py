from __future__ import annotations

from dataclasses import dataclass, field
from time import perf_counter
from typing import Dict, List, Protocol, Sequence, runtime_checkable

from mespprc_lrsp import (
    Arc,
    LRSPPhase1Solver,
    LRSPPhase2DPPricingSolver,
    LRSPPhase2IPPricingSolver,
    MESPPRCInstance,
    Node,
    NodeType,
    Route,
)

from .akca_instance_generator import (
    AkcaLRSPInstance,
    LRSPCustomer,
    LRSPFacility,
    rounded_euclidean_distance,
)
from .branching_rules import NodeConstraints
from .lrsp_column import LRSPPairingColumn, MasterDuals


@dataclass(slots=True)
class PricingEngineConfig:
    phase2_method: str = "dp"
    phase1_label_limit: int | None = None
    max_columns_per_facility: int = 1
    improvement_tolerance: float = 1e-6
    pricing_backend: "LRSPPricingBackend | None" = None


@dataclass(slots=True)
class FacilityPricingResult:
    facility_id: int
    phase2_method: str
    phase1_time: float
    phase2_time: float
    total_time: float
    route_count: int
    negative_route_count: int
    generated_columns: List[LRSPPairingColumn]
    best_reduced_cost: float | None
    has_improving_pairing: bool
    phase1_status: str
    phase2_status: str
    diagnostic_note: str


@dataclass(slots=True)
class _PricingBuildContext:
    pricing_instance: MESPPRCInstance
    source_node: int
    sink_node: int
    customer_by_id: Dict[int, LRSPCustomer]
    base_arc_costs: Dict[tuple[int, int], float]


@dataclass(slots=True)
class FacilityPricingProblem:
    instance: AkcaLRSPInstance
    facility: LRSPFacility
    duals: MasterDuals
    node_constraints: NodeConstraints
    pricing_instance: MESPPRCInstance
    source_node: int
    sink_node: int
    customer_by_id: Dict[int, LRSPCustomer]
    base_arc_costs: Dict[tuple[int, int], float]


@dataclass(slots=True)
class PricingColumnCandidate:
    routes: tuple[Route | object, ...]
    reduced_cost: float
    pricing_engine: str
    metadata: Dict[str, float | int | str | bool] = field(default_factory=dict)

    def signature(self) -> tuple[object, ...]:
        normalized = tuple(
            sorted(
                (
                    tuple(Route.from_route_like(route, -1).path),
                    tuple(sorted(Route.from_route_like(route, -1).covered_customers)),
                )
                for route in self.routes
            )
        )
        return normalized


@dataclass(slots=True)
class BackendPricingResult:
    phase1_time: float
    phase2_time: float
    route_count: int
    negative_route_count: int
    generated_columns: List[PricingColumnCandidate]
    best_reduced_cost: float | None
    phase1_status: str
    phase2_status: str
    diagnostic_note: str

    @property
    def total_time(self) -> float:
        return self.phase1_time + self.phase2_time


@runtime_checkable
class LRSPPricingBackend(Protocol):
    backend_name: str

    def solve(
        self,
        problem: FacilityPricingProblem,
        *,
        config: PricingEngineConfig,
    ) -> BackendPricingResult:
        ...


def build_facility_pricing_instance(
    instance: AkcaLRSPInstance,
    facility: LRSPFacility,
    duals: MasterDuals,
    *,
    node_constraints: NodeConstraints | None = None,
) -> _PricingBuildContext:
    """
    Build the single-facility pricing graph passed to the chosen MESPPRC backend.

    The graph is created per facility. Arc costs are the dual-adjusted reduced costs:
    travel cost minus the customer-coverage dual, minus the facility-capacity dual
    contribution for the entered customer, and minus any facility-customer linking or
    branching assignment duals associated with serving that customer from this facility.
    """

    node_constraints = node_constraints or NodeConstraints.root()
    customer_by_id = instance.customer_by_id()
    source_node = 0
    allowed_customers = [
        customer
        for customer in instance.customers
        if node_constraints.is_customer_allowed_at_facility(
            customer.customer_id,
            facility.facility_id,
        )
    ]
    allowed_customer_ids = [customer.customer_id for customer in allowed_customers]
    sink_node = (max(customer_by_id) + 1) if customer_by_id else 1

    pricing_instance = MESPPRCInstance(
        local_limits=[instance.vehicle_capacity, instance.vehicle_time_limit],
        global_limits=[instance.vehicle_time_limit],
    )
    pricing_instance.add_node(Node(source_node, NodeType.SOURCE))
    for customer in allowed_customers:
        pricing_instance.add_node(Node(customer.customer_id, NodeType.CUSTOMER))
    pricing_instance.add_node(Node(sink_node, NodeType.SINK))

    base_arc_costs: Dict[tuple[int, int], float] = {}
    facility_capacity_dual = duals.facility_capacity_duals.get(facility.facility_id, 0.0)

    def node_coordinates(node_id: int) -> tuple[float, float]:
        if node_id == source_node or node_id == sink_node:
            return facility.x, facility.y
        customer = customer_by_id[node_id]
        return customer.x, customer.y

    def demand_increment(head: int) -> float:
        if head == sink_node:
            return 0.0
        return customer_by_id[head].demand

    def reduced_arc_cost(head: int, base_cost: float) -> float:
        if head == sink_node:
            return base_cost
        customer = customer_by_id[head]
        customer_key = (customer.customer_id, facility.facility_id)
        return (
            base_cost
            - duals.coverage_duals.get(customer.customer_id, 0.0)
            - facility_capacity_dual * customer.demand
            - duals.facility_customer_link_duals.get(customer_key, 0.0)
            - duals.required_assignment_duals.get(customer_key, 0.0)
            - duals.forbidden_assignment_duals.get(customer_key, 0.0)
        )

    for head in allowed_customer_ids:
        x1, y1 = node_coordinates(source_node)
        x2, y2 = node_coordinates(head)
        base_cost = instance.vehicle_operating_cost * rounded_euclidean_distance(x1, y1, x2, y2)
        base_arc_costs[(source_node, head)] = base_cost
        pricing_instance.add_arc(
            Arc(
                source_node,
                head,
                cost=reduced_arc_cost(head, base_cost),
                local_res=[demand_increment(head), base_cost],
                global_res=[base_cost],
            )
        )
        x3, y3 = node_coordinates(head)
        x4, y4 = node_coordinates(sink_node)
        base_sink_cost = instance.vehicle_operating_cost * rounded_euclidean_distance(
            x3,
            y3,
            x4,
            y4,
        )
        base_arc_costs[(head, sink_node)] = base_sink_cost
        pricing_instance.add_arc(
            Arc(
                head,
                sink_node,
                cost=base_sink_cost,
                local_res=[0.0, base_sink_cost],
                global_res=[base_sink_cost],
            )
        )

    for tail in allowed_customer_ids:
        for head in allowed_customer_ids:
            if tail == head:
                continue
            x1, y1 = node_coordinates(tail)
            x2, y2 = node_coordinates(head)
            base_cost = instance.vehicle_operating_cost * rounded_euclidean_distance(x1, y1, x2, y2)
            base_arc_costs[(tail, head)] = base_cost
            pricing_instance.add_arc(
                Arc(
                    tail,
                    head,
                    cost=reduced_arc_cost(head, base_cost),
                    local_res=[demand_increment(head), base_cost],
                    global_res=[base_cost],
                )
            )

    return _PricingBuildContext(
        pricing_instance=pricing_instance,
        source_node=source_node,
        sink_node=sink_node,
        customer_by_id=customer_by_id,
        base_arc_costs=base_arc_costs,
    )


class MESPPRCTwoPhasePricingBackend:
    """
    Default LRSP pricing backend built around a swappable two-phase MESPPRC stack.

    The backend expects:
    - a Phase 1 route generator that returns route-like objects
    - a Phase 2 pairing solver that combines those routes into one improving LRSP pairing

    Users can substitute alternative MESPPRC implementations by providing different
    solver factories when constructing this backend, while the outer LRSP solver remains
    unchanged.
    """

    def __init__(
        self,
        *,
        backend_name: str = "mespprc_lrsp",
        phase1_solver_factory=None,
        phase2_dp_solver_factory=None,
        phase2_ip_solver_factory=None,
    ) -> None:
        self.backend_name = backend_name
        self.phase1_solver_factory = phase1_solver_factory or LRSPPhase1Solver
        self.phase2_dp_solver_factory = phase2_dp_solver_factory or LRSPPhase2DPPricingSolver
        self.phase2_ip_solver_factory = phase2_ip_solver_factory or LRSPPhase2IPPricingSolver

    def solve(
        self,
        problem: FacilityPricingProblem,
        *,
        config: PricingEngineConfig,
    ) -> BackendPricingResult:
        phase1_start = perf_counter()
        phase1_solver = self._build_phase1_solver(
            problem.pricing_instance,
            label_limit=config.phase1_label_limit,
        )
        phase1_result = phase1_solver.solve()
        phase1_time = perf_counter() - phase1_start

        feasible_routes = self._extract_routes(phase1_result, prefer_negative=False)
        negative_routes = self._extract_negative_routes(
            phase1_result,
            tolerance=config.improvement_tolerance,
        )
        pairing_constant = (
            problem.instance.vehicle_fixed_cost
            - problem.duals.facility_linking_duals.get(problem.facility.facility_id, 0.0)
        )

        generated_candidates: List[PricingColumnCandidate] = []
        seen_signatures: set[tuple[object, ...]] = set()
        best_reduced_cost: float | None = None
        phase2_status = "not_run"
        phase2_time = 0.0

        if negative_routes:
            phase2_start = perf_counter()
            phase2_solver = self._build_phase2_solver(
                config.phase2_method,
                problem.pricing_instance,
            )
            phase2_result = phase2_solver.solve(phase1_result)
            phase2_time = perf_counter() - phase2_start
            phase2_status = str(getattr(phase2_result, "status", "completed"))

            total_cost = getattr(phase2_result, "total_cost", None)
            selected_routes = list(getattr(phase2_result, "selected_routes", []) or [])
            if total_cost is not None and selected_routes:
                best_reduced_cost = float(total_cost) + pairing_constant
                if best_reduced_cost < -config.improvement_tolerance:
                    candidate = PricingColumnCandidate(
                        routes=tuple(selected_routes),
                        reduced_cost=best_reduced_cost,
                        pricing_engine=f"{self.backend_name}:{config.phase2_method}",
                        metadata={
                            "kind": "pairing",
                            "phase2_method": config.phase2_method,
                        },
                    )
                    signature = candidate.signature()
                    if signature not in seen_signatures:
                        seen_signatures.add(signature)
                        generated_candidates.append(candidate)

            for route in sorted(negative_routes, key=lambda item: float(getattr(item, "cost", 0.0))):
                if len(generated_candidates) >= config.max_columns_per_facility:
                    break
                normalized_route = Route.from_route_like(route, getattr(route, "route_id", -1))
                if not self._route_is_globally_feasible(
                    normalized_route,
                    problem.instance.vehicle_time_limit,
                ):
                    continue
                singleton_reduced_cost = float(getattr(route, "cost", 0.0)) + pairing_constant
                if singleton_reduced_cost >= -config.improvement_tolerance:
                    continue
                candidate = PricingColumnCandidate(
                    routes=(normalized_route,),
                    reduced_cost=singleton_reduced_cost,
                    pricing_engine=f"{self.backend_name}:singleton",
                    metadata={
                        "kind": "singleton",
                        "phase2_method": config.phase2_method,
                    },
                )
                signature = candidate.signature()
                if signature in seen_signatures:
                    continue
                seen_signatures.add(signature)
                generated_candidates.append(candidate)

        return BackendPricingResult(
            phase1_time=phase1_time,
            phase2_time=phase2_time,
            route_count=len(feasible_routes),
            negative_route_count=len(negative_routes),
            generated_columns=generated_candidates[: config.max_columns_per_facility],
            best_reduced_cost=best_reduced_cost,
            phase1_status="has_negative_routes" if negative_routes else "no_negative_routes",
            phase2_status=phase2_status,
            diagnostic_note=(
                f"facility={problem.facility.facility_id}, backend={self.backend_name}, "
                f"routes={len(feasible_routes)}, negative_routes={len(negative_routes)}, "
                f"candidates={len(generated_candidates)}"
            ),
        )

    def _build_phase1_solver(
        self,
        pricing_instance: MESPPRCInstance,
        *,
        label_limit: int | None,
    ):
        try:
            return self.phase1_solver_factory(
                pricing_instance,
                label_limit=label_limit,
            )
        except TypeError:
            return self.phase1_solver_factory(pricing_instance)

    def _build_phase2_solver(
        self,
        phase2_method: str,
        pricing_instance: MESPPRCInstance,
    ):
        if phase2_method == "dp":
            return self.phase2_dp_solver_factory(pricing_instance)
        if phase2_method == "ip":
            return self.phase2_ip_solver_factory(pricing_instance)
        raise ValueError(f"Unknown Phase 2 pricing method: {phase2_method}.")

    def _extract_routes(
        self,
        phase1_result: object,
        *,
        prefer_negative: bool,
    ) -> List[Route | object]:
        if prefer_negative:
            negative_routes = getattr(phase1_result, "negative_routes", None)
            if negative_routes is not None:
                return list(negative_routes)
            negative_cost_routes = getattr(phase1_result, "negative_cost_routes", None)
            if negative_cost_routes is not None:
                return list(negative_cost_routes)
        feasible_routes = getattr(phase1_result, "feasible_routes", None)
        if feasible_routes is not None:
            return list(feasible_routes)
        exported_routes = getattr(phase1_result, "exported_routes", None)
        if exported_routes is not None:
            return list(exported_routes)
        negative_cost_routes = getattr(phase1_result, "negative_cost_routes", None)
        if negative_cost_routes is not None:
            return list(negative_cost_routes)
        if isinstance(phase1_result, Sequence):
            return list(phase1_result)
        return []

    def _extract_negative_routes(
        self,
        phase1_result: object,
        *,
        tolerance: float,
    ) -> List[Route | object]:
        candidate_routes = self._extract_routes(phase1_result, prefer_negative=True)
        return [
            route
            for route in candidate_routes
            if float(getattr(route, "cost", 0.0)) < -tolerance
        ]

    def _route_is_globally_feasible(
        self,
        route: Route,
        vehicle_time_limit: float,
    ) -> bool:
        global_resources = list(getattr(route, "global_resources", []))
        return not global_resources or global_resources[0] <= vehicle_time_limit


class FacilityPricingAdapter:
    """
    Facility-specific pricing wrapper around a pluggable MESPPRC backend.

    This adapter translates LRSP master duals into a facility-level reduced-cost graph,
    then delegates the actual route/pairing generation to the configured backend.
    """

    def __init__(self, config: PricingEngineConfig | None = None) -> None:
        self.config = config or PricingEngineConfig()
        self.backend = self.config.pricing_backend or MESPPRCTwoPhasePricingBackend()

    def solve_facility(
        self,
        instance: AkcaLRSPInstance,
        facility: LRSPFacility,
        duals: MasterDuals,
        *,
        column_id_start: int,
        iteration_index: int,
        node_constraints: NodeConstraints | None = None,
    ) -> FacilityPricingResult:
        node_constraints = node_constraints or NodeConstraints.root()
        if node_constraints.fixed_openings.get(facility.facility_id) == 0:
            return FacilityPricingResult(
                facility_id=facility.facility_id,
                phase2_method=self.config.phase2_method,
                phase1_time=0.0,
                phase2_time=0.0,
                total_time=0.0,
                route_count=0,
                negative_route_count=0,
                generated_columns=[],
                best_reduced_cost=None,
                has_improving_pairing=False,
                phase1_status="facility_closed_by_branching",
                phase2_status="not_run",
                diagnostic_note=f"facility={facility.facility_id} skipped by node constraints",
            )

        build_context = build_facility_pricing_instance(
            instance,
            facility,
            duals,
            node_constraints=node_constraints,
        )
        if len(build_context.pricing_instance.customers()) == 0:
            return FacilityPricingResult(
                facility_id=facility.facility_id,
                phase2_method=self.config.phase2_method,
                phase1_time=0.0,
                phase2_time=0.0,
                total_time=0.0,
                route_count=0,
                negative_route_count=0,
                generated_columns=[],
                best_reduced_cost=None,
                has_improving_pairing=False,
                phase1_status="no_allowed_customers",
                phase2_status="not_run",
                diagnostic_note=f"facility={facility.facility_id} has no branch-feasible customers",
            )

        backend_result = self.backend.solve(
            FacilityPricingProblem(
                instance=instance,
                facility=facility,
                duals=duals,
                node_constraints=node_constraints,
                pricing_instance=build_context.pricing_instance,
                source_node=build_context.source_node,
                sink_node=build_context.sink_node,
                customer_by_id=build_context.customer_by_id,
                base_arc_costs=build_context.base_arc_costs,
            ),
            config=self.config,
        )

        generated_columns = [
            self._column_from_routes(
                instance=instance,
                facility=facility,
                routes=candidate.routes,
                reduced_cost=candidate.reduced_cost,
                column_id=column_id_start + index,
                iteration_index=iteration_index,
                pricing_engine=candidate.pricing_engine,
                metadata=candidate.metadata,
            )
            for index, candidate in enumerate(backend_result.generated_columns)
        ]

        return FacilityPricingResult(
            facility_id=facility.facility_id,
            phase2_method=self.config.phase2_method,
            phase1_time=backend_result.phase1_time,
            phase2_time=backend_result.phase2_time,
            total_time=backend_result.total_time,
            route_count=backend_result.route_count,
            negative_route_count=backend_result.negative_route_count,
            generated_columns=generated_columns,
            best_reduced_cost=backend_result.best_reduced_cost,
            has_improving_pairing=bool(generated_columns),
            phase1_status=backend_result.phase1_status,
            phase2_status=backend_result.phase2_status,
            diagnostic_note=backend_result.diagnostic_note,
        )

    def _column_from_routes(
        self,
        *,
        instance: AkcaLRSPInstance,
        facility: LRSPFacility,
        routes: Sequence[Route | object],
        reduced_cost: float,
        column_id: int,
        iteration_index: int,
        pricing_engine: str,
        metadata: Dict[str, float | int | str | bool],
    ) -> LRSPPairingColumn:
        normalized_routes = tuple(
            route if isinstance(route, Route) else Route.from_route_like(route, -1)
            for route in routes
        )
        covered_customers = tuple(
            sorted(
                {
                    customer_id
                    for route in normalized_routes
                    for customer_id in route.covered_customers
                }
            )
        )
        customer_lookup = instance.customer_by_id()
        total_demand = sum(customer_lookup[customer_id].demand for customer_id in covered_customers)
        total_route_time = sum(
            self._actual_route_travel_cost(instance, facility, route)
            for route in normalized_routes
        )
        pairing_cost = instance.vehicle_fixed_cost + total_route_time

        merged_metadata: Dict[str, float | int | str | bool] = {
            "vehicle_fixed_cost": instance.vehicle_fixed_cost,
            "travel_cost": total_route_time,
        }
        merged_metadata.update(metadata)

        return LRSPPairingColumn(
            column_id=column_id,
            facility_id=facility.facility_id,
            covered_customers=covered_customers,
            pairing_cost=pairing_cost,
            reduced_cost=reduced_cost,
            total_demand=total_demand,
            total_duty_time=total_route_time,
            total_route_time=total_route_time,
            route_count=len(normalized_routes),
            constituent_routes=normalized_routes,
            pricing_engine=pricing_engine,
            source_iteration=iteration_index,
            metadata=merged_metadata,
        )

    def _actual_route_travel_cost(
        self,
        instance: AkcaLRSPInstance,
        facility: LRSPFacility,
        route: Route,
    ) -> float:
        customer_lookup = instance.customer_by_id()
        sink_node = max(customer_lookup) + 1

        def coords(node_id: int) -> tuple[float, float]:
            if node_id == 0 or node_id == sink_node:
                return facility.x, facility.y
            customer = customer_lookup[node_id]
            return customer.x, customer.y

        total = 0.0
        for tail, head in zip(route.path, route.path[1:]):
            x1, y1 = coords(tail)
            x2, y2 = coords(head)
            total += instance.vehicle_operating_cost * rounded_euclidean_distance(x1, y1, x2, y2)
        return total
