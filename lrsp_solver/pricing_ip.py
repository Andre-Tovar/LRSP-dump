from __future__ import annotations

from time import perf_counter
from typing import List, Sequence, Tuple

from mespprc import (
    GeneratedRoute,
    MESPPRCInstance,
    Phase1Result,
    Phase1Solver,
    Phase2IPSolver,
)

from .column import Column
from .pricing_dp import _route_to_column, _routes_to_pairing_column
from .pricing_graph import (
    FacilityPricingGraph,
    build_facility_pricing_graph,
)
from .pricing_interface import PricingProblem, PricingResult, PricingSolver


class MESPPRCIPPricingSolver(PricingSolver):
    """
    LRSP pricing engine that uses the user's IP-based MESPPRC stack.

    The pipeline mirrors the DP pricing solver, but the route-combining stage uses
    `Phase2IPSolver` (a PuLP/CBC set-partitioning IP) instead of the route-network DP.
    This is the variant the user wants to benchmark against the DP variant.
    """

    name = "mespprc_ip"

    def solve(self, problem: PricingProblem) -> PricingResult:
        start = perf_counter()
        graph = build_facility_pricing_graph(
            problem.instance, problem.facility, problem.duals
        )

        phase1_solver = Phase1Solver(
            graph.pricing_instance, label_limit=problem.config.phase1_label_limit
        )
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

        pairing_column = _maybe_build_pairing_column_ip(
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


def _negative_routes(phase1_result: Phase1Result, tolerance: float) -> List[GeneratedRoute]:
    routes = [r for r in phase1_result.feasible_routes if r.cost < -tolerance]
    routes.sort(key=lambda r: r.cost)
    return routes


def _maybe_build_pairing_column_ip(
    *,
    problem: PricingProblem,
    graph: FacilityPricingGraph,
    phase1_result: Phase1Result,
    seen_signatures: set[tuple[object, ...]],
    next_column_id: int,
    engine_name: str,
) -> Column | None:
    if problem.instance.vehicle_time_limit is None:
        return None

    negative_routes = [
        r for r in phase1_result.feasible_routes if r.cost < -problem.config.improvement_tolerance
    ]
    if len(negative_routes) < 2:
        return None

    solver = Phase2IPSolver(graph.pricing_instance)
    try:
        result = solver.solve(negative_routes, collect_diagnostics=False)
    except Exception:
        return None

    if (
        not result.is_feasible
        or result.objective_value is None
        or not result.selected_routes
    ):
        return None

    return _routes_to_pairing_column(
        instance=problem.instance,
        facility=problem.facility,
        graph=graph,
        routes=list(result.selected_routes),
        phase2_objective=float(result.objective_value),
        column_id=next_column_id,
        iteration=problem.iteration,
        pricing_engine=engine_name,
        seen_signatures=seen_signatures,
    )
