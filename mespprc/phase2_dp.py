from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence, Set, Tuple

from .instance import MESPPRCInstance
from .label import Label, PERM_UNREACHABLE, REACHABLE, TEMP_UNREACHABLE
from .phase1 import Phase1Solver
from .route import (
    GLOBAL_LIMITS_INFEASIBLE,
    INFEASIBLE_STATUS,
    OPTIMAL_STATUS,
    ROUTE_SET_INFEASIBLE,
    Route,
    RouteReductionRecord,
)


@dataclass(frozen=True, slots=True)
class RouteNetworkNode:
    node_id: int
    kind: str
    route_id: int | None = None


@dataclass(frozen=True, slots=True)
class RouteNetworkArc:
    tail: int
    head: int


@dataclass(slots=True)
class RouteNetwork:
    """
    Explicit Phase 2 route network.

    The network contains a source node, a sink node, and one intermediate node per
    generated route. A path through this network corresponds to a canonicalized route
    sequence in the multi-trip solution.
    """

    source: int
    sink: int
    nodes: Dict[int, RouteNetworkNode]
    adjacency: Dict[int, List[int]]
    route_node_to_route: Dict[int, Route]
    arcs: List[RouteNetworkArc]

    def successors(self, node_id: int) -> List[int]:
        return list(self.adjacency.get(node_id, []))


@dataclass(slots=True)
class Phase2Diagnostics:
    """
    Diagnostics intended to support later refinement of route generation.

    Unprefixed coverage and participation fields describe the reduced route pool that
    Phase 2 actually solves over. Matching `original_route_pool_*` and
    `reduced_route_pool_*` fields are provided explicitly so diagnostics can compare the
    generated route pool against the post-reduction decision space.
    """

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


@dataclass
class Phase2DPResult:
    """
    Result of the exact Phase 2 route-network covering solver.
    """

    mode: str
    service_rule: str
    status: str
    is_feasible: bool
    infeasibility_reason: str | None
    total_cost: Optional[float]
    best_cost: Optional[float]
    selected_route_ids: List[int]
    selected_routes: List[Route]
    covered_customers: Set[int]
    required_customers: Set[int]
    coverage_complete: bool
    best_label: Optional[Label]
    label_buckets: Dict[int, List[Label]]
    route_network: RouteNetwork
    diagnostics: Phase2Diagnostics


@dataclass
class _StateRecord:
    label: Label
    route_sequence: List[int]
    sequence_structural_profile: Tuple[int, ...]
    residual_structural_support: Tuple[int, ...]


@dataclass
class _SearchOutcome:
    best_state: _StateRecord | None
    label_buckets: Dict[int, List[Label]]


class Phase2CoveringDPSolver(Phase1Solver):
    """
    Phase 2 exact route-network dynamic program.

    Phase 2 no longer treats generated routes as a flat subset. Instead it builds a
    second network with:
    - one source node,
    - one sink node,
    - one intermediate node per generated route,
    - arcs from source to each route node,
    - arcs from each route node to sink,
    - arcs between pairwise compatible route nodes in a canonical order.

    Labels move through this route network and still carry customer-level state:
    - positive values mean a required customer is already covered,
    - 0 means an uncovered customer is coverable by a currently feasible outgoing route,
    - -2 means the customer is still represented in the residual route network but not
      under the currently active global-feasibility filter,
    - -1 means no residual exact-once-compatible route remains for that customer.
    """

    mode = "route_network_exact_covering"
    service_rule = "exact_once"

    def __init__(
        self,
        instance: MESPPRCInstance,
        *,
        randomize_node_selection: bool = False,
        rng_seed: Optional[int] = None,
        label_limit: Optional[int] = None,
    ) -> None:
        super().__init__(
            instance,
            randomize_node_selection=randomize_node_selection,
            rng_seed=rng_seed,
            label_limit=label_limit,
        )
        self.all_customer_ids: List[int] = self.instance.customers()
        self.all_customer_index: Dict[int, int] = {
            customer_id: idx for idx, customer_id in enumerate(self.all_customer_ids)
        }
        self.required_customer_ids: List[int] = self.instance.required_customers()
        self.required_customer_set: Set[int] = set(self.required_customer_ids)
        self.customer_ids = list(self.required_customer_ids)
        self.customer_index: Dict[int, int] = {
            customer_id: idx for idx, customer_id in enumerate(self.customer_ids)
        }
        self.global_limits: List[float] = list(self.instance.global_limits)
        self._route_network: RouteNetwork | None = None
        self._enforce_global_limits = True

    def solve(self, routes: Sequence[Route | object]) -> Phase2DPResult:
        self.instance.validate()
        if not self.instance.exact_once_service:
            raise NotImplementedError(
                "This Phase 2 solver currently supports exact_once_service=True only."
            )

        self.required_customer_ids = self.instance.required_customers()
        self.required_customer_set = set(self.required_customer_ids)
        self.all_customer_ids = self.instance.customers()
        self.all_customer_index = {
            customer_id: idx for idx, customer_id in enumerate(self.all_customer_ids)
        }
        self.customer_ids = list(self.required_customer_ids)
        self.customer_index = {
            customer_id: idx for idx, customer_id in enumerate(self.customer_ids)
        }
        self.global_limits = list(self.instance.global_limits)
        self._label_limit_warned = False

        normalized_routes = self._normalize_routes(routes)
        reduced_routes, reduction_records = self._reduce_route_pool(normalized_routes)
        route_id_to_route = {route.route_id: route for route in reduced_routes}
        route_network = self.build_route_network(reduced_routes)
        diagnostics = self._build_diagnostics(
            original_routes=normalized_routes,
            reduced_routes=reduced_routes,
            reduction_records=reduction_records,
        )

        if diagnostics.reduced_route_pool_uncovered_customers:
            return self._build_infeasible_result(
                reason=ROUTE_SET_INFEASIBLE,
                label_buckets={node_id: [] for node_id in route_network.nodes},
                route_network=route_network,
                diagnostics=diagnostics,
            )

        relaxed_outcome = self._solve_on_route_network(
            route_network=route_network,
            enforce_global_limits=False,
        )
        if relaxed_outcome.best_state is None:
            return self._build_infeasible_result(
                reason=ROUTE_SET_INFEASIBLE,
                label_buckets=relaxed_outcome.label_buckets,
                route_network=route_network,
                diagnostics=diagnostics,
            )

        constrained_outcome = self._solve_on_route_network(
            route_network=route_network,
            enforce_global_limits=True,
        )
        if constrained_outcome.best_state is None:
            return self._build_infeasible_result(
                reason=GLOBAL_LIMITS_INFEASIBLE,
                label_buckets=constrained_outcome.label_buckets,
                route_network=route_network,
                diagnostics=diagnostics,
            )

        best = constrained_outcome.best_state
        selected_routes = [route_id_to_route[route_id] for route_id in best.route_sequence]
        required_customers = set(self.required_customer_ids)
        covered_customers = self._covered_customer_set(selected_routes)

        return Phase2DPResult(
            mode=self.mode,
            service_rule=self.service_rule,
            status=OPTIMAL_STATUS,
            is_feasible=True,
            infeasibility_reason=None,
            total_cost=best.label.cost,
            best_cost=best.label.cost,
            selected_route_ids=list(best.route_sequence),
            selected_routes=selected_routes,
            covered_customers=covered_customers,
            required_customers=required_customers,
            coverage_complete=required_customers.issubset(covered_customers),
            best_label=best.label,
            label_buckets=constrained_outcome.label_buckets,
            route_network=route_network,
            diagnostics=diagnostics,
        )

    def build_route_network(self, routes: Sequence[Route]) -> RouteNetwork:
        """
        Build the explicit Phase 2 route network.

        Routes are ordered canonically using first-customer metadata and route structure.
        This breaks route-sequence symmetry without changing feasibility.
        """

        ordered_routes = sorted(routes, key=self._route_order_key)
        source = 0
        sink = len(ordered_routes) + 1

        nodes: Dict[int, RouteNetworkNode] = {
            source: RouteNetworkNode(node_id=source, kind="source"),
            sink: RouteNetworkNode(node_id=sink, kind="sink"),
        }
        adjacency: Dict[int, List[int]] = {node_id: [] for node_id in range(sink + 1)}
        route_node_to_route: Dict[int, Route] = {}
        arcs: List[RouteNetworkArc] = []

        self._add_route_network_arc(adjacency, arcs, source, sink)
        for idx, route in enumerate(ordered_routes, start=1):
            nodes[idx] = RouteNetworkNode(
                node_id=idx,
                kind="route",
                route_id=route.route_id,
            )
            route_node_to_route[idx] = route
            self._add_route_network_arc(adjacency, arcs, source, idx)
            self._add_route_network_arc(adjacency, arcs, idx, sink)

        for i in range(1, len(ordered_routes) + 1):
            route_i = route_node_to_route[i]
            for j in range(i + 1, len(ordered_routes) + 1):
                route_j = route_node_to_route[j]
                if self._routes_pairwise_compatible(route_i, route_j):
                    self._add_route_network_arc(adjacency, arcs, i, j)

        return RouteNetwork(
            source=source,
            sink=sink,
            nodes=nodes,
            adjacency=adjacency,
            route_node_to_route=route_node_to_route,
            arcs=arcs,
        )

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
        """
        Safely remove structurally dominated routes before building the route network.

        A route can eliminate another only when they cover the same required customers,
        share the same first-customer class, have no worse cost and global resources,
        and the eliminator carries a no-weaker Phase 1 structural signature.
        """

        reductions: List[RouteReductionRecord] = []
        reduced_routes: List[Route] = []

        grouped: Dict[Tuple[Tuple[int, ...], int | None], List[Route]] = {}
        for route in routes:
            key = (
                tuple(sorted(self._required_covered_customers(route))),
                route.first_customer_in_route,
            )
            grouped.setdefault(key, []).append(route)

        for _, group_routes in grouped.items():
            survivors: List[Route] = []
            ordered_group = sorted(group_routes, key=self._route_order_key)
            for route in ordered_group:
                dominated = False
                kept_survivors: List[Route] = []
                for survivor in survivors:
                    if self._route_structurally_dominates(survivor, route):
                        reductions.append(
                            RouteReductionRecord(
                                kept_route_id=survivor.route_id,
                                removed_route_id=route.route_id,
                                reason=(
                                    "same required covered set, no worse cost/global "
                                    "resources, and stronger Phase 1 structural signature"
                                ),
                            )
                        )
                        dominated = True
                        break

                    if self._route_structurally_dominates(route, survivor):
                        reductions.append(
                            RouteReductionRecord(
                                kept_route_id=route.route_id,
                                removed_route_id=survivor.route_id,
                                reason=(
                                    "same required covered set, no worse cost/global "
                                    "resources, and stronger Phase 1 structural signature"
                                ),
                            )
                        )
                        continue

                    kept_survivors.append(survivor)

                if not dominated:
                    kept_survivors.append(route)
                    survivors = kept_survivors

            reduced_routes.extend(survivors)

        return sorted(reduced_routes, key=self._route_order_key), reductions

    def _route_structurally_dominates(self, a: Route, b: Route) -> bool:
        if self._required_covered_customers(a) != self._required_covered_customers(b):
            return False
        if a.first_customer_in_route != b.first_customer_in_route:
            return False
        if not self._route_no_worse_global_resources(a, b):
            return False
        if a.cost > b.cost:
            return False
        if not (
            self.route_has_full_customer_signature(a)
            and self.route_has_full_customer_signature(b)
        ):
            return False

        signature_a = self.route_structural_signature(a)
        signature_b = self.route_structural_signature(b)
        if not self._structural_signature_no_worse(signature_a, signature_b):
            return False

        return (
            a.cost < b.cost
            or self._vec_lt(list(a.global_resources), list(b.global_resources))
            or self._structural_signature_strictly_better(signature_a, signature_b)
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

    def _summarize_route_pool(
        self,
        routes: Sequence[Route],
    ) -> Dict[str, object]:
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

    def _support_route_quality_key(self, route: Route) -> tuple[object, ...]:
        return (
            sum(self.route_structural_signature(route)),
            route.cost,
            tuple(route.global_resources),
        )

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

    def _solve_on_route_network(
        self,
        *,
        route_network: RouteNetwork,
        enforce_global_limits: bool,
    ) -> _SearchOutcome:
        self._route_network = route_network
        self._enforce_global_limits = enforce_global_limits
        buckets: Dict[int, List[_StateRecord]] = {
            node_id: [] for node_id in route_network.nodes
        }
        buckets[route_network.source].append(self._initial_state(route_network.source))
        self._run_labeling_search(
            active_nodes=[route_network.source],
            buckets=buckets,
        )

        sink_states = buckets[route_network.sink]
        best_state = (
            min(
                sink_states,
                key=lambda state: (
                    state.label.cost,
                    state.label.resources,
                    state.route_sequence,
                ),
            )
            if sink_states
            else None
        )
        return _SearchOutcome(
            best_state=best_state,
            label_buckets={
                node_id: [state.label for state in states]
                for node_id, states in buckets.items()
            },
        )

    def _initial_state(self, source_node: int) -> _StateRecord:
        initial_resources = [0.0] * len(self.global_limits)
        initial_vector = self.refresh_customer_states(
            current_node=source_node,
            global_resources=initial_resources,
            vector=[REACHABLE] * len(self.required_customer_ids),
        )
        sequence_structural_profile = tuple(
            self._phase2_customer_state_rank(PERM_UNREACHABLE)
            for _ in self.required_customer_ids
        )
        residual_structural_support = self._compute_residual_structural_support(
            current_node=source_node,
            global_resources=initial_resources,
            vector=initial_vector,
        )
        label = Label(
            current_node=source_node,
            cost=0.0,
            resources=initial_resources,
            unreachable_vector=initial_vector,
        )
        return _StateRecord(
            label=label,
            route_sequence=[],
            sequence_structural_profile=sequence_structural_profile,
            residual_structural_support=residual_structural_support,
        )

    def refresh_customer_states(
        self,
        *,
        current_node: int,
        global_resources: List[float],
        vector: List[int],
    ) -> List[int]:
        out = list(vector)
        changed = True
        while changed:
            changed = False
            for customer_id in self.required_customer_ids:
                idx = self.customer_index[customer_id]
                state = out[idx]
                if Label.is_visited(state) or Label.is_perm_unreachable(state):
                    continue

                if self.proves_permanent_unreachability(
                    customer_id=customer_id,
                    current_node=current_node,
                    global_resources=global_resources,
                    vector=out,
                ):
                    out[idx] = PERM_UNREACHABLE
                    changed = True

        for customer_id in self.required_customer_ids:
            idx = self.customer_index[customer_id]
            state = out[idx]
            if Label.is_visited(state) or Label.is_perm_unreachable(state):
                continue

            out[idx] = self.classify_customer_state(
                customer_id=customer_id,
                current_node=current_node,
                global_resources=global_resources,
                vector=out,
            )

        return out

    def classify_customer_state(
        self,
        *,
        customer_id: int,
        current_node: int,
        global_resources: List[float],
        vector: List[int],
    ) -> int:
        if self.can_currently_reach_customer(
            customer_id=customer_id,
            current_node=current_node,
            global_resources=global_resources,
            vector=vector,
        ):
            return REACHABLE

        if self.can_still_reach_customer(
            customer_id=customer_id,
            current_node=current_node,
            global_resources=global_resources,
            vector=vector,
        ):
            return TEMP_UNREACHABLE

        return TEMP_UNREACHABLE

    def can_currently_reach_customer(
        self,
        *,
        customer_id: int,
        current_node: int,
        global_resources: List[float],
        vector: List[int],
    ) -> bool:
        return bool(
            self._residual_covering_route_nodes(
                customer_id=customer_id,
                current_node=current_node,
                global_resources=global_resources,
                vector=vector,
                require_globally_feasible=True,
            )
        )

    def can_still_reach_customer(
        self,
        *,
        customer_id: int,
        current_node: int,
        global_resources: List[float],
        vector: List[int],
    ) -> bool:
        return bool(
            self._residual_covering_route_nodes(
                customer_id=customer_id,
                current_node=current_node,
                global_resources=global_resources,
                vector=vector,
                require_globally_feasible=False,
            )
        )

    def proves_permanent_unreachability(
        self,
        *,
        customer_id: int,
        current_node: int,
        global_resources: List[float],
        vector: List[int],
        ) -> bool:
        return not self.can_still_reach_customer(
            customer_id=customer_id,
            current_node=current_node,
            global_resources=global_resources,
            vector=vector,
        )

    def _merge_sequence_structural_profile(
        self,
        current_profile: Tuple[int, ...],
        route: Route,
    ) -> Tuple[int, ...]:
        route_profile = self.route_structural_signature(route)
        if not current_profile:
            return route_profile
        return tuple(
            min(current_value, route_value)
            for current_value, route_value in zip(current_profile, route_profile)
        )

    def _compute_residual_structural_support(
        self,
        *,
        current_node: int,
        global_resources: List[float],
        vector: Sequence[int],
    ) -> Tuple[int, ...]:
        support: List[int] = []
        for customer_id in self.required_customer_ids:
            idx = self.customer_index[customer_id]
            if Label.is_visited(vector[idx]):
                support.append(0)
                continue

            support.append(
                self._best_residual_support_rank_for_customer(
                    customer_id=customer_id,
                    current_node=current_node,
                    global_resources=global_resources,
                    vector=vector,
                )
            )

        return tuple(support)

    def _best_residual_support_rank_for_customer(
        self,
        *,
        customer_id: int,
        current_node: int,
        global_resources: List[float],
        vector: Sequence[int],
    ) -> int:
        route_network = self._route_network_or_error()
        best_rank = self._phase2_customer_state_rank(PERM_UNREACHABLE)

        for succ in route_network.successors(current_node):
            if succ == route_network.sink:
                continue

            route = route_network.route_node_to_route[succ]
            if not self._route_is_exact_once_compatible(route, vector):
                continue
            if self._enforce_global_limits and self.global_limits:
                candidate_resources = self._vec_add(
                    list(global_resources),
                    list(route.global_resources),
                )
                if not self._within_limits(candidate_resources, self.global_limits):
                    continue

            if customer_id in self._required_covered_customers(route):
                candidate_rank = 0
            else:
                candidate_rank = self.route_structural_signature(route)[
                    self.customer_index[customer_id]
                ]

            if candidate_rank < best_rank:
                best_rank = candidate_rank

        return best_rank

    def _residual_covering_route_nodes(
        self,
        *,
        customer_id: int,
        current_node: int,
        global_resources: List[float],
        vector: List[int],
        require_globally_feasible: bool,
    ) -> List[int]:
        route_network = self._route_network_or_error()
        candidates: List[int] = []
        for succ in route_network.successors(current_node):
            if succ == route_network.sink:
                continue

            route = route_network.route_node_to_route[succ]
            if customer_id not in self._required_covered_customers(route):
                continue
            if not self._route_is_exact_once_compatible(route, vector):
                continue
            if require_globally_feasible and self._enforce_global_limits:
                candidate_resources = self._vec_add(
                    list(global_resources),
                    list(route.global_resources),
                )
                if self.global_limits and not self._within_limits(
                    candidate_resources,
                    self.global_limits,
                ):
                    continue

            candidates.append(succ)

        return candidates

    def _extend_from_node(
        self,
        node: int,
        buckets: Dict[int, List[_StateRecord]],
    ) -> List[int]:
        route_network = self._route_network_or_error()
        if node == route_network.sink:
            return []

        changed_nodes: List[int] = []
        for state in sorted(buckets[node], key=self._state_sort_key):
            for succ in route_network.successors(node):
                new_state = self._try_extend_state(state=state, succ=succ)
                if new_state is None:
                    continue

                changed = self._insert_with_dominance(new_state, buckets[succ])
                if changed and succ not in changed_nodes:
                    changed_nodes.append(succ)

        return changed_nodes

    def _try_extend_state(
        self,
        *,
        state: _StateRecord,
        succ: int,
    ) -> Optional[_StateRecord]:
        route_network = self._route_network_or_error()
        current_label = state.label

        if succ == route_network.sink:
            if not self._covers_all_required_customers(current_label.unreachable_vector):
                return None
            return _StateRecord(
                label=current_label.copy(current_node=succ),
                route_sequence=list(state.route_sequence),
                sequence_structural_profile=state.sequence_structural_profile,
                residual_structural_support=state.residual_structural_support,
            )

        route = route_network.route_node_to_route[succ]
        if not self._route_is_exact_once_compatible(
            route,
            current_label.unreachable_vector,
        ):
            return None

        new_resources = self._vec_add(
            list(current_label.resources),
            list(route.global_resources),
        )
        if (
            self._enforce_global_limits
            and self.global_limits
            and not self._within_limits(new_resources, self.global_limits)
        ):
            return None

        new_vector = list(current_label.unreachable_vector)
        next_visit_pos = self._next_visit_position(new_vector)
        for customer_id in self._route_visit_sequence(route):
            if customer_id not in self.customer_index:
                continue
            idx = self.customer_index[customer_id]
            if Label.is_visited(new_vector[idx]):
                return None
            new_vector[idx] = next_visit_pos
            next_visit_pos += 1

        new_vector = self.refresh_customer_states(
            current_node=succ,
            global_resources=new_resources,
            vector=new_vector,
        )
        new_label = Label(
            current_node=succ,
            cost=current_label.cost + route.cost,
            resources=new_resources,
            unreachable_vector=new_vector,
        )
        new_sequence_structural_profile = self._merge_sequence_structural_profile(
            state.sequence_structural_profile,
            route,
        )
        new_residual_structural_support = self._compute_residual_structural_support(
            current_node=succ,
            global_resources=new_resources,
            vector=new_vector,
        )
        return _StateRecord(
            label=new_label,
            route_sequence=list(state.route_sequence) + [route.route_id],
            sequence_structural_profile=new_sequence_structural_profile,
            residual_structural_support=new_residual_structural_support,
        )

    def _route_visit_sequence(self, route: Route) -> List[int]:
        required_customers = self._required_covered_customers(route)
        ordered: List[int] = []
        seen: Set[int] = set()
        for node_id in route.path:
            if node_id in required_customers and node_id not in seen:
                ordered.append(node_id)
                seen.add(node_id)

        for customer_id in sorted(required_customers):
            if customer_id not in seen:
                ordered.append(customer_id)

        return ordered

    def _required_covered_customers(self, route: Route) -> Set[int]:
        return route.covered_customers & self.required_customer_set

    def _route_is_exact_once_compatible(
        self,
        route: Route,
        vector: Sequence[int],
    ) -> bool:
        return all(
            not Label.is_visited(vector[self.customer_index[customer_id]])
            for customer_id in self._required_covered_customers(route)
        )

    def _covers_all_required_customers(self, vector: Sequence[int]) -> bool:
        return all(
            Label.is_visited(vector[self.customer_index[customer_id]])
            for customer_id in self.required_customer_ids
        )

    def _states_equivalent(self, a: _StateRecord, b: _StateRecord) -> bool:
        return (
            a.label == b.label
            and a.route_sequence == b.route_sequence
            and a.sequence_structural_profile == b.sequence_structural_profile
            and a.residual_structural_support == b.residual_structural_support
        )

    def _state_sort_key(self, state: _StateRecord) -> tuple[object, ...]:
        return (
            state.label.cost,
            state.label.resources,
            self._phase2_customer_state_signature(state.label.unreachable_vector),
            state.sequence_structural_profile,
            state.residual_structural_support,
            state.route_sequence,
        )

    def _state_dominates(self, a: _StateRecord, b: _StateRecord) -> bool:
        base_label_relation = self._dominates(a.label, b.label)
        labels_equal = a.label == b.label
        if not (base_label_relation or labels_equal):
            return False

        if a.label.unreachable_vector == b.label.unreachable_vector:
            if not self._structural_signature_no_worse(
                a.sequence_structural_profile,
                b.sequence_structural_profile,
            ):
                return False
            if not self._structural_signature_no_worse(
                a.residual_structural_support,
                b.residual_structural_support,
            ):
                return False

            if (
                self._structural_signature_strictly_better(
                    a.sequence_structural_profile,
                    b.sequence_structural_profile,
                )
                or self._structural_signature_strictly_better(
                    a.residual_structural_support,
                    b.residual_structural_support,
                )
            ):
                return True

        return labels_equal and tuple(a.route_sequence) < tuple(b.route_sequence)

    def _dominates(self, a: Label, b: Label) -> bool:
        if a.current_node != b.current_node:
            return False
        if not self._vec_le(a.resources, b.resources):
            return False
        if a.cost > b.cost:
            return False
        if not self._phase2_customer_state_no_worse(
            a.unreachable_vector,
            b.unreachable_vector,
        ):
            return False

        return (
            a.cost < b.cost
            or self._vec_lt(a.resources, b.resources)
            or self._phase2_customer_state_strictly_better(
                a.unreachable_vector,
                b.unreachable_vector,
            )
        )

    def _phase2_customer_state_no_worse(
        self,
        a: Sequence[int],
        b: Sequence[int],
    ) -> bool:
        return all(
            self._phase2_customer_state_rank(av) <= self._phase2_customer_state_rank(bv)
            for av, bv in zip(a, b)
        )

    def _phase2_customer_state_strictly_better(
        self,
        a: Sequence[int],
        b: Sequence[int],
    ) -> bool:
        return any(
            self._phase2_customer_state_rank(av) < self._phase2_customer_state_rank(bv)
            for av, bv in zip(a, b)
        )

    def _phase2_customer_state_signature(self, vector: Sequence[int]) -> Tuple[int, ...]:
        return tuple(self._phase2_customer_state_rank(value) for value in vector)

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

    def _routes_pairwise_compatible(self, a: Route, b: Route) -> bool:
        return self._required_covered_customers(a).isdisjoint(
            self._required_covered_customers(b)
        )

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

    def _build_diagnostics(
        self,
        *,
        original_routes: Sequence[Route],
        reduced_routes: Sequence[Route],
        reduction_records: Sequence[RouteReductionRecord],
    ) -> Phase2Diagnostics:
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

        return Phase2Diagnostics(
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

    def _route_is_individually_global_feasible(self, route: Route) -> bool:
        if not self.global_limits:
            return True
        return self._within_limits(list(route.global_resources), self.global_limits)

    def _route_network_or_error(self) -> RouteNetwork:
        if self._route_network is None:
            raise ValueError("Phase 2 route network has not been initialized.")
        return self._route_network

    @staticmethod
    def _add_route_network_arc(
        adjacency: Dict[int, List[int]],
        arcs: List[RouteNetworkArc],
        tail: int,
        head: int,
    ) -> None:
        adjacency[tail].append(head)
        arcs.append(RouteNetworkArc(tail=tail, head=head))

    def _build_infeasible_result(
        self,
        *,
        reason: str,
        label_buckets: Dict[int, List[Label]],
        route_network: RouteNetwork,
        diagnostics: Phase2Diagnostics,
    ) -> Phase2DPResult:
        if diagnostics.infeasibility_driver is None:
            diagnostics.infeasibility_driver = reason
        if reason not in diagnostics.diagnostic_summary:
            diagnostics.diagnostic_summary = (
                f"{diagnostics.diagnostic_summary} Observed Phase 2 failure mode: {reason}."
            ).strip()

        return Phase2DPResult(
            mode=self.mode,
            service_rule=self.service_rule,
            status=INFEASIBLE_STATUS,
            is_feasible=False,
            infeasibility_reason=reason,
            total_cost=None,
            best_cost=None,
            selected_route_ids=[],
            selected_routes=[],
            covered_customers=set(),
            required_customers=set(self.required_customer_ids),
            coverage_complete=False,
            best_label=None,
            label_buckets=label_buckets,
            route_network=route_network,
            diagnostics=diagnostics,
        )

    @staticmethod
    def _covered_customer_set(routes: Sequence[Route]) -> Set[int]:
        covered: Set[int] = set()
        for route in routes:
            covered.update(route.covered_customers)
        return covered


class Phase2DPSolver(Phase2CoveringDPSolver):
    """
    Backward-compatible alias for the exact Phase 2 route-network covering solver.
    """
