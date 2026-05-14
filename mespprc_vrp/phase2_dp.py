from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence, Set, Tuple

from .instance import MESPPRCInstance
from .label import Label, PERM_UNREACHABLE, REACHABLE
from .phase1 import Phase1Result, Phase1Solver
from .route import (
    GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING,
    NO_FEASIBLE_NEGATIVE_PAIRING,
    NO_IMPROVING_PAIRING_STATUS,
    NO_NEGATIVE_ROUTES_FROM_PHASE1,
    OPTIMAL_STATUS,
    Route,
    RouteReductionRecord,
    reduce_pricing_route_pool,
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
    Explicit pricing network used by Phase 2.

    The network contains a source node, a sink node, and one intermediate node per
    negative reduced-cost route. Arcs only move forward in route index order to break
    permutation symmetry.
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
    input_route_count: int
    negative_route_count: int
    reduced_route_count: int
    route_network_arc_count: int
    route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]]
    route_ids_by_first_customer: Dict[int | None, List[int]]
    route_ids_by_cover_and_signature: Dict[
        Tuple[Tuple[int, ...], Tuple[int, ...]],
        List[int],
    ]
    incompatible_route_pairs: List[Tuple[int, int]]
    globally_infeasible_single_route_ids: List[int]
    reduction_records: List[RouteReductionRecord]
    kept_route_ids: List[int]
    removed_route_ids: List[int]
    infeasibility_driver: str | None
    diagnostic_summary: str


@dataclass(slots=True)
class Phase2DPResult:
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
    has_negative_pairing: bool
    has_improving_pairing: bool
    best_label: Optional[Label]
    label_buckets: Dict[int, List[Label]]
    route_network: RouteNetwork
    diagnostics: Phase2Diagnostics


@dataclass(slots=True)
class _StateRecord:
    label: Label
    route_sequence: List[int]


@dataclass(slots=True)
class _SearchOutcome:
    best_state: _StateRecord | None
    label_buckets: Dict[int, List[Label]]


class Phase2DPPricingSolver(Phase1Solver):
    """
    Phase 2 pricing solver over the route network.

    This solver no longer enforces full customer coverage. It searches for the minimum
    reduced-cost nonempty subset of pairwise customer-disjoint negative routes that also
    respects the global pairing-level resource limits.
    """

    mode = "route_network_pricing"
    service_rule = "pairwise_customer_disjointness"

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
        self.customer_ids = self.instance.customers()
        self.customer_index = {
            customer_id: idx for idx, customer_id in enumerate(self.customer_ids)
        }
        self.global_limits = list(self.instance.global_limits)
        self._route_network: RouteNetwork | None = None
        self._enforce_global_limits = True

    def solve(self, routes: Sequence[Route | object] | Phase1Result | object) -> Phase2DPResult:
        self.instance.validate()
        self.customer_ids = self.instance.customers()
        self.customer_index = {
            customer_id: idx for idx, customer_id in enumerate(self.customer_ids)
        }
        self.global_limits = list(self.instance.global_limits)
        self._label_limit_warned = False

        original_routes = self._normalize_routes(self._coerce_routes(routes))
        negative_routes = [route for route in original_routes if route.cost < 0.0]
        reduced_routes, reduction_records = reduce_pricing_route_pool(negative_routes)
        route_id_to_route = {route.route_id: route for route in reduced_routes}
        route_network = self.build_route_network(reduced_routes)
        diagnostics = self._build_diagnostics(
            original_routes=original_routes,
            negative_routes=negative_routes,
            reduced_routes=reduced_routes,
            reduction_records=reduction_records,
            route_network=route_network,
        )

        if not reduced_routes:
            return self._build_no_pairing_result(
                reason=NO_NEGATIVE_ROUTES_FROM_PHASE1,
                label_buckets={node_id: [] for node_id in route_network.nodes},
                route_network=route_network,
                diagnostics=diagnostics,
            )

        relaxed_outcome = self._solve_on_route_network(
            route_network=route_network,
            enforce_global_limits=False,
        )
        if relaxed_outcome.best_state is None:
            return self._build_no_pairing_result(
                reason=NO_FEASIBLE_NEGATIVE_PAIRING,
                label_buckets=relaxed_outcome.label_buckets,
                route_network=route_network,
                diagnostics=diagnostics,
            )

        constrained_outcome = self._solve_on_route_network(
            route_network=route_network,
            enforce_global_limits=True,
        )
        if constrained_outcome.best_state is None:
            return self._build_no_pairing_result(
                reason=GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING,
                label_buckets=constrained_outcome.label_buckets,
                route_network=route_network,
                diagnostics=diagnostics,
            )

        best = constrained_outcome.best_state
        selected_routes = [route_id_to_route[route_id] for route_id in best.route_sequence]
        covered_customers = self._covered_customer_set(selected_routes)
        total_cost = best.label.cost

        return Phase2DPResult(
            mode=self.mode,
            service_rule=self.service_rule,
            status=OPTIMAL_STATUS,
            is_feasible=True,
            infeasibility_reason=None,
            total_cost=total_cost,
            best_cost=total_cost,
            selected_route_ids=list(best.route_sequence),
            selected_routes=selected_routes,
            covered_customers=covered_customers,
            has_negative_pairing=total_cost < 0.0,
            has_improving_pairing=total_cost < 0.0,
            best_label=best.label,
            label_buckets=constrained_outcome.label_buckets,
            route_network=route_network,
            diagnostics=diagnostics,
        )

    def build_route_network(self, routes: Sequence[Route]) -> RouteNetwork:
        ordered_routes = sorted(routes, key=lambda route: route.route_id)
        source = 0
        sink = len(ordered_routes) + 1

        nodes: Dict[int, RouteNetworkNode] = {
            source: RouteNetworkNode(node_id=source, kind="source"),
            sink: RouteNetworkNode(node_id=sink, kind="sink"),
        }
        adjacency: Dict[int, List[int]] = {node_id: [] for node_id in range(sink + 1)}
        route_node_to_route: Dict[int, Route] = {}
        arcs: List[RouteNetworkArc] = []

        for idx, route in enumerate(ordered_routes, start=1):
            nodes[idx] = RouteNetworkNode(node_id=idx, kind="route", route_id=route.route_id)
            route_node_to_route[idx] = route
            self._add_route_network_arc(adjacency, arcs, source, idx)
            self._add_route_network_arc(adjacency, arcs, idx, sink)

        for i in range(1, len(ordered_routes) + 1):
            route_i = route_node_to_route[i]
            for j in range(i + 1, len(ordered_routes) + 1):
                route_j = route_node_to_route[j]
                if route_i.covered_customers.isdisjoint(route_j.covered_customers):
                    self._add_route_network_arc(adjacency, arcs, i, j)

        return RouteNetwork(
            source=source,
            sink=sink,
            nodes=nodes,
            adjacency=adjacency,
            route_node_to_route=route_node_to_route,
            arcs=arcs,
        )

    def _add_route_network_arc(
        self,
        adjacency: Dict[int, List[int]],
        arcs: List[RouteNetworkArc],
        tail: int,
        head: int,
    ) -> None:
        adjacency[tail].append(head)
        arcs.append(RouteNetworkArc(tail=tail, head=head))

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
                    tuple(state.route_sequence),
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
        initial_vector = self.refresh_customer_states(
            current_node=source_node,
            global_resources=[0.0] * len(self.global_limits),
            vector=[REACHABLE] * len(self.customer_ids),
        )
        label = Label(
            current_node=source_node,
            cost=0.0,
            resources=[0.0] * len(self.global_limits),
            unreachable_vector=initial_vector,
        )
        return _StateRecord(label=label, route_sequence=[])

    def refresh_customer_states(
        self,
        *,
        current_node: int,
        global_resources: List[float],
        vector: List[int],
    ) -> List[int]:
        out = list(vector)
        for customer_id in self.customer_ids:
            idx = self.customer_index[customer_id]
            state = out[idx]
            if Label.is_visited(state) or Label.is_perm_unreachable(state):
                continue

            if self._has_residual_route_for_customer(
                customer_id=customer_id,
                current_node=current_node,
                global_resources=global_resources,
                vector=out,
            ):
                out[idx] = REACHABLE
            else:
                out[idx] = PERM_UNREACHABLE

        return out

    def _has_residual_route_for_customer(
        self,
        *,
        customer_id: int,
        current_node: int,
        global_resources: List[float],
        vector: Sequence[int],
    ) -> bool:
        route_network = self._route_network_or_error()
        for succ in route_network.successors(current_node):
            if succ == route_network.sink:
                continue

            route = route_network.route_node_to_route[succ]
            if customer_id not in route.covered_customers:
                continue
            if not self._route_is_customer_disjoint(route, vector):
                continue

            if self._enforce_global_limits and self.global_limits:
                candidate_resources = self._vec_add(
                    list(global_resources),
                    list(route.global_resources),
                )
                if not self._within_limits(candidate_resources, self.global_limits):
                    continue

            return True

        return False

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
            if not state.route_sequence:
                return None
            return _StateRecord(
                label=current_label.copy(current_node=succ),
                route_sequence=list(state.route_sequence),
            )

        route = route_network.route_node_to_route[succ]
        if not self._route_is_customer_disjoint(route, current_label.unreachable_vector):
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
        return _StateRecord(
            label=new_label,
            route_sequence=list(state.route_sequence) + [route.route_id],
        )

    def _route_visit_sequence(self, route: Route) -> List[int]:
        ordered: List[int] = []
        seen: Set[int] = set()
        for node_id in route.path:
            if node_id in route.covered_customers and node_id not in seen:
                ordered.append(node_id)
                seen.add(node_id)

        for customer_id in sorted(route.covered_customers):
            if customer_id not in seen:
                ordered.append(customer_id)

        return ordered

    def _route_is_customer_disjoint(
        self,
        route: Route,
        vector: Sequence[int],
    ) -> bool:
        return all(
            not Label.is_visited(vector[self.customer_index[customer_id]])
            for customer_id in route.covered_customers
            if customer_id in self.customer_index
        )

    def _states_equivalent(self, a: _StateRecord, b: _StateRecord) -> bool:
        return a.label == b.label and a.route_sequence == b.route_sequence

    def _state_sort_key(self, state: _StateRecord) -> tuple[object, ...]:
        return (
            state.label.cost,
            state.label.resources,
            self._visited_signature(state.label.unreachable_vector),
            tuple(state.label.unreachable_vector),
            tuple(state.route_sequence),
        )

    def _state_dominates(self, a: _StateRecord, b: _StateRecord) -> bool:
        if a.label.current_node != b.label.current_node:
            return False
        if self._visited_signature(a.label.unreachable_vector) != self._visited_signature(
            b.label.unreachable_vector
        ):
            return False
        if a.label.cost > b.label.cost:
            return False
        if not self._vec_le(a.label.resources, b.label.resources):
            return False
        if not self._pricing_state_no_worse(
            a.label.unreachable_vector,
            b.label.unreachable_vector,
        ):
            return False

        return (
            a.label.cost < b.label.cost
            or self._vec_lt(a.label.resources, b.label.resources)
            or self._pricing_state_strictly_better(
                a.label.unreachable_vector,
                b.label.unreachable_vector,
            )
        )

    def _visited_signature(self, vector: Sequence[int]) -> Tuple[int, ...]:
        return tuple(
            customer_id
            for customer_id in self.customer_ids
            if Label.is_visited(vector[self.customer_index[customer_id]])
        )

    def _pricing_state_no_worse(
        self,
        a: Sequence[int],
        b: Sequence[int],
    ) -> bool:
        for av, bv in zip(a, b):
            if Label.is_visited(av) or Label.is_visited(bv):
                if Label.is_visited(av) != Label.is_visited(bv):
                    return False
                continue
            if self._pricing_state_rank(av) > self._pricing_state_rank(bv):
                return False
        return True

    def _pricing_state_strictly_better(
        self,
        a: Sequence[int],
        b: Sequence[int],
    ) -> bool:
        for av, bv in zip(a, b):
            if Label.is_visited(av) or Label.is_visited(bv):
                continue
            if self._pricing_state_rank(av) < self._pricing_state_rank(bv):
                return True
        return False

    @staticmethod
    def _pricing_state_rank(value: int) -> int:
        if Label.is_reachable(value):
            return 0
        return 1

    def _route_network_or_error(self) -> RouteNetwork:
        if self._route_network is None:
            raise ValueError("Phase 2 route network is not initialized.")
        return self._route_network

    def _coerce_routes(
        self,
        routes: Sequence[Route | object] | Phase1Result | object,
    ) -> List[Route | object]:
        if isinstance(routes, Phase1Result):
            return list(routes.negative_cost_routes)
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

    def _build_diagnostics(
        self,
        *,
        original_routes: Sequence[Route],
        negative_routes: Sequence[Route],
        reduced_routes: Sequence[Route],
        reduction_records: Sequence[RouteReductionRecord],
        route_network: RouteNetwork,
    ) -> Phase2Diagnostics:
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

        incompatible_route_pairs: List[Tuple[int, int]] = []
        ordered_routes = sorted(reduced_routes, key=lambda route: route.route_id)
        for idx, route in enumerate(ordered_routes):
            for other in ordered_routes[idx + 1 :]:
                if not route.covered_customers.isdisjoint(other.covered_customers):
                    incompatible_route_pairs.append((route.route_id, other.route_id))

        globally_infeasible_single_route_ids = sorted(
            route.route_id
            for route in reduced_routes
            if self.global_limits
            and not self._within_limits(list(route.global_resources), self.global_limits)
        )

        if not negative_routes:
            summary = "Phase 1 produced no negative reduced-cost routes for pricing."
        elif not reduced_routes:
            summary = (
                "The negative reduced-cost route pool became empty after safe pricing "
                "route reduction."
            )
        else:
            summary = (
                "Phase 2 pricing built a route network with "
                f"{len(reduced_routes)} reduced negative routes and "
                f"{len(route_network.arcs)} arcs."
            )

        return Phase2Diagnostics(
            input_route_count=len(original_routes),
            negative_route_count=len(negative_routes),
            reduced_route_count=len(reduced_routes),
            route_network_arc_count=len(route_network.arcs),
            route_ids_by_covered_customer_set=dict(route_ids_by_covered_customer_set),
            route_ids_by_first_customer=dict(route_ids_by_first_customer),
            route_ids_by_cover_and_signature=dict(route_ids_by_cover_and_signature),
            incompatible_route_pairs=incompatible_route_pairs,
            globally_infeasible_single_route_ids=globally_infeasible_single_route_ids,
            reduction_records=list(reduction_records),
            kept_route_ids=[route.route_id for route in reduced_routes],
            removed_route_ids=[record.removed_route_id for record in reduction_records],
            infeasibility_driver=None,
            diagnostic_summary=summary,
        )

    def _build_no_pairing_result(
        self,
        *,
        reason: str,
        label_buckets: Dict[int, List[Label]],
        route_network: RouteNetwork,
        diagnostics: Phase2Diagnostics,
    ) -> Phase2DPResult:
        diagnostics.infeasibility_driver = reason
        diagnostics.diagnostic_summary = (
            f"{diagnostics.diagnostic_summary} Pricing result: {reason}."
        )
        return Phase2DPResult(
            mode=self.mode,
            service_rule=self.service_rule,
            status=NO_IMPROVING_PAIRING_STATUS,
            is_feasible=False,
            infeasibility_reason=reason,
            total_cost=None,
            best_cost=None,
            selected_route_ids=[],
            selected_routes=[],
            covered_customers=set(),
            has_negative_pairing=False,
            has_improving_pairing=False,
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


Phase2DPSolver = Phase2DPPricingSolver
