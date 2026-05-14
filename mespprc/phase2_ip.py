from __future__ import annotations

from dataclasses import dataclass
from math import inf
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
from .label import Label, TEMP_UNREACHABLE
from .phase1 import Phase1Result
from .route import (
    GLOBAL_LIMITS_INFEASIBLE,
    INFEASIBLE_STATUS,
    OPTIMAL_STATUS,
    ROUTE_SET_INFEASIBLE,
    Route,
    RouteReductionRecord,
)


@dataclass(slots=True)
class Phase2IPRoutePoolDiagnostics:
    customer_to_route_ids: Dict[int, List[int]]
    customer_to_individually_global_feasible_route_ids: Dict[int, List[int]]
    uncovered_by_generated_routes: Set[int]
    uncovered_by_individually_global_feasible_routes: Set[int]
    route_ids_by_first_customer: Dict[int | None, List[int]]
    route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]]
    route_ids_by_cover_and_structural_signature: Dict[
        Tuple[Tuple[int, ...], Tuple[int, ...]],
        List[int],
    ]
    original_route_pool_customer_to_route_ids: Dict[int, List[int]]
    reduced_route_pool_customer_to_route_ids: Dict[int, List[int]]
    original_route_pool_customer_to_individually_global_feasible_route_ids: Dict[
        int,
        List[int],
    ]
    reduced_route_pool_customer_to_individually_global_feasible_route_ids: Dict[
        int,
        List[int],
    ]
    original_route_pool_uncovered_customers: Set[int]
    reduced_route_pool_uncovered_customers: Set[int]
    original_route_pool_uncovered_by_individually_global_feasible_routes: Set[int]
    reduced_route_pool_uncovered_by_individually_global_feasible_routes: Set[int]
    original_route_pool_route_ids_by_first_customer: Dict[int | None, List[int]]
    reduced_route_pool_route_ids_by_first_customer: Dict[int | None, List[int]]
    original_route_pool_route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]]
    reduced_route_pool_route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]]
    original_route_pool_route_ids_by_cover_and_structural_signature: Dict[
        Tuple[Tuple[int, ...], Tuple[int, ...]],
        List[int],
    ]
    reduced_route_pool_route_ids_by_cover_and_structural_signature: Dict[
        Tuple[Tuple[int, ...], Tuple[int, ...]],
        List[int],
    ]
    covered_customer_sets_with_structural_variants: Dict[
        Tuple[int, ...],
        List[Tuple[int, ...]],
    ]
    customer_temp_unreachable_counts: Dict[int, int]
    customer_perm_unreachable_counts: Dict[int, int]
    original_route_pool_customer_temp_unreachable_counts: Dict[int, int]
    reduced_route_pool_customer_temp_unreachable_counts: Dict[int, int]
    original_route_pool_customer_perm_unreachable_counts: Dict[int, int]
    reduced_route_pool_customer_perm_unreachable_counts: Dict[int, int]
    customers_with_low_route_support: Set[int]
    customers_only_in_structurally_weak_routes: Set[int]
    bottleneck_customers: Set[int]
    route_classes_by_covered_set: Dict[Tuple[int, ...], List[Dict[str, object]]]
    reduction_records: List[RouteReductionRecord]
    dominance_reduction_summary: Dict[str, object]
    kept_route_ids: List[int]
    removed_route_ids: List[int]
    infeasibility_driver: str | None
    diagnostic_summary: str


@dataclass(slots=True)
class Phase2IPDiagnostics:
    route_pool_diagnostics: Phase2IPRoutePoolDiagnostics
    reduced_route_pool_used: bool
    original_route_count: int
    reduced_route_count: int
    customers_with_no_supporting_routes: Set[int]
    uncovered_customers: Set[int]
    route_usage_summary: Dict[int, int]
    selected_customer_coverage_counts: Dict[int, int]
    global_resource_usage: Optional[List[float]]
    global_resource_slacks: Optional[List[float]]
    minimum_resource_usage_to_cover: Optional[List[float]]
    resource_constraint_violations: Dict[int, float]
    resource_constraint_note: str | None
    solver_status: str
    diagnostic_summary: str


@dataclass(slots=True)
class Phase2IPResult:
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
    required_customers: Set[int]
    coverage_complete: bool
    original_route_count: int
    reduced_route_count: int
    variable_count: int
    coverage_constraint_count: int
    global_constraint_count: int
    solver_status: str
    diagnostics: Phase2IPDiagnostics | None


@dataclass(slots=True)
class _IPModelResult:
    status_name: str
    objective_value: Optional[float]
    selected_route_indices: Tuple[int, ...]

    @property
    def is_optimal(self) -> bool:
        return self.status_name == "Optimal"

    @property
    def is_infeasible(self) -> bool:
        return self.status_name == "Infeasible"


@dataclass(slots=True)
class _PreparedRoutePool:
    original_routes: List[Route]
    reduced_routes: List[Route]
    reduction_records: List[RouteReductionRecord]
    required_customer_sets: List[Tuple[int, ...]]
    customer_to_route_indices: Dict[int, List[int]]
    route_costs: List[float]
    global_resource_matrix: List[List[float]]


class Phase2IPSolver:
    mode = "ip_exact_covering"
    service_rule = "exact_once"

    def __init__(self, instance: MESPPRCInstance) -> None:
        self.instance = instance
        self.all_customer_ids: List[int] = []
        self.all_customer_index: Dict[int, int] = {}
        self.required_customer_ids: List[int] = []
        self.required_customer_set: Set[int] = set()
        self.global_limits: List[float] = []

    def solve(
        self,
        routes: Sequence[Route | object] | Phase1Result | object,
        *,
        collect_diagnostics: bool = True,
    ) -> Phase2IPResult:
        self.instance.validate()
        if not self.instance.exact_once_service:
            raise NotImplementedError(
                "This Phase 2 solver currently supports exact_once_service=True only."
            )

        self._refresh_context()
        original_routes = self._normalize_routes(self._coerce_routes(routes))
        prepared = self._prepare_route_pool(original_routes)
        uncovered_customers = {
            customer_id
            for customer_id, route_indices in prepared.customer_to_route_indices.items()
            if not route_indices
        }

        if uncovered_customers:
            diagnostics = self._build_ip_diagnostics(
                prepared=prepared,
                selected_routes=[],
                solver_status="NotSolved",
                minimum_resource_usage_to_cover=None,
                resource_constraint_violations={},
                resource_constraint_note=None,
                infeasibility_reason=ROUTE_SET_INFEASIBLE,
            ) if collect_diagnostics else None
            return self._build_infeasible_result(
                reason=ROUTE_SET_INFEASIBLE,
                diagnostics=diagnostics,
                prepared=prepared,
                solver_status="NotSolved",
            )

        relaxed_result = self._solve_ip_model(
            prepared=prepared,
            include_global_limits=False,
            objective="cost",
        )
        self._validate_solver_status(relaxed_result.status_name, "relaxed Phase 2 IP")
        if relaxed_result.is_infeasible:
            diagnostics = self._build_ip_diagnostics(
                prepared=prepared,
                selected_routes=[],
                solver_status=relaxed_result.status_name,
                minimum_resource_usage_to_cover=None,
                resource_constraint_violations={},
                resource_constraint_note=None,
                infeasibility_reason=ROUTE_SET_INFEASIBLE,
            ) if collect_diagnostics else None
            return self._build_infeasible_result(
                reason=ROUTE_SET_INFEASIBLE,
                diagnostics=diagnostics,
                prepared=prepared,
                solver_status=relaxed_result.status_name,
            )

        constrained_result = self._solve_ip_model(
            prepared=prepared,
            include_global_limits=True,
            objective="cost",
        )
        self._validate_solver_status(
            constrained_result.status_name,
            "constrained Phase 2 IP",
        )
        if constrained_result.is_infeasible:
            min_resource_usage = (
                self._minimum_resource_usage_to_cover(prepared)
                if collect_diagnostics
                else None
            )
            resource_constraint_violations = (
                self._resource_constraint_violations(min_resource_usage)
                if collect_diagnostics
                else {}
            )
            diagnostics = self._build_ip_diagnostics(
                prepared=prepared,
                selected_routes=[],
                solver_status=constrained_result.status_name,
                minimum_resource_usage_to_cover=min_resource_usage,
                resource_constraint_violations=resource_constraint_violations,
                resource_constraint_note=self._resource_constraint_note(
                    min_resource_usage=min_resource_usage,
                    resource_constraint_violations=resource_constraint_violations,
                ) if collect_diagnostics else None,
                infeasibility_reason=GLOBAL_LIMITS_INFEASIBLE,
            ) if collect_diagnostics else None
            return self._build_infeasible_result(
                reason=GLOBAL_LIMITS_INFEASIBLE,
                diagnostics=diagnostics,
                prepared=prepared,
                solver_status=constrained_result.status_name,
            )

        selected_routes = [
            prepared.reduced_routes[idx]
            for idx in constrained_result.selected_route_indices
        ]
        covered_customers = self._covered_customer_set(selected_routes)
        diagnostics = self._build_ip_diagnostics(
            prepared=prepared,
            selected_routes=selected_routes,
            solver_status=constrained_result.status_name,
            minimum_resource_usage_to_cover=None,
            resource_constraint_violations={},
            resource_constraint_note=None,
            infeasibility_reason=None,
        ) if collect_diagnostics else None
        objective_value = constrained_result.objective_value

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
            required_customers=set(self.required_customer_ids),
            coverage_complete=set(self.required_customer_ids).issubset(covered_customers),
            original_route_count=len(prepared.original_routes),
            reduced_route_count=len(prepared.reduced_routes),
            variable_count=len(prepared.reduced_routes),
            coverage_constraint_count=len(self.required_customer_ids),
            global_constraint_count=len(self.global_limits),
            solver_status=constrained_result.status_name,
            diagnostics=diagnostics,
        )

    def _refresh_context(self) -> None:
        self.required_customer_ids = self.instance.required_customers()
        self.required_customer_set = set(self.required_customer_ids)
        self.all_customer_ids = self.instance.customers()
        self.all_customer_index = {
            customer_id: idx for idx, customer_id in enumerate(self.all_customer_ids)
        }
        self.global_limits = list(self.instance.global_limits)

    def _coerce_routes(
        self,
        routes: Sequence[Route | object] | Phase1Result | object,
    ) -> List[Route | object]:
        if isinstance(routes, Phase1Result):
            return list(routes.exported_routes)
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
            raise ValueError("Phase 2 requires unique route ids.")

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

    def _prepare_route_pool(self, original_routes: Sequence[Route]) -> _PreparedRoutePool:
        reduced_routes, reduction_records = self._reduce_route_pool(original_routes)
        required_customer_sets = [
            tuple(sorted(self._required_covered_customers(route)))
            for route in reduced_routes
        ]
        customer_to_route_indices = {
            customer_id: [] for customer_id in self.required_customer_ids
        }
        for route_idx, covered_customers in enumerate(required_customer_sets):
            for customer_id in covered_customers:
                customer_to_route_indices[customer_id].append(route_idx)

        global_resource_matrix = [
            [route.global_resources[dim] for route in reduced_routes]
            for dim in range(len(self.global_limits))
        ]
        return _PreparedRoutePool(
            original_routes=list(original_routes),
            reduced_routes=reduced_routes,
            reduction_records=reduction_records,
            required_customer_sets=required_customer_sets,
            customer_to_route_indices=customer_to_route_indices,
            route_costs=[route.cost for route in reduced_routes],
            global_resource_matrix=global_resource_matrix,
        )

    def route_has_full_customer_signature(self, route: Route) -> bool:
        return len(route.customer_state_signature) == len(self.all_customer_ids)

    def route_visited_customers(
        self,
        route: Route,
        *,
        required_only: bool = True,
    ) -> Set[int]:
        customers = self.required_customer_ids if required_only else self.all_customer_ids
        return {
            customer_id
            for customer_id in customers
            if Label.is_visited(self._route_customer_state(route, customer_id))
        }

    def route_reachable_uncovered_customers(
        self,
        route: Route,
        *,
        required_only: bool = True,
    ) -> Set[int]:
        customers = self.required_customer_ids if required_only else self.all_customer_ids
        return {
            customer_id
            for customer_id in customers
            if Label.is_reachable(self._route_customer_state(route, customer_id))
        }

    def route_temp_unreachable_customers(
        self,
        route: Route,
        *,
        required_only: bool = True,
    ) -> Set[int]:
        customers = self.required_customer_ids if required_only else self.all_customer_ids
        return {
            customer_id
            for customer_id in customers
            if Label.is_temp_unreachable(self._route_customer_state(route, customer_id))
        }

    def route_perm_unreachable_customers(
        self,
        route: Route,
        *,
        required_only: bool = True,
    ) -> Set[int]:
        customers = self.required_customer_ids if required_only else self.all_customer_ids
        return {
            customer_id
            for customer_id in customers
            if Label.is_perm_unreachable(self._route_customer_state(route, customer_id))
        }

    def route_structural_signature(
        self,
        route: Route,
        *,
        required_only: bool = True,
    ) -> Tuple[int, ...]:
        customers = self.required_customer_ids if required_only else self.all_customer_ids
        return tuple(
            self._phase2_customer_state_rank(self._route_customer_state(route, customer_id))
            for customer_id in customers
        )

    def _route_customer_state(self, route: Route, customer_id: int) -> int:
        if self.route_has_full_customer_signature(route):
            return route.customer_state_signature[self.all_customer_index[customer_id]]
        if customer_id in route.covered_customers:
            return 1
        return TEMP_UNREACHABLE

    def _reduce_route_pool(
        self,
        routes: Sequence[Route],
    ) -> Tuple[List[Route], List[RouteReductionRecord]]:
        reductions: List[RouteReductionRecord] = []

        filtered_routes: List[Route] = []
        duplicate_map: Dict[Tuple[Tuple[int, ...], float, Tuple[float, ...]], Route] = {}
        for route in routes:
            required_customers = tuple(sorted(self._required_covered_customers(route)))
            if (
                not required_customers
                and route.cost >= 0
                and all(value >= 0 for value in route.global_resources)
            ):
                reductions.append(
                    RouteReductionRecord(
                        kept_route_id=-1,
                        removed_route_id=route.route_id,
                        reason="covers no required customers",
                    )
                )
                continue

            duplicate_key = (
                required_customers,
                route.cost,
                tuple(route.global_resources),
            )
            kept_duplicate = duplicate_map.get(duplicate_key)
            if kept_duplicate is not None:
                reductions.append(
                    RouteReductionRecord(
                        kept_route_id=kept_duplicate.route_id,
                        removed_route_id=route.route_id,
                        reason="duplicate route under required coverage, cost, and global resources",
                    )
                )
                continue

            duplicate_map[duplicate_key] = route
            filtered_routes.append(route)

        reduced_routes: List[Route] = []
        grouped: Dict[Tuple[int, ...], List[Route]] = {}
        for route in filtered_routes:
            grouped.setdefault(
                tuple(sorted(self._required_covered_customers(route))),
                [],
            ).append(route)

        for covered_set, group_routes in grouped.items():
            del covered_set
            survivors: List[Route] = []
            for route in sorted(group_routes, key=self._route_reduction_key):
                dominated = False
                next_survivors: List[Route] = []
                for survivor in survivors:
                    if self._route_dominates_for_ip(survivor, route):
                        reductions.append(
                            RouteReductionRecord(
                                kept_route_id=survivor.route_id,
                                removed_route_id=route.route_id,
                                reason=(
                                    "same required covered set, no worse cost, and no "
                                    "worse global resources"
                                ),
                            )
                        )
                        dominated = True
                        break

                    if self._route_dominates_for_ip(route, survivor):
                        reductions.append(
                            RouteReductionRecord(
                                kept_route_id=route.route_id,
                                removed_route_id=survivor.route_id,
                                reason=(
                                    "same required covered set, no worse cost, and no "
                                    "worse global resources"
                                ),
                            )
                        )
                        continue

                    next_survivors.append(survivor)

                if not dominated:
                    next_survivors.append(route)
                    survivors = next_survivors

            reduced_routes.extend(survivors)

        reduced_routes.sort(key=self._route_reduction_key)
        return reduced_routes, reductions

    def _route_dominates_for_ip(self, a: Route, b: Route) -> bool:
        return (
            self._required_covered_customers(a) == self._required_covered_customers(b)
            and a.cost <= b.cost
            and self._route_no_worse_global_resources(a, b)
            and (
                a.cost < b.cost
                or self._vec_lt(list(a.global_resources), list(b.global_resources))
            )
        )

    @staticmethod
    def _structural_signature_no_worse(
        a: Sequence[int],
        b: Sequence[int],
    ) -> bool:
        return all(av <= bv for av, bv in zip(a, b))

    @staticmethod
    def _structural_signature_strictly_better(
        a: Sequence[int],
        b: Sequence[int],
    ) -> bool:
        return any(av < bv for av, bv in zip(a, b))

    @staticmethod
    def _route_no_worse_global_resources(a: Route, b: Route) -> bool:
        return len(a.global_resources) == len(b.global_resources) and all(
            av <= bv for av, bv in zip(a.global_resources, b.global_resources)
        )

    def _route_reduction_key(self, route: Route) -> tuple[object, ...]:
        return (
            tuple(sorted(self._required_covered_customers(route))),
            route.cost,
            tuple(route.global_resources),
            route.route_id,
        )

    def _summarize_route_pool(self, routes: Sequence[Route]) -> Dict[str, object]:
        customer_to_route_ids = {
            customer_id: [] for customer_id in self.required_customer_ids
        }
        customer_to_individually_feasible_route_ids = {
            customer_id: [] for customer_id in self.required_customer_ids
        }
        route_ids_by_first_customer: Dict[int | None, List[int]] = {}
        route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]] = {}
        route_ids_by_cover_and_structural_signature: Dict[
            Tuple[Tuple[int, ...], Tuple[int, ...]],
            List[int],
        ] = {}
        structural_variants_by_cover: Dict[Tuple[int, ...], Set[Tuple[int, ...]]] = {}
        customer_temp_unreachable_counts = {
            customer_id: 0 for customer_id in self.required_customer_ids
        }
        customer_perm_unreachable_counts = {
            customer_id: 0 for customer_id in self.required_customer_ids
        }

        for route in routes:
            required_covered = tuple(sorted(self._required_covered_customers(route)))
            structural_signature = self.route_structural_signature(route)
            for customer_id in required_covered:
                customer_to_route_ids[customer_id].append(route.route_id)
                if self._route_is_individually_global_feasible(route):
                    customer_to_individually_feasible_route_ids[customer_id].append(
                        route.route_id
                    )
            for customer_id in self.route_temp_unreachable_customers(route):
                customer_temp_unreachable_counts[customer_id] += 1
            for customer_id in self.route_perm_unreachable_customers(route):
                customer_perm_unreachable_counts[customer_id] += 1

            route_ids_by_first_customer.setdefault(
                route.first_customer_in_route,
                [],
            ).append(route.route_id)
            route_ids_by_covered_customer_set.setdefault(
                required_covered,
                [],
            ).append(route.route_id)
            route_ids_by_cover_and_structural_signature.setdefault(
                (required_covered, structural_signature),
                [],
            ).append(route.route_id)
            structural_variants_by_cover.setdefault(required_covered, set()).add(
                structural_signature
            )

        uncovered_customers = {
            customer_id
            for customer_id, route_ids in customer_to_route_ids.items()
            if not route_ids
        }
        uncovered_by_individually_global_feasible_routes = {
            customer_id
            for customer_id, route_ids in customer_to_individually_feasible_route_ids.items()
            if not route_ids
        }

        for route_ids in customer_to_route_ids.values():
            route_ids.sort()
        for route_ids in customer_to_individually_feasible_route_ids.values():
            route_ids.sort()
        for route_ids in route_ids_by_first_customer.values():
            route_ids.sort()
        for route_ids in route_ids_by_covered_customer_set.values():
            route_ids.sort()
        for route_ids in route_ids_by_cover_and_structural_signature.values():
            route_ids.sort()

        return {
            "customer_to_route_ids": customer_to_route_ids,
            "customer_to_individually_global_feasible_route_ids": customer_to_individually_feasible_route_ids,
            "uncovered_customers": uncovered_customers,
            "uncovered_by_individually_global_feasible_routes": uncovered_by_individually_global_feasible_routes,
            "route_ids_by_first_customer": route_ids_by_first_customer,
            "route_ids_by_covered_customer_set": route_ids_by_covered_customer_set,
            "route_ids_by_cover_and_structural_signature": route_ids_by_cover_and_structural_signature,
            "covered_customer_sets_with_structural_variants": {
                covered_set: sorted(variants)
                for covered_set, variants in structural_variants_by_cover.items()
                if len(variants) > 1
            },
            "customer_temp_unreachable_counts": customer_temp_unreachable_counts,
            "customer_perm_unreachable_counts": customer_perm_unreachable_counts,
        }

    def _support_route_no_worse(self, a: Route, b: Route) -> bool:
        a_signature_score = sum(self.route_structural_signature(a))
        b_signature_score = sum(self.route_structural_signature(b))
        return (
            a_signature_score <= b_signature_score
            and a.cost <= b.cost
            and self._route_no_worse_global_resources(a, b)
        )

    def _support_route_strictly_better(self, a: Route, b: Route) -> bool:
        return (
            sum(self.route_structural_signature(a)) < sum(self.route_structural_signature(b))
            or a.cost < b.cost
            or self._vec_lt(list(a.global_resources), list(b.global_resources))
        )

    def _customers_only_in_structurally_weak_routes(
        self,
        routes: Sequence[Route],
    ) -> Set[int]:
        weak_customers: Set[int] = set()
        for customer_id in self.required_customer_ids:
            supporting_routes = [
                route
                for route in routes
                if customer_id in self._required_covered_customers(route)
            ]
            if not supporting_routes:
                continue

            all_weak = True
            for route in supporting_routes:
                dominated = False
                for other in supporting_routes:
                    if other.route_id == route.route_id:
                        continue
                    if self._support_route_no_worse(other, route) and self._support_route_strictly_better(
                        other,
                        route,
                    ):
                        dominated = True
                        break
                if not dominated:
                    all_weak = False
                    break

            if all_weak:
                weak_customers.add(customer_id)

        return weak_customers

    def _build_route_classes_by_covered_set(
        self,
        routes: Sequence[Route],
    ) -> Dict[Tuple[int, ...], List[Dict[str, object]]]:
        grouped: Dict[Tuple[int, ...], Dict[Tuple[int, ...], List[Route]]] = {}
        for route in routes:
            covered_set = tuple(sorted(self._required_covered_customers(route)))
            structural_signature = self.route_structural_signature(route)
            grouped.setdefault(covered_set, {}).setdefault(structural_signature, []).append(
                route
            )

        summaries: Dict[Tuple[int, ...], List[Dict[str, object]]] = {}
        for covered_set, by_signature in grouped.items():
            class_summaries: List[Dict[str, object]] = []
            for structural_signature, class_routes in sorted(by_signature.items()):
                class_summaries.append(
                    {
                        "structural_signature": structural_signature,
                        "route_ids": sorted(route.route_id for route in class_routes),
                        "first_customer_classes": sorted(
                            {
                                route.first_customer_in_route
                                for route in class_routes
                            },
                            key=self._first_customer_sort_key,
                        ),
                        "min_cost": min(route.cost for route in class_routes),
                        "max_cost": max(route.cost for route in class_routes),
                        "min_global_resources": [
                            min(values)
                            for values in zip(
                                *(route.global_resources for route in class_routes),
                                strict=False,
                            )
                        ]
                        if class_routes and class_routes[0].global_resources
                        else [],
                        "max_global_resources": [
                            max(values)
                            for values in zip(
                                *(route.global_resources for route in class_routes),
                                strict=False,
                            )
                        ]
                        if class_routes and class_routes[0].global_resources
                        else [],
                    }
                )
            summaries[covered_set] = class_summaries

        return summaries

    def _build_dominance_reduction_summary(
        self,
        *,
        original_routes: Sequence[Route],
        reduced_routes: Sequence[Route],
        reduction_records: Sequence[RouteReductionRecord],
    ) -> Dict[str, object]:
        removed_by_kept_route: Dict[int, List[int]] = {}
        for record in reduction_records:
            removed_by_kept_route.setdefault(record.kept_route_id, []).append(
                record.removed_route_id
            )

        return {
            "original_route_count": len(original_routes),
            "reduced_route_count": len(reduced_routes),
            "removed_route_count": len(reduction_records),
            "kept_route_ids": [route.route_id for route in reduced_routes],
            "removed_route_ids": sorted(
                record.removed_route_id for record in reduction_records
            ),
            "removed_by_kept_route": {
                kept_route_id: sorted(removed_ids)
                for kept_route_id, removed_ids in removed_by_kept_route.items()
            },
            "reduction_reasons": sorted({record.reason for record in reduction_records}),
        }

    def _build_feedback_summary(
        self,
        *,
        reduced_summary: Dict[str, object],
        customers_with_low_route_support: Set[int],
        customers_only_in_structurally_weak_routes: Set[int],
        bottleneck_customers: Set[int],
    ) -> str:
        pieces: List[str] = []
        reduced_uncovered = sorted(reduced_summary["uncovered_customers"])
        reduced_global_uncovered = sorted(
            reduced_summary["uncovered_by_individually_global_feasible_routes"]
        )

        if reduced_uncovered:
            pieces.append(
                f"Reduced route pool leaves customers uncovered: {reduced_uncovered}."
            )
        elif reduced_global_uncovered:
            pieces.append(
                "Reduced route pool represents every customer, but some customers lack "
                f"individually global-feasible support: {reduced_global_uncovered}."
            )
        else:
            pieces.append(
                "Reduced route pool represents every required customer, so remaining "
                "Phase 2 failures would come from exact-once combination conflicts or "
                "global resource limits."
            )

        if customers_with_low_route_support:
            pieces.append(
                f"Low-support customers in the reduced route pool: {sorted(customers_with_low_route_support)}."
            )
        if customers_only_in_structurally_weak_routes:
            pieces.append(
                "Customers supported only by structurally weak reduced routes: "
                f"{sorted(customers_only_in_structurally_weak_routes)}."
            )
        if bottleneck_customers:
            pieces.append(
                f"Likely bottleneck customers for future Phase 1 refinement: {sorted(bottleneck_customers)}."
            )

        return " ".join(pieces)

    def _build_route_pool_diagnostics(
        self,
        *,
        original_routes: Sequence[Route],
        reduced_routes: Sequence[Route],
        reduction_records: Sequence[RouteReductionRecord],
    ) -> Phase2IPRoutePoolDiagnostics:
        original_summary = self._summarize_route_pool(original_routes)
        reduced_summary = self._summarize_route_pool(reduced_routes)

        reduced_customer_to_route_ids: Dict[int, List[int]] = reduced_summary[
            "customer_to_route_ids"
        ]
        original_customer_perm_counts: Dict[int, int] = original_summary[
            "customer_perm_unreachable_counts"
        ]

        customers_with_low_route_support = {
            customer_id
            for customer_id, route_ids in reduced_customer_to_route_ids.items()
            if len(route_ids) <= 1
        }
        customers_only_in_structurally_weak_routes = (
            self._customers_only_in_structurally_weak_routes(reduced_routes)
        )
        bottleneck_customers = {
            customer_id
            for customer_id in self.required_customer_ids
            if (
                len(reduced_customer_to_route_ids[customer_id]) <= 1
                or original_customer_perm_counts[customer_id] * 2
                >= max(len(original_routes), 1)
            )
        }

        infeasibility_driver: str | None = None
        if reduced_summary["uncovered_customers"]:
            infeasibility_driver = ROUTE_SET_INFEASIBLE
        elif reduced_summary["uncovered_by_individually_global_feasible_routes"]:
            infeasibility_driver = GLOBAL_LIMITS_INFEASIBLE

        return Phase2IPRoutePoolDiagnostics(
            customer_to_route_ids=dict(reduced_summary["customer_to_route_ids"]),
            customer_to_individually_global_feasible_route_ids=dict(
                reduced_summary["customer_to_individually_global_feasible_route_ids"]
            ),
            uncovered_by_generated_routes=set(reduced_summary["uncovered_customers"]),
            uncovered_by_individually_global_feasible_routes=set(
                reduced_summary["uncovered_by_individually_global_feasible_routes"]
            ),
            route_ids_by_first_customer=dict(
                reduced_summary["route_ids_by_first_customer"]
            ),
            route_ids_by_covered_customer_set=dict(
                reduced_summary["route_ids_by_covered_customer_set"]
            ),
            route_ids_by_cover_and_structural_signature=dict(
                reduced_summary["route_ids_by_cover_and_structural_signature"]
            ),
            original_route_pool_customer_to_route_ids=dict(
                original_summary["customer_to_route_ids"]
            ),
            reduced_route_pool_customer_to_route_ids=dict(
                reduced_summary["customer_to_route_ids"]
            ),
            original_route_pool_customer_to_individually_global_feasible_route_ids=dict(
                original_summary["customer_to_individually_global_feasible_route_ids"]
            ),
            reduced_route_pool_customer_to_individually_global_feasible_route_ids=dict(
                reduced_summary["customer_to_individually_global_feasible_route_ids"]
            ),
            original_route_pool_uncovered_customers=set(
                original_summary["uncovered_customers"]
            ),
            reduced_route_pool_uncovered_customers=set(
                reduced_summary["uncovered_customers"]
            ),
            original_route_pool_uncovered_by_individually_global_feasible_routes=set(
                original_summary["uncovered_by_individually_global_feasible_routes"]
            ),
            reduced_route_pool_uncovered_by_individually_global_feasible_routes=set(
                reduced_summary["uncovered_by_individually_global_feasible_routes"]
            ),
            original_route_pool_route_ids_by_first_customer=dict(
                original_summary["route_ids_by_first_customer"]
            ),
            reduced_route_pool_route_ids_by_first_customer=dict(
                reduced_summary["route_ids_by_first_customer"]
            ),
            original_route_pool_route_ids_by_covered_customer_set=dict(
                original_summary["route_ids_by_covered_customer_set"]
            ),
            reduced_route_pool_route_ids_by_covered_customer_set=dict(
                reduced_summary["route_ids_by_covered_customer_set"]
            ),
            original_route_pool_route_ids_by_cover_and_structural_signature=dict(
                original_summary["route_ids_by_cover_and_structural_signature"]
            ),
            reduced_route_pool_route_ids_by_cover_and_structural_signature=dict(
                reduced_summary["route_ids_by_cover_and_structural_signature"]
            ),
            covered_customer_sets_with_structural_variants=dict(
                reduced_summary["covered_customer_sets_with_structural_variants"]
            ),
            customer_temp_unreachable_counts=dict(
                reduced_summary["customer_temp_unreachable_counts"]
            ),
            customer_perm_unreachable_counts=dict(
                reduced_summary["customer_perm_unreachable_counts"]
            ),
            original_route_pool_customer_temp_unreachable_counts=dict(
                original_summary["customer_temp_unreachable_counts"]
            ),
            reduced_route_pool_customer_temp_unreachable_counts=dict(
                reduced_summary["customer_temp_unreachable_counts"]
            ),
            original_route_pool_customer_perm_unreachable_counts=dict(
                original_summary["customer_perm_unreachable_counts"]
            ),
            reduced_route_pool_customer_perm_unreachable_counts=dict(
                reduced_summary["customer_perm_unreachable_counts"]
            ),
            customers_with_low_route_support=customers_with_low_route_support,
            customers_only_in_structurally_weak_routes=customers_only_in_structurally_weak_routes,
            bottleneck_customers=bottleneck_customers,
            route_classes_by_covered_set=self._build_route_classes_by_covered_set(
                reduced_routes
            ),
            reduction_records=list(reduction_records),
            dominance_reduction_summary=self._build_dominance_reduction_summary(
                original_routes=original_routes,
                reduced_routes=reduced_routes,
                reduction_records=reduction_records,
            ),
            kept_route_ids=[route.route_id for route in reduced_routes],
            removed_route_ids=sorted(
                record.removed_route_id for record in reduction_records
            ),
            infeasibility_driver=infeasibility_driver,
            diagnostic_summary=self._build_feedback_summary(
                reduced_summary=reduced_summary,
                customers_with_low_route_support=customers_with_low_route_support,
                customers_only_in_structurally_weak_routes=customers_only_in_structurally_weak_routes,
                bottleneck_customers=bottleneck_customers,
            ),
        )

    def _required_covered_customers(self, route: Route) -> Set[int]:
        return route.covered_customers & self.required_customer_set

    def _solve_ip_model(
        self,
        *,
        prepared: _PreparedRoutePool,
        include_global_limits: bool,
        objective: str,
        resource_dimension: int | None = None,
    ) -> _IPModelResult:
        problem = LpProblem(name="phase2_exact_cover", sense=LpMinimize)
        route_vars = [
            LpVariable(f"x_{idx}", lowBound=0, upBound=1, cat=LpBinary)
            for idx in range(len(prepared.reduced_routes))
        ]

        if objective == "cost":
            problem += lpSum(
                prepared.route_costs[idx] * route_vars[idx]
                for idx in range(len(route_vars))
            )
        elif objective == "resource":
            if resource_dimension is None:
                raise ValueError("resource_dimension is required for resource objectives.")
            problem += lpSum(
                prepared.global_resource_matrix[resource_dimension][idx] * route_vars[idx]
                for idx in range(len(route_vars))
            )
        else:
            raise ValueError(f"Unknown IP objective type: {objective}.")

        for customer_id in self.required_customer_ids:
            support_indices = prepared.customer_to_route_indices[customer_id]
            problem += lpSum(route_vars[idx] for idx in support_indices) == 1, (
                f"cover_customer_{customer_id}"
            )

        if include_global_limits:
            for dim, limit in enumerate(self.global_limits):
                problem += (
                    lpSum(
                        prepared.global_resource_matrix[dim][idx] * route_vars[idx]
                        for idx in range(len(route_vars))
                    )
                    <= limit
                ), f"global_resource_{dim}"

        solver = PULP_CBC_CMD(msg=False, presolve=True, cuts=True)
        problem.solve(solver)
        status_name = LpStatus.get(problem.status, str(problem.status))
        objective_value = (
            float(value(problem.objective)) if status_name == "Optimal" else None
        )
        selected_route_indices = tuple(
            idx
            for idx, variable in enumerate(route_vars)
            if variable.value() is not None and variable.value() > 0.5
        )
        return _IPModelResult(
            status_name=status_name,
            objective_value=objective_value,
            selected_route_indices=selected_route_indices,
        )

    def _minimum_resource_usage_to_cover(
        self,
        prepared: _PreparedRoutePool,
    ) -> Optional[List[float]]:
        if not self.global_limits:
            return []

        minima: List[float] = []
        for dim in range(len(self.global_limits)):
            result = self._solve_ip_model(
                prepared=prepared,
                include_global_limits=False,
                objective="resource",
                resource_dimension=dim,
            )
            self._validate_solver_status(
                result.status_name,
                f"resource-tightening Phase 2 IP dimension {dim}",
            )
            if not result.is_optimal or result.objective_value is None:
                return None
            minima.append(result.objective_value)

        return minima

    def _resource_constraint_violations(
        self,
        minimum_resource_usage_to_cover: Optional[Sequence[float]],
    ) -> Dict[int, float]:
        if minimum_resource_usage_to_cover is None:
            return {}

        violations: Dict[int, float] = {}
        for dim, (minimum_usage, limit) in enumerate(
            zip(minimum_resource_usage_to_cover, self.global_limits)
        ):
            excess = minimum_usage - limit
            if excess > 1e-9:
                violations[dim] = excess

        return violations

    def _build_ip_diagnostics(
        self,
        *,
        prepared: _PreparedRoutePool,
        selected_routes: Sequence[Route],
        solver_status: str,
        minimum_resource_usage_to_cover: Optional[List[float]],
        resource_constraint_violations: Dict[int, float],
        resource_constraint_note: str | None,
        infeasibility_reason: str | None,
    ) -> Phase2IPDiagnostics:
        route_pool_diagnostics = self._build_route_pool_diagnostics(
            original_routes=prepared.original_routes,
            reduced_routes=prepared.reduced_routes,
            reduction_records=prepared.reduction_records,
        )
        selected_route_ids = {route.route_id for route in selected_routes}
        route_usage_summary = {
            route.route_id: int(route.route_id in selected_route_ids)
            for route in prepared.reduced_routes
        }
        selected_customer_coverage_counts = {
            customer_id: sum(
                1
                for route in selected_routes
                if customer_id in self._required_covered_customers(route)
            )
            for customer_id in self.required_customer_ids
        }
        global_resource_usage = (
            [
                sum(route.global_resources[dim] for route in selected_routes)
                for dim in range(len(self.global_limits))
            ]
            if selected_routes
            else None
        )
        global_resource_slacks = (
            [
                limit - usage
                for limit, usage in zip(self.global_limits, global_resource_usage)
            ]
            if global_resource_usage is not None
            else None
        )
        uncovered_customers_from_selection = {
            customer_id
            for customer_id, coverage_count in selected_customer_coverage_counts.items()
            if coverage_count == 0
        }
        customers_with_no_supporting_routes = set(
            route_pool_diagnostics.reduced_route_pool_uncovered_customers
        )

        return Phase2IPDiagnostics(
            route_pool_diagnostics=route_pool_diagnostics,
            reduced_route_pool_used=True,
            original_route_count=len(prepared.original_routes),
            reduced_route_count=len(prepared.reduced_routes),
            customers_with_no_supporting_routes=customers_with_no_supporting_routes,
            uncovered_customers=(
                customers_with_no_supporting_routes
                if infeasibility_reason == ROUTE_SET_INFEASIBLE
                else uncovered_customers_from_selection
                if selected_routes
                else set()
            ),
            route_usage_summary=route_usage_summary,
            selected_customer_coverage_counts=selected_customer_coverage_counts,
            global_resource_usage=global_resource_usage,
            global_resource_slacks=global_resource_slacks,
            minimum_resource_usage_to_cover=minimum_resource_usage_to_cover,
            resource_constraint_violations=resource_constraint_violations,
            resource_constraint_note=resource_constraint_note,
            solver_status=solver_status,
            diagnostic_summary=self._build_ip_diagnostic_summary(
                route_pool_diagnostics=route_pool_diagnostics,
                customers_with_no_supporting_routes=customers_with_no_supporting_routes,
                resource_constraint_violations=resource_constraint_violations,
                resource_constraint_note=resource_constraint_note,
                selected_routes=selected_routes,
                infeasibility_reason=infeasibility_reason,
            ),
        )

    def _build_ip_diagnostic_summary(
        self,
        *,
        route_pool_diagnostics: Phase2IPRoutePoolDiagnostics,
        customers_with_no_supporting_routes: Set[int],
        resource_constraint_violations: Dict[int, float],
        resource_constraint_note: str | None,
        selected_routes: Sequence[Route],
        infeasibility_reason: str | None,
    ) -> str:
        if infeasibility_reason == ROUTE_SET_INFEASIBLE:
            return (
                "Reduced route pool cannot cover all required customers. "
                f"Customers with no supporting routes: {sorted(customers_with_no_supporting_routes)}. "
                f"{route_pool_diagnostics.diagnostic_summary}"
            )

        if infeasibility_reason == GLOBAL_LIMITS_INFEASIBLE:
            if resource_constraint_violations:
                violations = ", ".join(
                    f"dim {dim}: +{excess:.6g}"
                    for dim, excess in sorted(resource_constraint_violations.items())
                )
                return (
                    "Exact cover exists on the reduced route pool, but global resource "
                    f"limits are too tight. Minimum-resource violations: {violations}. "
                    f"{route_pool_diagnostics.diagnostic_summary}"
                )
            if resource_constraint_note is not None:
                return (
                    "Exact cover exists on the reduced route pool, but the global "
                    f"resource limits still make the IP infeasible. {resource_constraint_note} "
                    f"{route_pool_diagnostics.diagnostic_summary}"
                )
            return (
                "Exact cover exists on the reduced route pool, but the global "
                f"resource limits still make the IP infeasible. {route_pool_diagnostics.diagnostic_summary}"
            )

        return (
            "Optimal exact-cover IP solution found on the reduced route pool. "
            f"Selected route ids: {[route.route_id for route in selected_routes]}. "
            f"{route_pool_diagnostics.diagnostic_summary}"
        )

    def _resource_constraint_note(
        self,
        *,
        min_resource_usage: Optional[Sequence[float]],
        resource_constraint_violations: Dict[int, float],
    ) -> str | None:
        if min_resource_usage is None:
            return None
        if resource_constraint_violations:
            return None
        if not min_resource_usage:
            return None
        return (
            "No single global resource dimension alone certifies the infeasibility; "
            "the conflict appears to arise from the joint global limits across the exact cover."
        )

    def _build_infeasible_result(
        self,
        *,
        reason: str,
        diagnostics: Phase2IPDiagnostics | None,
        prepared: _PreparedRoutePool,
        solver_status: str,
    ) -> Phase2IPResult:
        return Phase2IPResult(
            mode=self.mode,
            service_rule=self.service_rule,
            status=INFEASIBLE_STATUS,
            is_feasible=False,
            infeasibility_reason=reason,
            objective_value=None,
            total_cost=None,
            selected_route_ids=[],
            selected_routes=[],
            covered_customers=set(),
            required_customers=set(self.required_customer_ids),
            coverage_complete=False,
            original_route_count=len(prepared.original_routes),
            reduced_route_count=len(prepared.reduced_routes),
            variable_count=len(prepared.reduced_routes),
            coverage_constraint_count=len(self.required_customer_ids),
            global_constraint_count=len(self.global_limits),
            solver_status=solver_status,
            diagnostics=diagnostics,
        )

    @staticmethod
    def _validate_solver_status(status_name: str, model_name: str) -> None:
        if status_name in {"Optimal", "Infeasible"}:
            return
        raise RuntimeError(
            f"{model_name} finished with unsupported solver status {status_name!r}."
        )

    def _route_is_individually_global_feasible(self, route: Route) -> bool:
        if not self.global_limits:
            return True
        return self._within_limits(list(route.global_resources), self.global_limits)

    def _route_order_key(self, route: Route) -> tuple[object, ...]:
        return (
            self._first_customer_sort_key(route.first_customer_in_route),
            tuple(sorted(self._required_covered_customers(route))),
            tuple(sorted(route.covered_customers)),
            self.route_structural_signature(route, required_only=True),
            self._route_signature_key(route.customer_state_signature),
            route.cost,
            tuple(route.global_resources),
            tuple(route.path),
            route.route_id,
        )

    @staticmethod
    def _route_signature_key(signature: Sequence[int]) -> Tuple[int, ...]:
        collapsed: List[int] = []
        for value in signature:
            if value > 0:
                collapsed.append(1)
            else:
                collapsed.append(int(value))
        return tuple(collapsed)

    @staticmethod
    def _first_customer_sort_key(customer_id: int | None) -> tuple[int, float | int]:
        if customer_id is None:
            return (1, inf)
        return (0, customer_id)

    @staticmethod
    def _phase2_customer_state_rank(value: int) -> int:
        if Label.is_visited(value):
            return 0
        if Label.is_reachable(value):
            return 1
        if Label.is_temp_unreachable(value):
            return 2
        if Label.is_perm_unreachable(value):
            return 3
        raise ValueError("Invalid Phase 2 customer state.")

    @staticmethod
    def _within_limits(vec: Sequence[float], limits: Sequence[float]) -> bool:
        return len(vec) == len(limits) and all(v <= lim for v, lim in zip(vec, limits))

    @staticmethod
    def _vec_lt(a: Sequence[float], b: Sequence[float]) -> bool:
        return len(a) == len(b) and any(x < y for x, y in zip(a, b)) and all(
            x <= y for x, y in zip(a, b)
        )

    @staticmethod
    def _covered_customer_set(routes: Sequence[Route]) -> Set[int]:
        covered: Set[int] = set()
        for route in routes:
            covered.update(route.covered_customers)
        return covered
