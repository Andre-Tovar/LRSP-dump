from __future__ import annotations

from time import perf_counter
from typing import List, Sequence, Tuple

from mespprc import (
    GeneratedRoute,
    MESPPRCInstance,
    Phase1Result,
    Phase1Solver,
    Phase2DPSolver,
)

from .column import Column
from .pricing_graph import (
    FacilityPricingGraph,
    SOURCE_NODE_ID,
    actual_route_travel_cost,
    build_facility_pricing_graph,
)
from .pricing_interface import PricingProblem, PricingResult, PricingSolver


class MESPPRCDPPricingSolver(PricingSolver):
    """
    LRSP pricing engine that uses the user's DP-based MESPPRC stack.

    Pipeline:
    - Build the per-facility reduced-cost graph from current master duals.
    - Run Phase 1 (label-setting DP) to enumerate elementary routes; their `cost`
      attribute is already the reduced cost because the arc costs in the pricing
      graph are dual-adjusted.
    - Emit each negative-reduced-cost Phase 1 route as a singleton column.
    - When the instance has a vehicle duty-time limit, run Phase 2 DP (route-network
      DP) to combine routes into one negative-reduced-cost pairing column.

    The pricing engine is intentionally conservative: it returns at most
    `max_columns_per_facility` columns per facility per call.
    """

    name = "mespprc_dp"

    def solve(self, problem: PricingProblem) -> PricingResult:
        start = perf_counter()
        graph = build_facility_pricing_graph(
            problem.instance, problem.facility, problem.duals
        )

        phase1_solver = _build_phase1(graph.pricing_instance, problem.config.phase1_label_limit)
        phase1_result = phase1_solver.solve()

        improving_routes = _negative_routes(phase1_result, problem.config.improvement_tolerance)
        next_id = problem.next_column_id_start
        columns: List[Column] = []
        best_reduced_cost: float | None = None
        seen_signatures: set[tuple[object, ...]] = set()

        for route in improving_routes[: problem.config.max_columns_per_facility]:
            column = _route_to_column(
                instance=problem.instance,
                facility=problem.facility,
                graph=graph,
                route=route,
                column_id=next_id,
                iteration=problem.iteration,
                pricing_engine=self.name,
                kind="phase1_route",
            )
            sig = column.signature()
            if sig in seen_signatures:
                continue
            seen_signatures.add(sig)
            columns.append(column)
            best_reduced_cost = (
                column.reduced_cost
                if best_reduced_cost is None
                else min(best_reduced_cost, column.reduced_cost)
            )
            next_id += 1

        pairing_column = _maybe_build_pairing_column_dp(
            problem=problem,
            graph=graph,
            phase1_result=phase1_result,
            seen_signatures=seen_signatures,
            next_column_id=next_id,
            engine_name=self.name,
        )
        if pairing_column is not None:
            columns.append(pairing_column)
            best_reduced_cost = (
                pairing_column.reduced_cost
                if best_reduced_cost is None
                else min(best_reduced_cost, pairing_column.reduced_cost)
            )

        return PricingResult(
            facility_id=problem.facility.id,
            columns=columns,
            pricing_time=perf_counter() - start,
            best_reduced_cost=best_reduced_cost,
            status="ok",
            diagnostics={
                "phase1_route_count": len(phase1_result.feasible_routes),
                "phase1_negative_count": len(improving_routes),
                "pairing_column_added": pairing_column is not None,
            },
        )


def _build_phase1(pricing_instance: MESPPRCInstance, label_limit: int | None) -> Phase1Solver:
    return Phase1Solver(pricing_instance, label_limit=label_limit)


def _negative_routes(phase1_result: Phase1Result, tolerance: float) -> List[GeneratedRoute]:
    routes = [r for r in phase1_result.feasible_routes if r.cost < -tolerance]
    routes.sort(key=lambda r: r.cost)
    return routes


def _route_to_column(
    *,
    instance,
    facility,
    graph: FacilityPricingGraph,
    route: GeneratedRoute,
    column_id: int,
    iteration: int,
    pricing_engine: str,
    kind: str,
) -> Column:
    travel_cost = actual_route_travel_cost(
        instance=instance,
        facility=facility,
        path=list(route.path),
        sink_node=graph.sink_node,
    )
    pairing_cost = instance.vehicle_fixed_cost + travel_cost
    total_demand = sum(graph.customer_by_id[c].demand for c in route.covered_customers)
    reduced_cost = float(route.cost) + graph.pairing_constant
    covered = tuple(sorted(route.covered_customers))
    return Column(
        column_id=column_id,
        facility_id=facility.id,
        covered_customers=covered,
        pairing_cost=pairing_cost,
        reduced_cost=reduced_cost,
        total_demand=total_demand,
        total_travel_cost=travel_cost,
        routes=(tuple(route.path),),
        pricing_engine=pricing_engine,
        iteration=iteration,
        metadata={"kind": kind, "phase1_cost": float(route.cost)},
    )


def _maybe_build_pairing_column_dp(
    *,
    problem: PricingProblem,
    graph: FacilityPricingGraph,
    phase1_result: Phase1Result,
    seen_signatures: set[tuple[object, ...]],
    next_column_id: int,
    engine_name: str,
) -> Column | None:
    """
    Run Phase 2 DP on the routes from Phase 1 and turn the resulting pairing into a
    column when it strictly improves on the best singleton column.

    This is only attempted when:
    - the LRSP instance declares a `vehicle_time_limit`, so a global duty-time limit
      is meaningful, and
    - Phase 1 produced at least two negative-reduced-cost routes worth combining.
    """

    if problem.instance.vehicle_time_limit is None:
        return None

    negative_routes = [r for r in phase1_result.feasible_routes if r.cost < -problem.config.improvement_tolerance]
    if len(negative_routes) < 2:
        return None

    solver = Phase2DPSolver(graph.pricing_instance)
    try:
        result = solver.solve(negative_routes)
    except Exception:
        return None

    if not result.is_feasible or result.total_cost is None or not result.selected_routes:
        return None

    pairing_routes = list(result.selected_routes)
    return _routes_to_pairing_column(
        instance=problem.instance,
        facility=problem.facility,
        graph=graph,
        routes=pairing_routes,
        phase2_objective=float(result.total_cost),
        column_id=next_column_id,
        iteration=problem.iteration,
        pricing_engine=engine_name,
        seen_signatures=seen_signatures,
    )


def _routes_to_pairing_column(
    *,
    instance,
    facility,
    graph: FacilityPricingGraph,
    routes: Sequence[object],
    phase2_objective: float,
    column_id: int,
    iteration: int,
    pricing_engine: str,
    seen_signatures: set[tuple[object, ...]],
) -> Column | None:
    covered: set[int] = set()
    paths: List[Tuple[int, ...]] = []
    travel_cost = 0.0
    for route in routes:
        path = list(getattr(route, "path", []))
        if not path:
            continue
        paths.append(tuple(path))
        covered.update(getattr(route, "covered_customers", set()))
        travel_cost += actual_route_travel_cost(
            instance=instance,
            facility=facility,
            path=path,
            sink_node=graph.sink_node,
        )
    if not paths:
        return None

    pairing_cost = instance.vehicle_fixed_cost * len(paths) + travel_cost
    total_demand = sum(graph.customer_by_id[c].demand for c in covered)
    reduced_cost = phase2_objective + graph.pairing_constant

    candidate = Column(
        column_id=column_id,
        facility_id=facility.id,
        covered_customers=tuple(sorted(covered)),
        pairing_cost=pairing_cost,
        reduced_cost=reduced_cost,
        total_demand=total_demand,
        total_travel_cost=travel_cost,
        routes=tuple(paths),
        pricing_engine=pricing_engine,
        iteration=iteration,
        metadata={"kind": "phase2_pairing", "phase2_objective": phase2_objective},
    )
    sig = candidate.signature()
    if sig in seen_signatures:
        return None
    seen_signatures.add(sig)
    if candidate.reduced_cost >= -1e-6:
        return None
    return candidate
