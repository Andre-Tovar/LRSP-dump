from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence, Set, Tuple

from pulp import (
    LpBinary,
    LpMinimize,
    LpProblem,
    LpStatus,
    LpVariable,
    PULP_CBC_CMD,
    lpSum,
    value,
)

from .instance import MESPPRCInstance
from .phase1 import Phase1Result
from .route import (
    GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING,
    NO_IMPROVING_PAIRING_STATUS,
    NO_NEGATIVE_ROUTES_FROM_PHASE1,
    OPTIMAL_STATUS,
    Route,
    RouteReductionRecord,
    reduce_pricing_route_pool,
)


@dataclass(slots=True)
class Phase2IPDiagnostics:
    """
    Diagnostics for the LRSP pairing-selection IP.
    """

    input_route_count: int
    negative_route_count: int
    reduced_route_count: int
    overlap_constraint_count: int
    global_constraint_count: int
    route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]]
    route_ids_by_first_customer: Dict[int | None, List[int]]
    route_ids_by_cover_and_signature: Dict[
        Tuple[Tuple[int, ...], Tuple[int, ...]],
        List[int],
    ]
    globally_infeasible_single_route_ids: List[int]
    reduction_records: List[RouteReductionRecord]
    kept_route_ids: List[int]
    removed_route_ids: List[int]
    selected_route_ids: List[int]
    route_usage_summary: Dict[int, int]
    infeasibility_driver: str | None
    diagnostic_summary: str


@dataclass(slots=True)
class Phase2IPResult:
    """
    Result of the LRSP Phase 2 pairing-selection IP.
    """

    mode: str
    service_rule: str
    status: str
    is_feasible: bool
    infeasibility_reason: str | None
    objective_value: Optional[float]
    total_cost: Optional[float]
    selected_route_ids: List[int]
    selected_routes: List[Route]
    covered_customers: Set[int]
    has_negative_pairing: bool
    has_improving_pairing: bool
    variable_count: int
    overlap_constraint_count: int
    global_constraint_count: int
    solver_status: str
    diagnostics: Phase2IPDiagnostics | None

    @property
    def best_pairing(self) -> List[Route]:
        return self.selected_routes


class Phase2IPPricingSolver:
    """
    IP alternative for the Phase 2 LRSP pairing problem.

    The model chooses a nonempty subset of negative reduced-cost routes, minimizes total
    reduced cost, forbids overlapping-customer route pairs, and enforces global pairing
    resource limits for one vehicle schedule.
    """

    mode = "lrsp_ip_pairing"
    service_rule = "single_facility_pairing"

    def __init__(self, instance: MESPPRCInstance) -> None:
        self.instance = instance
        self.global_limits: List[float] = []

    def solve(
        self,
        routes: Sequence[Route | object] | Phase1Result | object,
        *,
        collect_diagnostics: bool = True,
    ) -> Phase2IPResult:
        self.instance.validate()
        self.global_limits = list(self.instance.global_limits)

        original_routes = self._normalize_routes(self._coerce_routes(routes))
        negative_routes = [route for route in original_routes if route.cost < 0.0]
        reduced_routes, reduction_records = reduce_pricing_route_pool(negative_routes)

        if not reduced_routes:
            diagnostics = (
                self._build_diagnostics(
                    original_routes=original_routes,
                    negative_routes=negative_routes,
                    reduced_routes=reduced_routes,
                    reduction_records=reduction_records,
                    selected_routes=[],
                    overlap_pairs=[],
                    solver_status="NotSolved",
                    infeasibility_reason=NO_NEGATIVE_ROUTES_FROM_PHASE1,
                )
                if collect_diagnostics
                else None
            )
            return self._build_no_pairing_result(
                reason=NO_NEGATIVE_ROUTES_FROM_PHASE1,
                reduced_routes=reduced_routes,
                overlap_pairs=[],
                solver_status="NotSolved",
                diagnostics=diagnostics,
            )

        overlap_pairs = self._overlap_pairs(reduced_routes)
        model = LpProblem("mespprc_lrsp_phase2_pricing", LpMinimize)
        route_vars = [
            LpVariable(name=f"x_{idx}", lowBound=0, upBound=1, cat=LpBinary)
            for idx in range(len(reduced_routes))
        ]

        model += lpSum(
            reduced_routes[idx].cost * route_vars[idx]
            for idx in range(len(reduced_routes))
        )
        model += lpSum(route_vars) >= 1, "nonempty_selection"

        for pair_idx, (i, j) in enumerate(overlap_pairs):
            model += route_vars[i] + route_vars[j] <= 1, f"overlap_{pair_idx}"

        for dim, limit in enumerate(self.global_limits):
            model += (
                lpSum(
                    reduced_routes[idx].global_resources[dim] * route_vars[idx]
                    for idx in range(len(reduced_routes))
                )
                <= limit,
                f"global_{dim}",
            )

        status_code = model.solve(PULP_CBC_CMD(msg=False))
        del status_code
        status_name = LpStatus[model.status]
        self._validate_solver_status(status_name)

        if status_name == "Infeasible":
            diagnostics = (
                self._build_diagnostics(
                    original_routes=original_routes,
                    negative_routes=negative_routes,
                    reduced_routes=reduced_routes,
                    reduction_records=reduction_records,
                    selected_routes=[],
                    overlap_pairs=overlap_pairs,
                    solver_status=status_name,
                    infeasibility_reason=GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING,
                )
                if collect_diagnostics
                else None
            )
            return self._build_no_pairing_result(
                reason=GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING,
                reduced_routes=reduced_routes,
                overlap_pairs=overlap_pairs,
                solver_status=status_name,
                diagnostics=diagnostics,
            )

        selected_indices = [
            idx for idx, route_var in enumerate(route_vars) if value(route_var) > 0.5
        ]
        selected_routes = [reduced_routes[idx] for idx in selected_indices]
        objective_value = value(model.objective)
        if objective_value is None:
            raise RuntimeError("Phase 2 IP did not produce an objective value.")

        diagnostics = (
            self._build_diagnostics(
                original_routes=original_routes,
                negative_routes=negative_routes,
                reduced_routes=reduced_routes,
                reduction_records=reduction_records,
                selected_routes=selected_routes,
                overlap_pairs=overlap_pairs,
                solver_status=status_name,
                infeasibility_reason=None,
            )
            if collect_diagnostics
            else None
        )
        covered_customers = self._covered_customer_set(selected_routes)

        return Phase2IPResult(
            mode=self.mode,
            service_rule=self.service_rule,
            status=OPTIMAL_STATUS,
            is_feasible=True,
            infeasibility_reason=None,
            objective_value=objective_value,
            total_cost=objective_value,
            selected_route_ids=[route.route_id for route in selected_routes],
            selected_routes=selected_routes,
            covered_customers=covered_customers,
            has_negative_pairing=objective_value < 0.0,
            has_improving_pairing=objective_value < 0.0,
            variable_count=len(reduced_routes),
            overlap_constraint_count=len(overlap_pairs),
            global_constraint_count=len(self.global_limits),
            solver_status=status_name,
            diagnostics=diagnostics,
        )

    def _coerce_routes(
        self,
        routes: Sequence[Route | object] | Phase1Result | object,
    ) -> List[Route | object]:
        if isinstance(routes, Phase1Result):
            return list(routes.negative_cost_routes)
        if hasattr(routes, "negative_routes"):
            return list(getattr(routes, "negative_routes"))
        if hasattr(routes, "negative_cost_routes"):
            return list(getattr(routes, "negative_cost_routes"))
        if hasattr(routes, "exported_routes"):
            return list(getattr(routes, "exported_routes"))
        if hasattr(routes, "feasible_routes"):
            return list(getattr(routes, "feasible_routes"))
        return list(routes)

    def _normalize_routes(self, routes: Sequence[Route | object]) -> List[Route]:
        normalized = [
            route if isinstance(route, Route) else Route.from_route_like(route, idx)
            for idx, route in enumerate(routes, start=1)
        ]

        route_ids = [route.route_id for route in normalized]
        if len(route_ids) != len(set(route_ids)):
            raise ValueError("Phase 2 pricing requires unique route ids.")

        known_customers = set(self.instance.customers())
        local_dim = len(self.instance.local_limits)
        global_dim = len(self.global_limits)
        for route in normalized:
            unknown_customers = sorted(route.covered_customers - known_customers)
            if unknown_customers:
                raise ValueError(
                    f"Route {route.route_id} covers unknown customer ids: {unknown_customers}."
                )
            if len(route.local_resources) != local_dim:
                raise ValueError(
                    f"Route {route.route_id} local resource dimension "
                    f"{len(route.local_resources)} does not match instance local limits "
                    f"dimension {local_dim}."
                )
            if len(route.global_resources) != global_dim:
                raise ValueError(
                    f"Route {route.route_id} global resource dimension "
                    f"{len(route.global_resources)} does not match instance global limits "
                    f"dimension {global_dim}."
                )

        return normalized

    def _overlap_pairs(self, routes: Sequence[Route]) -> List[Tuple[int, int]]:
        overlap_pairs: List[Tuple[int, int]] = []
        for i, route in enumerate(routes):
            for j in range(i + 1, len(routes)):
                if not route.covered_customers.isdisjoint(routes[j].covered_customers):
                    overlap_pairs.append((i, j))
        return overlap_pairs

    def _build_diagnostics(
        self,
        *,
        original_routes: Sequence[Route],
        negative_routes: Sequence[Route],
        reduced_routes: Sequence[Route],
        reduction_records: Sequence[RouteReductionRecord],
        selected_routes: Sequence[Route],
        overlap_pairs: Sequence[Tuple[int, int]],
        solver_status: str,
        infeasibility_reason: str | None,
    ) -> Phase2IPDiagnostics:
        route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]] = defaultdict(list)
        route_ids_by_first_customer: Dict[int | None, List[int]] = defaultdict(list)
        route_ids_by_cover_and_signature: Dict[
            Tuple[Tuple[int, ...], Tuple[int, ...]],
            List[int],
        ] = defaultdict(list)

        for route in reduced_routes:
            cover_key = tuple(sorted(route.covered_customers))
            route_ids_by_covered_customer_set[cover_key].append(route.route_id)
            route_ids_by_first_customer[route.first_customer_in_route].append(route.route_id)
            route_ids_by_cover_and_signature[
                (cover_key, tuple(route.customer_state_signature))
            ].append(route.route_id)

        selected_route_ids = [route.route_id for route in selected_routes]
        route_usage_summary = {
            route.route_id: int(route.route_id in selected_route_ids)
            for route in reduced_routes
        }

        globally_infeasible_single_route_ids = sorted(
            route.route_id
            for route in reduced_routes
            if self.global_limits
            and not self._within_limits(list(route.global_resources), self.global_limits)
        )

        if infeasibility_reason == NO_NEGATIVE_ROUTES_FROM_PHASE1:
            summary = (
                "Phase 1 produced no negative reduced-cost routes for LRSP pairing generation."
            )
        elif infeasibility_reason == GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING:
            summary = (
                "Negative reduced-cost routes exist, but the global pairing-level "
                "resource limits prevent any nonempty feasible pairing."
            )
        else:
            summary = (
                "Phase 2 LRSP pairing IP solved a reduced negative-route pool with "
                f"{len(reduced_routes)} variables and {len(overlap_pairs)} overlap constraints."
            )

        return Phase2IPDiagnostics(
            input_route_count=len(original_routes),
            negative_route_count=len(negative_routes),
            reduced_route_count=len(reduced_routes),
            overlap_constraint_count=len(overlap_pairs),
            global_constraint_count=len(self.global_limits),
            route_ids_by_covered_customer_set=dict(route_ids_by_covered_customer_set),
            route_ids_by_first_customer=dict(route_ids_by_first_customer),
            route_ids_by_cover_and_signature=dict(route_ids_by_cover_and_signature),
            globally_infeasible_single_route_ids=globally_infeasible_single_route_ids,
            reduction_records=list(reduction_records),
            kept_route_ids=[route.route_id for route in reduced_routes],
            removed_route_ids=[record.removed_route_id for record in reduction_records],
            selected_route_ids=selected_route_ids,
            route_usage_summary=route_usage_summary,
            infeasibility_driver=infeasibility_reason,
            diagnostic_summary=f"{summary} Solver status: {solver_status}.",
        )

    def _build_no_pairing_result(
        self,
        *,
        reason: str,
        reduced_routes: Sequence[Route],
        overlap_pairs: Sequence[Tuple[int, int]],
        solver_status: str,
        diagnostics: Phase2IPDiagnostics | None,
    ) -> Phase2IPResult:
        return Phase2IPResult(
            mode=self.mode,
            service_rule=self.service_rule,
            status=NO_IMPROVING_PAIRING_STATUS,
            is_feasible=False,
            infeasibility_reason=reason,
            objective_value=None,
            total_cost=None,
            selected_route_ids=[],
            selected_routes=[],
            covered_customers=set(),
            has_negative_pairing=False,
            has_improving_pairing=False,
            variable_count=len(reduced_routes),
            overlap_constraint_count=len(overlap_pairs),
            global_constraint_count=len(self.global_limits),
            solver_status=solver_status,
            diagnostics=diagnostics,
        )

    @staticmethod
    def _validate_solver_status(status_name: str) -> None:
        if status_name not in {"Optimal", "Infeasible"}:
            raise RuntimeError(
                "Unexpected Phase 2 IP solver status. "
                f"Expected Optimal or Infeasible, got {status_name!r}."
            )

    @staticmethod
    def _within_limits(vec: List[float], limits: List[float]) -> bool:
        return len(vec) == len(limits) and all(v <= lim for v, lim in zip(vec, limits))

    @staticmethod
    def _covered_customer_set(routes: Sequence[Route]) -> Set[int]:
        covered: Set[int] = set()
        for route in routes:
            covered.update(route.covered_customers)
        return covered


Phase2IPSolver = Phase2IPPricingSolver
LRSPPhase2IPDiagnostics = Phase2IPDiagnostics
LRSPPairingIPResult = Phase2IPResult
LRSPPhase2IPPricingSolver = Phase2IPPricingSolver
