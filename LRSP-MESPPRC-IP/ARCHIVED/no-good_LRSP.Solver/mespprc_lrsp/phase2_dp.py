from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

from .instance import MESPPRCInstance
from .label import Label, PERM_UNREACHABLE, REACHABLE, TEMP_UNREACHABLE
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


@dataclass(slots=True, frozen=True)
class Phase2PairingNetworkNode:
    """
    Node in the specialized Phase 2 pairing network.

    The network is the LRSP second network:
    - one artificial source node
    - one artificial sink node
    - one intermediate node per negative Phase 1 route

    A path through this network encodes a canonicalized vehicle pairing, where each
    visited route node contributes one route to the pairing.
    """

    node_id: int
    node_kind: str
    route_id: int | None = None


@dataclass(slots=True, frozen=True)
class Phase2PairingNetworkArc:
    """
    Arc in the Phase 2 pairing network.

    Arc kinds:
    - ``source_to_route``: start a pairing with a negative route
    - ``route_to_route``: continue a partial pairing with another negative route
    - ``route_to_sink``: terminate the current pairing
    """

    tail: int
    head: int
    arc_kind: str


@dataclass(slots=True)
class Phase2PairingNetwork:
    """
    Specialized second network used by the LRSP pairing DP.

    ``respect_global_limits`` distinguishes:
    - the structural pairing network, which captures customer-disjoint canonical
      continuation relationships between route nodes
    - the feasible pairing network, which additionally screens arcs through a
      conservative global-resource continuation test
    """

    nodes: Dict[int, Phase2PairingNetworkNode]
    adjacency: Dict[int, List[int]]
    outgoing_arcs: Dict[int, List[Phase2PairingNetworkArc]]
    source_node: int
    sink_node: int
    route_node_to_route: Dict[int, Route]
    respect_global_limits: bool

    @property
    def arc_count(self) -> int:
        return sum(len(arcs) for arcs in self.outgoing_arcs.values())


@dataclass(slots=True)
class Phase2PairingDiagnostics:
    """
    Diagnostics for the specialized LRSP second-network pairing DP.

    The diagnostics focus on the second-network structure itself: how many negative
    Phase 1 routes survive reduction, how richly connected the pairing network is,
    which route nodes can start a pairing, which ones are isolated, and whether any
    failure is best explained by a lack of negative routes, no feasible start arcs,
    or no feasible continuation under the global pairing limits.
    """

    input_route_count: int
    negative_route_count: int
    reduced_route_count: int
    pairing_network_arc_count: int
    source_to_route_arc_count: int
    route_to_route_arc_count: int
    route_to_sink_arc_count: int
    structural_pairing_network_arc_count: int
    structural_source_to_route_arc_count: int
    structural_route_to_route_arc_count: int
    structural_route_to_sink_arc_count: int
    outgoing_continuation_counts_by_route_id: Dict[int, int]
    structural_continuation_counts_by_route_id: Dict[int, int]
    startable_route_ids: List[int]
    structurally_startable_route_ids: List[int]
    no_feasible_start_route_ids: List[int]
    isolated_negative_route_ids: List[int]
    no_feasible_continuation_route_ids: List[int]
    route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]]
    route_ids_by_first_customer: Dict[int | None, List[int]]
    route_ids_by_cover_and_signature: Dict[Tuple[Tuple[int, ...], Tuple[int, ...]], List[int]]
    incompatible_route_pairs: List[Tuple[int, int]]
    globally_infeasible_single_route_ids: List[int]
    reduction_records: List[RouteReductionRecord]
    kept_route_ids: List[int]
    removed_route_ids: List[int]
    network_failure_mode: str | None
    infeasibility_driver: str | None
    diagnostic_summary: str


@dataclass(slots=True)
class Phase2DPResult:
    """
    Output of the specialized LRSP pairing-network dynamic program.

    A feasible result corresponds to a nonempty path from the pairing-network source
    to the pairing-network sink. The selected route sequence is the path through the
    second network and therefore the canonicalized route order for one vehicle
    pairing. The solver seeks the minimum reduced-cost feasible pairing, not any
    full-demand covering structure.
    """

    mode: str
    service_rule: str
    status: str
    is_feasible: bool
    has_negative_pairing: bool
    infeasibility_reason: str | None
    total_cost: float | None
    best_cost: float | None
    selected_route_ids: List[int]
    selected_routes: List[Route]
    covered_customers: Set[int]
    best_label: Label | None
    label_buckets: Dict[int, List[Label]]
    diagnostics: Phase2PairingDiagnostics | None = None
    pairing_network: Phase2PairingNetwork | None = None

    @property
    def has_improving_pairing(self) -> bool:
        return self.has_negative_pairing

    @property
    def best_pairing(self) -> List[Route]:
        return list(self.selected_routes)

    @property
    def route_network(self) -> Phase2PairingNetwork | None:
        return self.pairing_network


@dataclass(slots=True)
class _ResidualPairingSupport:
    """
    Residual continuation summary for a partial pairing.

    ``structural_*`` fields are based on the canonical second-network topology only.
    ``feasible_*`` fields additionally enforce the current pairing resource state, so
    they represent routes that can legally continue the partial pairing right now.
    """

    structural_continuation_nodes: Tuple[int, ...]
    feasible_continuation_nodes: Tuple[int, ...]
    structural_customer_support_profile: Tuple[int, ...]
    feasible_customer_support_profile: Tuple[int, ...]


@dataclass(slots=True)
class _PairingStateRecord:
    """
    Internal Phase 2 DP state for one partial LRSP pairing.

    The state represents a partial path in the specialized second network:
    - ``label.current_node`` identifies the current pairing-network node
    - ``pairing_route_sequence`` is the route-id path selected so far
    - ``pairing_node_path`` is the explicit path through the second network
    - ``label.resources`` stores the pairing-level global resource usage
    - ``label.unreachable_vector`` summarizes customer service and residual support
    - residual continuation sets/profiles describe how strong the remaining pairing
      network is from the current state
    """

    label: Label
    pairing_route_sequence: List[int]
    pairing_node_path: List[int]
    structural_continuation_nodes: Tuple[int, ...]
    feasible_continuation_nodes: Tuple[int, ...]
    structural_continuation_node_set: frozenset[int]
    feasible_continuation_node_set: frozenset[int]
    structural_customer_support_profile: Tuple[int, ...]
    feasible_customer_support_profile: Tuple[int, ...]


class Phase2DPPricingSolver(Phase1Solver):
    """
    Specialized LRSP second-network dynamic program for pairing generation.

    The input to Phase 2 is the negative reduced-cost route pool produced by Phase 1.
    Each intermediate node in the second network corresponds to one negative route.
    A source-to-sink path therefore represents a feasible vehicle pairing:
    a canonicalized sequence of customer-disjoint routes whose cumulative pairing
    resource usage respects the vehicle-level global limits.

    This solver is deliberately not a covering master problem. It does not require
    serving all customers, and it does not search for an exact cover. It solves a
    shortest-path-style dynamic program over the specialized second network to find
    the minimum reduced-cost feasible nonempty pairing.
    """

    mode = "lrsp_pairing_network_dp"
    service_rule = "single_facility_pairing"

    def __init__(
        self,
        instance: MESPPRCInstance,
        *,
        collect_diagnostics: bool = True,
        apply_route_reduction: bool = True,
    ) -> None:
        super().__init__(instance)
        self.collect_diagnostics = collect_diagnostics
        self.apply_route_reduction = apply_route_reduction

        self.global_limits = list(instance.global_limits)
        self.customer_ids = instance.customers()
        self.customer_index = {
            customer_id: idx for idx, customer_id in enumerate(self.customer_ids)
        }

        self._pairing_network: Phase2PairingNetwork | None = None
        self._structural_pairing_network: Phase2PairingNetwork | None = None
        self._pairing_sink: int | None = None
        self._pairing_source: int | None = None
        self._route_node_to_route: Dict[int, Route] = {}
        self._pairing_node_id_by_route_id: Dict[int, int] = {}
        self._structural_adjacency: Dict[int, Tuple[int, ...]] = {}

    def solve(
        self,
        routes_or_phase1: Phase1Result | Sequence[Route],
    ) -> Phase2DPResult:
        input_routes = self._coerce_routes(routes_or_phase1)
        negative_routes = [route for route in input_routes if route.cost < 0]
        reduced_routes, reduction_records = self._reduce_route_pool(negative_routes)

        if not reduced_routes:
            diagnostics = self._build_diagnostics(
                input_route_count=len(input_routes),
                negative_routes=negative_routes,
                reduced_routes=[],
                reduction_records=reduction_records,
                pairing_network=None,
                structural_pairing_network=None,
            )
            return Phase2DPResult(
                mode=self.mode,
                service_rule=self.service_rule,
                status=NO_IMPROVING_PAIRING_STATUS,
                is_feasible=False,
                has_negative_pairing=False,
                infeasibility_reason=(
                    diagnostics.infeasibility_driver if diagnostics else NO_NEGATIVE_ROUTES_FROM_PHASE1
                ),
                total_cost=None,
                best_cost=None,
                selected_route_ids=[],
                selected_routes=[],
                covered_customers=set(),
                best_label=None,
                label_buckets={},
                diagnostics=diagnostics,
                pairing_network=None,
            )

        structural_pairing_network = self._build_pairing_network(
            reduced_routes,
            respect_global_limits=False,
        )
        pairing_network = self._build_pairing_network(
            reduced_routes,
            respect_global_limits=True,
        )
        self._initialize_pairing_network_state(
            pairing_network=pairing_network,
            structural_pairing_network=structural_pairing_network,
        )

        source_state = self._initial_pairing_state()
        raw_buckets = self._run_labeling_search(source_state)
        label_buckets = {
            node_id: [state.label for state in states]
            for node_id, states in raw_buckets.items()
        }

        sink = self._pairing_sink
        if sink is None:
            raise ValueError("Pairing-network sink was not initialized.")

        sink_states = raw_buckets.get(sink, [])
        diagnostics = self._build_diagnostics(
            input_route_count=len(input_routes),
            negative_routes=negative_routes,
            reduced_routes=reduced_routes,
            reduction_records=reduction_records,
            pairing_network=pairing_network,
            structural_pairing_network=structural_pairing_network,
        )

        if not sink_states:
            return Phase2DPResult(
                mode=self.mode,
                service_rule=self.service_rule,
                status=NO_IMPROVING_PAIRING_STATUS,
                is_feasible=False,
                has_negative_pairing=False,
                infeasibility_reason=(
                    diagnostics.infeasibility_driver if diagnostics else NO_FEASIBLE_NEGATIVE_PAIRING
                ),
                total_cost=None,
                best_cost=None,
                selected_route_ids=[],
                selected_routes=[],
                covered_customers=set(),
                best_label=None,
                label_buckets=label_buckets,
                diagnostics=diagnostics,
                pairing_network=pairing_network,
            )

        best_state = min(
            sink_states,
            key=lambda state: (
                state.label.cost,
                len(state.pairing_route_sequence),
                state.pairing_route_sequence,
            ),
        )
        selected_routes = [
            next(route for route in reduced_routes if route.route_id == route_id)
            for route_id in best_state.pairing_route_sequence
        ]
        total_cost = best_state.label.cost

        return Phase2DPResult(
            mode=self.mode,
            service_rule=self.service_rule,
            status=OPTIMAL_STATUS if total_cost < 0 else NO_IMPROVING_PAIRING_STATUS,
            is_feasible=True,
            has_negative_pairing=total_cost < 0,
            infeasibility_reason=None,
            total_cost=total_cost,
            best_cost=total_cost,
            selected_route_ids=list(best_state.pairing_route_sequence),
            selected_routes=selected_routes,
            covered_customers=self._covered_customer_set(selected_routes),
            best_label=best_state.label,
            label_buckets=label_buckets,
            diagnostics=diagnostics,
            pairing_network=pairing_network,
        )

    def _coerce_routes(
        self,
        routes_or_phase1: Phase1Result | Sequence[Route],
    ) -> List[Route]:
        if isinstance(routes_or_phase1, Phase1Result):
            phase1_result = routes_or_phase1
            if phase1_result.negative_routes:
                return [self._coerce_route(route) for route in phase1_result.negative_routes]
            if phase1_result.negative_cost_routes:
                return [
                    self._coerce_route(route)
                    for route in phase1_result.negative_cost_routes
                ]
            return [self._coerce_route(route) for route in phase1_result.exported_routes]
        return [self._coerce_route(route) for route in routes_or_phase1]

    @staticmethod
    def _coerce_route(route: Route) -> Route:
        if isinstance(route, Route):
            return route

        return Route(
            route_id=route.route_id,
            cost=route.cost,
            local_resources=list(getattr(route, "local_resources", [])),
            global_resources=list(getattr(route, "global_resources", [])),
            covered_customers=set(route.covered_customers),
            path=list(route.path),
            first_customer_in_route=getattr(route, "first_customer_in_route", None),
            customer_state_signature=tuple(
                getattr(route, "customer_state_signature", ())
            ),
        )

    def _reduce_route_pool(
        self,
        routes: Sequence[Route],
    ) -> tuple[List[Route], List[RouteReductionRecord]]:
        if not self.apply_route_reduction:
            return list(routes), []
        return reduce_pricing_route_pool(routes)

    def _build_pairing_network(
        self,
        routes: Sequence[Route],
        *,
        respect_global_limits: bool,
    ) -> Phase2PairingNetwork:
        source_node = 0
        sink_node = len(routes) + 1
        nodes: Dict[int, Phase2PairingNetworkNode] = {
            source_node: Phase2PairingNetworkNode(source_node, "source"),
            sink_node: Phase2PairingNetworkNode(sink_node, "sink"),
        }
        adjacency: Dict[int, List[int]] = {node_id: [] for node_id in range(sink_node + 1)}
        outgoing_arcs: Dict[int, List[Phase2PairingNetworkArc]] = {
            node_id: [] for node_id in range(sink_node + 1)
        }
        route_node_to_route: Dict[int, Route] = {}

        ordered_routes = sorted(routes, key=lambda route: route.route_id)

        for idx, route in enumerate(ordered_routes, start=1):
            nodes[idx] = Phase2PairingNetworkNode(
                node_id=idx,
                node_kind="route",
                route_id=route.route_id,
            )
            route_node_to_route[idx] = route

        for node_id, route in route_node_to_route.items():
            if self._route_can_start_pairing(route, respect_global_limits=respect_global_limits):
                self._add_pairing_arc(
                    adjacency=adjacency,
                    outgoing_arcs=outgoing_arcs,
                    tail=source_node,
                    head=node_id,
                    arc_kind="source_to_route",
                )
            if self._route_can_finish_pairing(route, respect_global_limits=respect_global_limits):
                self._add_pairing_arc(
                    adjacency=adjacency,
                    outgoing_arcs=outgoing_arcs,
                    tail=node_id,
                    head=sink_node,
                    arc_kind="route_to_sink",
                )

        route_nodes = sorted(route_node_to_route)
        for pos, tail_node in enumerate(route_nodes):
            tail_route = route_node_to_route[tail_node]
            for head_node in route_nodes[pos + 1 :]:
                head_route = route_node_to_route[head_node]
                if self._routes_can_chain_in_phase2(
                    current_route=tail_route,
                    next_route=head_route,
                    respect_global_limits=respect_global_limits,
                ):
                    self._add_pairing_arc(
                        adjacency=adjacency,
                        outgoing_arcs=outgoing_arcs,
                        tail=tail_node,
                        head=head_node,
                        arc_kind="route_to_route",
                    )

        return Phase2PairingNetwork(
            nodes=nodes,
            adjacency=adjacency,
            outgoing_arcs=outgoing_arcs,
            source_node=source_node,
            sink_node=sink_node,
            route_node_to_route=route_node_to_route,
            respect_global_limits=respect_global_limits,
        )

    @staticmethod
    def _add_pairing_arc(
        *,
        adjacency: Dict[int, List[int]],
        outgoing_arcs: Dict[int, List[Phase2PairingNetworkArc]],
        tail: int,
        head: int,
        arc_kind: str,
    ) -> None:
        adjacency[tail].append(head)
        outgoing_arcs[tail].append(
            Phase2PairingNetworkArc(
                tail=tail,
                head=head,
                arc_kind=arc_kind,
            )
        )

    def _initialize_pairing_network_state(
        self,
        *,
        pairing_network: Phase2PairingNetwork,
        structural_pairing_network: Phase2PairingNetwork,
    ) -> None:
        self._pairing_network = pairing_network
        self._structural_pairing_network = structural_pairing_network
        self._pairing_source = pairing_network.source_node
        self._pairing_sink = pairing_network.sink_node
        self._route_node_to_route = dict(pairing_network.route_node_to_route)
        self._pairing_node_id_by_route_id = {
            route.route_id: node_id
            for node_id, route in pairing_network.route_node_to_route.items()
        }
        self._structural_adjacency = {
            node_id: tuple(sorted(structural_pairing_network.adjacency[node_id]))
            for node_id in structural_pairing_network.adjacency
        }

    def _route_can_start_pairing(
        self,
        route: Route,
        *,
        respect_global_limits: bool,
    ) -> bool:
        if not respect_global_limits or not self.global_limits:
            return True
        return self._within_limits(list(route.global_resources), self.global_limits)

    def _route_can_finish_pairing(
        self,
        route: Route,
        *,
        respect_global_limits: bool,
    ) -> bool:
        return self._route_can_start_pairing(
            route,
            respect_global_limits=respect_global_limits,
        )

    def _routes_can_chain_in_phase2(
        self,
        *,
        current_route: Route,
        next_route: Route,
        respect_global_limits: bool,
    ) -> bool:
        """
        Decide whether ``next_route`` can be a legal continuation after ``current_route``
        in the specialized Phase 2 pairing network.

        The rule is intentionally phrased as a second-network continuation test:
        - forward route ordering enforces canonical path orientation
        - customer disjointness preserves pairing elementarity
        - a conservative global-resource screen keeps only continuations that can still
          belong to some feasible pairing under the vehicle-level limits
        """

        if current_route.route_id >= next_route.route_id:
            return False
        if not current_route.covered_customers.isdisjoint(next_route.covered_customers):
            return False
        if not respect_global_limits or not self.global_limits:
            return True

        combined_resources = self._vec_add(
            list(current_route.global_resources),
            list(next_route.global_resources),
        )
        return self._within_limits(combined_resources, self.global_limits)

    def _initial_pairing_state(self) -> _PairingStateRecord:
        if self._pairing_source is None:
            raise ValueError("Pairing-network source was not initialized.")

        label = Label(
            current_node=self._pairing_source,
            cost=0.0,
            resources=[0.0] * len(self.global_limits),
            unreachable_vector=[REACHABLE] * len(self.customer_ids),
        )
        support = self._compute_residual_pairing_support(
            current_node=self._pairing_source,
            selected_customer_ids=set(),
            resources=label.resources,
        )
        refreshed_vector = self._refresh_customer_states_from_support(
            current_vector=label.unreachable_vector,
            selected_customer_ids=set(),
            support=support,
        )
        refreshed_label = label.copy(
            unreachable_vector=refreshed_vector,
            unreachable_count=None,
        )
        return self._state_with_support(
            label=refreshed_label,
            pairing_route_sequence=[],
            pairing_node_path=[self._pairing_source],
            support=support,
        )

    def _run_labeling_search(
        self,
        initial_state: _PairingStateRecord,
    ) -> Dict[int, List[_PairingStateRecord]]:
        if self._pairing_network is None:
            raise ValueError("Pairing network was not initialized.")

        buckets: Dict[int, List[_PairingStateRecord]] = {
            node_id: [] for node_id in self._pairing_network.nodes
        }
        buckets[initial_state.label.current_node].append(initial_state)

        pending_nodes: List[int] = [initial_state.label.current_node]
        while pending_nodes:
            node = pending_nodes.pop(0)
            changed_nodes = self._extend_from_pairing_node(node=node, buckets=buckets)
            for changed in changed_nodes:
                if changed not in pending_nodes:
                    pending_nodes.append(changed)

        return buckets

    def _extend_from_pairing_node(
        self,
        *,
        node: int,
        buckets: Dict[int, List[_PairingStateRecord]],
    ) -> List[int]:
        if self._pairing_network is None:
            raise ValueError("Pairing network was not initialized.")

        changed_nodes: List[int] = []
        for state in list(buckets[node]):
            for succ in self._pairing_network.adjacency.get(node, []):
                new_state = self._try_extend_pairing_state(state=state, succ=succ)
                if new_state is None:
                    continue

                changed = self._insert_with_dominance(new_state, buckets[succ])
                if changed and succ not in changed_nodes:
                    changed_nodes.append(succ)
        return changed_nodes

    def _try_extend_pairing_state(
        self,
        *,
        state: _PairingStateRecord,
        succ: int,
    ) -> _PairingStateRecord | None:
        if self._pairing_network is None or self._pairing_sink is None:
            raise ValueError("Pairing network state was not initialized.")

        current_label = state.label
        current_customers = self._selected_customers_from_vector(
            current_label.unreachable_vector
        )

        if succ == self._pairing_sink:
            if not state.pairing_route_sequence:
                return None
            sink_label = current_label.copy(current_node=succ)
            sink_support = self._compute_residual_pairing_support(
                current_node=succ,
                selected_customer_ids=current_customers,
                resources=sink_label.resources,
            )
            sink_vector = self._refresh_customer_states_from_support(
                current_vector=sink_label.unreachable_vector,
                selected_customer_ids=current_customers,
                support=sink_support,
            )
            sink_label = sink_label.copy(
                unreachable_vector=sink_vector,
                unreachable_count=None,
            )
            return self._state_with_support(
                label=sink_label,
                pairing_route_sequence=list(state.pairing_route_sequence),
                pairing_node_path=list(state.pairing_node_path) + [succ],
                support=sink_support,
            )

        if not self._is_feasible_pairing_extension(
            state=state,
            next_node=succ,
        ):
            return None

        route = self._route_node_to_route[succ]
        new_resources = self._vec_add(
            list(current_label.resources),
            list(route.global_resources),
        )
        new_customers = current_customers.union(route.covered_customers)
        new_vector = self._visit_customers_in_pairing_vector(
            current_vector=current_label.unreachable_vector,
            customers_to_visit=sorted(route.covered_customers),
        )
        support = self._compute_residual_pairing_support(
            current_node=succ,
            selected_customer_ids=new_customers,
            resources=new_resources,
        )
        if not self._can_still_finish_pairing_after_extension(
            next_node=succ,
            support=support,
        ):
            return None
        refreshed_vector = self._refresh_customer_states_from_support(
            current_vector=new_vector,
            selected_customer_ids=new_customers,
            support=support,
        )
        new_label = Label(
            current_node=succ,
            cost=current_label.cost + route.cost,
            resources=new_resources,
            unreachable_vector=refreshed_vector,
        )
        return self._state_with_support(
            label=new_label,
            pairing_route_sequence=list(state.pairing_route_sequence) + [route.route_id],
            pairing_node_path=list(state.pairing_node_path) + [succ],
            support=support,
        )

    def _is_feasible_pairing_extension(
        self,
        *,
        state: _PairingStateRecord,
        next_node: int,
    ) -> bool:
        """
        Check whether the given route node can legally continue the current partial
        pairing path in the second network.

        This is stronger than generic subset compatibility:
        - the continuation must exist as a structural second-network arc
        - the route must remain customer-disjoint from the current pairing
        - the cumulative pairing resource usage must remain feasible
        """

        current_node = state.label.current_node
        structural_successors = self._structural_adjacency.get(current_node, ())
        if next_node not in structural_successors:
            return False

        next_route = self._route_node_to_route[next_node]
        selected_customers = self._selected_customers_from_vector(
            state.label.unreachable_vector
        )
        if not selected_customers.isdisjoint(next_route.covered_customers):
            return False

        new_resources = self._vec_add(
            list(state.label.resources),
            list(next_route.global_resources),
        )
        if self.global_limits and not self._within_limits(new_resources, self.global_limits):
            return False

        return True

    def _can_still_finish_pairing_after_extension(
        self,
        *,
        next_node: int,
        support: _ResidualPairingSupport,
    ) -> bool:
        """
        Decide whether the extended partial pairing can still terminate cleanly in the
        specialized second network.

        In this no-time-window LRSP setting, every nonempty partial pairing may always
        finish immediately by taking its direct route-to-sink arc, so this helper is
        mostly a semantic hook for the second-network logic and diagnostics.
        """

        if self._pairing_sink is None:
            raise ValueError("Pairing-network sink was not initialized.")
        return True

    def _compute_residual_pairing_support(
        self,
        *,
        current_node: int,
        selected_customer_ids: Set[int],
        resources: Sequence[float],
    ) -> _ResidualPairingSupport:
        structural_nodes: List[int] = []
        feasible_nodes: List[int] = []
        structural_profile = [0] * len(self.customer_ids)
        feasible_profile = [0] * len(self.customer_ids)

        for candidate in self._structural_adjacency.get(current_node, ()):
            if candidate == self._pairing_sink:
                continue
            route = self._route_node_to_route[candidate]
            if not selected_customer_ids.isdisjoint(route.covered_customers):
                continue

            structural_nodes.append(candidate)
            for customer_id in route.covered_customers:
                structural_profile[self.customer_index[customer_id]] += 1

            candidate_resources = self._vec_add(
                list(resources),
                list(route.global_resources),
            )
            if self.global_limits and not self._within_limits(
                candidate_resources,
                self.global_limits,
            ):
                continue

            feasible_nodes.append(candidate)
            for customer_id in route.covered_customers:
                feasible_profile[self.customer_index[customer_id]] += 1

        return _ResidualPairingSupport(
            structural_continuation_nodes=tuple(structural_nodes),
            feasible_continuation_nodes=tuple(feasible_nodes),
            structural_customer_support_profile=tuple(structural_profile),
            feasible_customer_support_profile=tuple(feasible_profile),
        )

    def _refresh_customer_states_from_support(
        self,
        *,
        current_vector: Sequence[int],
        selected_customer_ids: Set[int],
        support: _ResidualPairingSupport,
    ) -> List[int]:
        """
        Refresh customer states using residual feasibility in the specialized second
        network, not merely route-pool membership.

        Semantics:
        - visited customers stay positive
        - ``0`` means at least one continuation route can still support the customer
          now under the current pairing resource state
        - ``-2`` means only the structural second network still supports the customer,
          but no currently globally feasible continuation does
        - ``-1`` means the residual second network offers no continuation route that
          could still serve the customer from this pairing state
        """

        refreshed = list(current_vector)
        for customer_id, idx in self.customer_index.items():
            value = refreshed[idx]
            if Label.is_visited(value) or Label.is_perm_unreachable(value):
                continue
            if customer_id in selected_customer_ids:
                continue

            feasible_support = support.feasible_customer_support_profile[idx]
            structural_support = support.structural_customer_support_profile[idx]
            if feasible_support > 0:
                refreshed[idx] = REACHABLE
            elif structural_support > 0:
                refreshed[idx] = TEMP_UNREACHABLE
            else:
                refreshed[idx] = PERM_UNREACHABLE
        return refreshed

    def _state_with_support(
        self,
        *,
        label: Label,
        pairing_route_sequence: List[int],
        pairing_node_path: List[int],
        support: _ResidualPairingSupport,
    ) -> _PairingStateRecord:
        return _PairingStateRecord(
            label=label,
            pairing_route_sequence=pairing_route_sequence,
            pairing_node_path=pairing_node_path,
            structural_continuation_nodes=support.structural_continuation_nodes,
            feasible_continuation_nodes=support.feasible_continuation_nodes,
            structural_continuation_node_set=frozenset(
                support.structural_continuation_nodes
            ),
            feasible_continuation_node_set=frozenset(
                support.feasible_continuation_nodes
            ),
            structural_customer_support_profile=support.structural_customer_support_profile,
            feasible_customer_support_profile=support.feasible_customer_support_profile,
        )

    def _selected_customers_from_vector(self, vector: Sequence[int]) -> Set[int]:
        selected: Set[int] = set()
        for customer_id, idx in self.customer_index.items():
            if Label.is_visited(vector[idx]):
                selected.add(customer_id)
        return selected

    def _visit_customers_in_pairing_vector(
        self,
        *,
        current_vector: Sequence[int],
        customers_to_visit: Sequence[int],
    ) -> List[int]:
        updated = list(current_vector)
        next_visit_position = self._next_visit_position(updated)
        for customer_id in customers_to_visit:
            idx = self.customer_index[customer_id]
            updated[idx] = next_visit_position
            next_visit_position += 1
        return updated

    def _insert_with_dominance(
        self,
        candidate: _PairingStateRecord,
        bucket: List[_PairingStateRecord],
    ) -> bool:
        survivors: List[_PairingStateRecord] = []
        changed = False
        for incumbent in bucket:
            if self._pairing_state_dominates(incumbent, candidate):
                return False
            if self._pairing_state_dominates(candidate, incumbent):
                changed = True
                continue
            survivors.append(incumbent)
        survivors.append(candidate)
        survivors.sort(key=self._pairing_state_sort_key)
        bucket[:] = survivors
        return True

    def _pairing_state_sort_key(
        self,
        state: _PairingStateRecord,
    ) -> tuple[object, ...]:
        return (
            state.label.cost,
            state.label.resources,
            tuple(-value for value in state.feasible_customer_support_profile),
            tuple(-value for value in state.structural_customer_support_profile),
            tuple(sorted(state.feasible_continuation_node_set)),
            tuple(sorted(state.structural_continuation_node_set)),
            state.pairing_route_sequence,
        )

    def _pairing_state_dominates(
        self,
        a: _PairingStateRecord,
        b: _PairingStateRecord,
    ) -> bool:
        """
        Dominance between two partial pairings ending at the same second-network node.

        The dominating partial pairing must:
        - end at the same pairing-network node
        - serve the same customers so far
        - use no more cost or pairing-level global resources
        - preserve at least as strong a residual continuation structure in the second
          network, both in terms of feasible continuation nodes and customer support
        """

        if a.label.current_node != b.label.current_node:
            return False
        if not self._same_visited_customer_signature(
            a.label.unreachable_vector,
            b.label.unreachable_vector,
        ):
            return False

        resources_le = self._vec_le(a.label.resources, b.label.resources)
        if not (a.label.cost <= b.label.cost and resources_le):
            return False

        if not b.feasible_continuation_node_set.issubset(a.feasible_continuation_node_set):
            return False
        if not b.structural_continuation_node_set.issubset(
            a.structural_continuation_node_set
        ):
            return False
        if not self._support_profile_ge(
            a.feasible_customer_support_profile,
            b.feasible_customer_support_profile,
        ):
            return False
        if not self._support_profile_ge(
            a.structural_customer_support_profile,
            b.structural_customer_support_profile,
        ):
            return False

        return (
            a.label.cost < b.label.cost
            or self._vec_lt(a.label.resources, b.label.resources)
            or a.feasible_continuation_node_set != b.feasible_continuation_node_set
            or a.structural_continuation_node_set != b.structural_continuation_node_set
            or a.feasible_customer_support_profile
            != b.feasible_customer_support_profile
            or a.structural_customer_support_profile
            != b.structural_customer_support_profile
        )

    @staticmethod
    def _support_profile_ge(
        lhs: Sequence[int],
        rhs: Sequence[int],
    ) -> bool:
        return all(a >= b for a, b in zip(lhs, rhs))

    @staticmethod
    def _same_visited_customer_signature(
        lhs: Sequence[int],
        rhs: Sequence[int],
    ) -> bool:
        if len(lhs) != len(rhs):
            return False
        return all(Label.is_visited(a) == Label.is_visited(b) for a, b in zip(lhs, rhs))

    def _build_diagnostics(
        self,
        *,
        input_route_count: int,
        negative_routes: Sequence[Route],
        reduced_routes: Sequence[Route],
        reduction_records: Sequence[RouteReductionRecord],
        pairing_network: Phase2PairingNetwork | None,
        structural_pairing_network: Phase2PairingNetwork | None,
    ) -> Phase2PairingDiagnostics | None:
        if not self.collect_diagnostics:
            return None

        route_ids_by_covered_customer_set: Dict[Tuple[int, ...], List[int]] = {}
        route_ids_by_first_customer: Dict[int | None, List[int]] = {}
        route_ids_by_cover_and_signature: Dict[
            Tuple[Tuple[int, ...], Tuple[int, ...]],
            List[int],
        ] = {}
        globally_infeasible_single_route_ids: List[int] = []

        for route in reduced_routes:
            cover_key = tuple(sorted(route.covered_customers))
            route_ids_by_covered_customer_set.setdefault(cover_key, []).append(route.route_id)
            route_ids_by_first_customer.setdefault(
                route.first_customer_in_route,
                [],
            ).append(route.route_id)
            signature_key = (
                cover_key,
                tuple(route.customer_state_signature or ()),
            )
            route_ids_by_cover_and_signature.setdefault(signature_key, []).append(
                route.route_id
            )
            if self.global_limits and not self._within_limits(
                list(route.global_resources),
                self.global_limits,
            ):
                globally_infeasible_single_route_ids.append(route.route_id)

        incompatible_route_pairs: List[Tuple[int, int]] = []
        ordered_reduced_routes = sorted(reduced_routes, key=lambda route: route.route_id)
        for idx, route in enumerate(ordered_reduced_routes):
            for other in ordered_reduced_routes[idx + 1 :]:
                if not route.covered_customers.isdisjoint(other.covered_customers):
                    incompatible_route_pairs.append((route.route_id, other.route_id))

        source_to_route_arc_count = self._count_arcs_by_kind(
            pairing_network,
            "source_to_route",
        )
        route_to_route_arc_count = self._count_arcs_by_kind(
            pairing_network,
            "route_to_route",
        )
        route_to_sink_arc_count = self._count_arcs_by_kind(
            pairing_network,
            "route_to_sink",
        )
        structural_source_to_route_arc_count = self._count_arcs_by_kind(
            structural_pairing_network,
            "source_to_route",
        )
        structural_route_to_route_arc_count = self._count_arcs_by_kind(
            structural_pairing_network,
            "route_to_route",
        )
        structural_route_to_sink_arc_count = self._count_arcs_by_kind(
            structural_pairing_network,
            "route_to_sink",
        )

        outgoing_continuation_counts_by_route_id = self._continuation_counts_by_route_id(
            pairing_network
        )
        structural_continuation_counts_by_route_id = self._continuation_counts_by_route_id(
            structural_pairing_network
        )
        startable_route_ids = self._startable_route_ids(pairing_network)
        structurally_startable_route_ids = self._startable_route_ids(
            structural_pairing_network
        )
        no_feasible_start_route_ids = sorted(
            set(structurally_startable_route_ids) - set(startable_route_ids)
        )
        isolated_negative_route_ids = self._isolated_route_ids(pairing_network)
        no_feasible_continuation_route_ids = sorted(
            route_id
            for route_id, count in outgoing_continuation_counts_by_route_id.items()
            if count == 0
        )
        network_failure_mode = self._infer_network_failure_mode(
            negative_routes=negative_routes,
            pairing_network=pairing_network,
            structural_pairing_network=structural_pairing_network,
            startable_route_ids=startable_route_ids,
            structurally_startable_route_ids=structurally_startable_route_ids,
            route_to_route_arc_count=route_to_route_arc_count,
            structural_route_to_route_arc_count=structural_route_to_route_arc_count,
        )
        infeasibility_driver = self._map_failure_mode_to_driver(network_failure_mode)
        diagnostic_summary = self._build_diagnostic_summary(
            negative_route_count=len(negative_routes),
            reduced_route_count=len(reduced_routes),
            source_to_route_arc_count=source_to_route_arc_count,
            route_to_route_arc_count=route_to_route_arc_count,
            route_to_sink_arc_count=route_to_sink_arc_count,
            structural_source_to_route_arc_count=structural_source_to_route_arc_count,
            structural_route_to_route_arc_count=structural_route_to_route_arc_count,
            isolated_negative_route_ids=isolated_negative_route_ids,
            network_failure_mode=network_failure_mode,
        )

        kept_route_ids = sorted(route.route_id for route in reduced_routes)
        removed_route_ids = sorted(
            {
                record.removed_route_id
                for record in reduction_records
            }
        )

        return Phase2PairingDiagnostics(
            input_route_count=input_route_count,
            negative_route_count=len(negative_routes),
            reduced_route_count=len(reduced_routes),
            pairing_network_arc_count=pairing_network.arc_count if pairing_network else 0,
            source_to_route_arc_count=source_to_route_arc_count,
            route_to_route_arc_count=route_to_route_arc_count,
            route_to_sink_arc_count=route_to_sink_arc_count,
            structural_pairing_network_arc_count=(
                structural_pairing_network.arc_count if structural_pairing_network else 0
            ),
            structural_source_to_route_arc_count=structural_source_to_route_arc_count,
            structural_route_to_route_arc_count=structural_route_to_route_arc_count,
            structural_route_to_sink_arc_count=structural_route_to_sink_arc_count,
            outgoing_continuation_counts_by_route_id=outgoing_continuation_counts_by_route_id,
            structural_continuation_counts_by_route_id=(
                structural_continuation_counts_by_route_id
            ),
            startable_route_ids=startable_route_ids,
            structurally_startable_route_ids=structurally_startable_route_ids,
            no_feasible_start_route_ids=no_feasible_start_route_ids,
            isolated_negative_route_ids=isolated_negative_route_ids,
            no_feasible_continuation_route_ids=no_feasible_continuation_route_ids,
            route_ids_by_covered_customer_set=route_ids_by_covered_customer_set,
            route_ids_by_first_customer=route_ids_by_first_customer,
            route_ids_by_cover_and_signature=route_ids_by_cover_and_signature,
            incompatible_route_pairs=incompatible_route_pairs,
            globally_infeasible_single_route_ids=sorted(
                globally_infeasible_single_route_ids
            ),
            reduction_records=list(reduction_records),
            kept_route_ids=kept_route_ids,
            removed_route_ids=removed_route_ids,
            network_failure_mode=network_failure_mode,
            infeasibility_driver=infeasibility_driver,
            diagnostic_summary=diagnostic_summary,
        )

    @staticmethod
    def _count_arcs_by_kind(
        pairing_network: Phase2PairingNetwork | None,
        arc_kind: str,
    ) -> int:
        if pairing_network is None:
            return 0
        return sum(
            1
            for arcs in pairing_network.outgoing_arcs.values()
            for arc in arcs
            if arc.arc_kind == arc_kind
        )

    @staticmethod
    def _continuation_counts_by_route_id(
        pairing_network: Phase2PairingNetwork | None,
    ) -> Dict[int, int]:
        if pairing_network is None:
            return {}

        counts: Dict[int, int] = {}
        for node_id, route in pairing_network.route_node_to_route.items():
            counts[route.route_id] = sum(
                1
                for arc in pairing_network.outgoing_arcs.get(node_id, [])
                if arc.arc_kind == "route_to_route"
            )
        return counts

    @staticmethod
    def _startable_route_ids(
        pairing_network: Phase2PairingNetwork | None,
    ) -> List[int]:
        if pairing_network is None:
            return []

        startable: List[int] = []
        for arc in pairing_network.outgoing_arcs.get(pairing_network.source_node, []):
            if arc.arc_kind != "source_to_route":
                continue
            route = pairing_network.route_node_to_route[arc.head]
            startable.append(route.route_id)
        return sorted(startable)

    @staticmethod
    def _isolated_route_ids(
        pairing_network: Phase2PairingNetwork | None,
    ) -> List[int]:
        if pairing_network is None:
            return []

        isolated: List[int] = []
        for node_id, route in pairing_network.route_node_to_route.items():
            outgoing_route_to_route = any(
                arc.arc_kind == "route_to_route"
                for arc in pairing_network.outgoing_arcs.get(node_id, [])
            )
            incoming_route_to_route = any(
                arc.arc_kind == "route_to_route" and arc.head == node_id
                for arcs in pairing_network.outgoing_arcs.values()
                for arc in arcs
            )
            if not outgoing_route_to_route and not incoming_route_to_route:
                isolated.append(route.route_id)
        return sorted(isolated)

    def _infer_network_failure_mode(
        self,
        *,
        negative_routes: Sequence[Route],
        pairing_network: Phase2PairingNetwork | None,
        structural_pairing_network: Phase2PairingNetwork | None,
        startable_route_ids: Sequence[int],
        structurally_startable_route_ids: Sequence[int],
        route_to_route_arc_count: int,
        structural_route_to_route_arc_count: int,
    ) -> str | None:
        if not negative_routes:
            return "no_negative_phase1_routes"
        if not structurally_startable_route_ids:
            return "no_structural_source_start_route"
        if not startable_route_ids:
            return "no_feasible_source_start_route"
        if structural_pairing_network and structural_route_to_route_arc_count == 0:
            return "singleton_pairings_only_structurally"
        if pairing_network and route_to_route_arc_count == 0:
            return "singleton_pairings_only_under_global_limits"
        return None

    @staticmethod
    def _map_failure_mode_to_driver(failure_mode: str | None) -> str | None:
        if failure_mode == "no_negative_phase1_routes":
            return NO_NEGATIVE_ROUTES_FROM_PHASE1
        if failure_mode in {
            "no_structural_source_start_route",
            "singleton_pairings_only_structurally",
        }:
            return NO_FEASIBLE_NEGATIVE_PAIRING
        if failure_mode in {
            "no_feasible_source_start_route",
            "singleton_pairings_only_under_global_limits",
        }:
            return GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING
        return None

    @staticmethod
    def _build_diagnostic_summary(
        *,
        negative_route_count: int,
        reduced_route_count: int,
        source_to_route_arc_count: int,
        route_to_route_arc_count: int,
        route_to_sink_arc_count: int,
        structural_source_to_route_arc_count: int,
        structural_route_to_route_arc_count: int,
        isolated_negative_route_ids: Sequence[int],
        network_failure_mode: str | None,
    ) -> str:
        if network_failure_mode == "no_negative_phase1_routes":
            return (
                "Phase 2 pairing network is empty because Phase 1 produced no negative "
                "reduced-cost routes."
            )
        if network_failure_mode == "no_structural_source_start_route":
            return (
                "Negative routes exist, but the structural Phase 2 pairing network has "
                "no source-to-route start arcs."
            )
        if network_failure_mode == "no_feasible_source_start_route":
            return (
                "Negative routes exist, but the vehicle-level pairing limits block all "
                "source-to-route starts in the feasible second network."
            )
        if network_failure_mode == "singleton_pairings_only_structurally":
            return (
                "The structural second network contains start arcs, but no route-to-route "
                "continuations; only singleton pairings are structurally possible."
            )
        if network_failure_mode == "singleton_pairings_only_under_global_limits":
            return (
                "The structural second network admits chaining, but global pairing limits "
                "remove all feasible route-to-route continuations."
            )
        return (
            "Phase 2 built a specialized LRSP pairing network with "
            f"{reduced_route_count}/{negative_route_count} negative routes kept, "
            f"{source_to_route_arc_count} feasible source-start arcs, "
            f"{route_to_route_arc_count} feasible route-to-route continuation arcs, "
            f"{route_to_sink_arc_count} route-to-sink finish arcs, and "
            f"{len(isolated_negative_route_ids)} isolated negative route nodes. "
            f"The structural network contains {structural_source_to_route_arc_count} "
            f"source starts and {structural_route_to_route_arc_count} continuation arcs."
        )

    @staticmethod
    def _covered_customer_set(routes: Sequence[Route]) -> Set[int]:
        covered: Set[int] = set()
        for route in routes:
            covered.update(route.covered_customers)
        return covered


Phase2DPSolver = Phase2DPPricingSolver

RouteNetworkNode = Phase2PairingNetworkNode
RouteNetworkArc = Phase2PairingNetworkArc
RouteNetwork = Phase2PairingNetwork
Phase2Diagnostics = Phase2PairingDiagnostics

LRSPRouteNetworkNode = Phase2PairingNetworkNode
LRSPRouteNetworkArc = Phase2PairingNetworkArc
LRSPRouteNetwork = Phase2PairingNetwork
LRSPPhase2Diagnostics = Phase2PairingDiagnostics
LRSPPairingDPResult = Phase2DPResult
LRSPPhase2DPPricingSolver = Phase2DPPricingSolver
