from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Dict, List, Sequence


class NodeType(IntEnum):
    SOURCE = 0
    CUSTOMER = 1
    SINK = 2


@dataclass(slots=True)
class Node:
    id: int
    node_type: NodeType


@dataclass(slots=True)
class Arc:
    tail: int
    head: int
    cost: float
    local_res: Sequence[float] | None = None
    global_res: Sequence[float] | None = None

    def __post_init__(self) -> None:
        self.local_res = list(self.local_res or [])
        self.global_res = list(self.global_res or [])


class MESPPRCInstance:
    """
    Directed graph container for the research prototype.

    Resource families remain intentionally separated:
    - local resources are enforced in Phase 1 while generating single-trip routes
    - global resources are enforced in Phase 2 while combining generated routes

    The instance stores both vectors on every arc, but validation never merges them into
    a single undifferentiated resource system.
    """

    def __init__(
        self,
        *,
        local_limits: Sequence[float] | None = None,
        global_limits: Sequence[float] | None = None,
        required_customers: Sequence[int] | None = None,
        exact_once_service: bool = True,
    ) -> None:
        self.nodes: Dict[int, Node] = {}
        self.arcs: Dict[tuple[int, int], Arc] = {}
        self.out_neighbors: Dict[int, List[int]] = {}
        self.in_neighbors: Dict[int, List[int]] = {}
        self.source: int | None = None
        self.sink: int | None = None
        self.local_limits = list(local_limits or [])
        self.global_limits = list(global_limits or [])
        self._explicit_required_customers = (
            None if required_customers is None else set(required_customers)
        )
        self.exact_once_service = bool(exact_once_service)

    def add_node(self, node: Node) -> None:
        if node.id in self.nodes:
            raise ValueError(f"Node {node.id} already exists.")

        self.nodes[node.id] = node
        self.out_neighbors[node.id] = []
        self.in_neighbors[node.id] = []

        if node.node_type == NodeType.SOURCE:
            if self.source is not None:
                raise ValueError("Instance already has a source node.")
            self.source = node.id
        elif node.node_type == NodeType.SINK:
            if self.sink is not None:
                raise ValueError("Instance already has a sink node.")
            self.sink = node.id

    def add_arc(self, arc: Arc) -> None:
        if arc.tail not in self.nodes:
            raise ValueError(f"Arc tail {arc.tail} is not a known node.")
        if arc.head not in self.nodes:
            raise ValueError(f"Arc head {arc.head} is not a known node.")
        if (arc.tail, arc.head) in self.arcs:
            raise ValueError(f"Arc ({arc.tail}, {arc.head}) already exists.")

        self._validate_arc_dimensions(arc)

        self.arcs[(arc.tail, arc.head)] = arc
        self.out_neighbors[arc.tail].append(arc.head)
        self.in_neighbors[arc.head].append(arc.tail)

    def set_resource_limits(
        self,
        *,
        local_limits: Sequence[float] | None = None,
        global_limits: Sequence[float] | None = None,
    ) -> None:
        if local_limits is not None:
            self.local_limits = list(local_limits)
        if global_limits is not None:
            self.global_limits = list(global_limits)

        self.validate()

    def customers(self) -> List[int]:
        return sorted(
            node_id
            for node_id, node in self.nodes.items()
            if node.node_type == NodeType.CUSTOMER
        )

    def required_customers(self) -> List[int]:
        """
        Return the customer nodes that must be served in the final Phase 2 solution.

        By default every customer node is required.
        """

        if self._explicit_required_customers is None:
            return self.customers()
        return sorted(self._explicit_required_customers)

    def successors(self, node: int) -> List[int]:
        return list(self.out_neighbors.get(node, []))

    def predecessors(self, node: int) -> List[int]:
        return list(self.in_neighbors.get(node, []))

    def get_arc(self, i: int, j: int) -> Arc:
        return self.arcs[(i, j)]

    def validate(self) -> None:
        if self.source is None:
            raise ValueError("Instance source is not defined.")
        if self.sink is None:
            raise ValueError("Instance sink is not defined.")
        if not self.arcs:
            raise ValueError("Instance contains no arcs.")

        self._validate_resource_family(
            resource_name="local",
            limits=self.local_limits,
            arc_vectors=[arc.local_res for arc in self.arcs.values()],
        )
        self._validate_resource_family(
            resource_name="global",
            limits=self.global_limits,
            arc_vectors=[arc.global_res for arc in self.arcs.values()],
        )
        self._validate_required_customers()

    def _validate_arc_dimensions(self, arc: Arc) -> None:
        self._validate_single_arc_family(
            resource_name="local",
            vector=arc.local_res,
            limits=self.local_limits,
            observed_dimension=self._observed_arc_dimension("local"),
            arc=arc,
        )
        self._validate_single_arc_family(
            resource_name="global",
            vector=arc.global_res,
            limits=self.global_limits,
            observed_dimension=self._observed_arc_dimension("global"),
            arc=arc,
        )

    def _observed_arc_dimension(self, resource_name: str) -> int | None:
        if not self.arcs:
            return None

        if resource_name == "local":
            return len(next(iter(self.arcs.values())).local_res)
        return len(next(iter(self.arcs.values())).global_res)

    def _validate_single_arc_family(
        self,
        *,
        resource_name: str,
        vector: Sequence[float],
        limits: Sequence[float],
        observed_dimension: int | None,
        arc: Arc,
    ) -> None:
        vector_dim = len(vector)

        if observed_dimension is not None and vector_dim != observed_dimension:
            raise ValueError(
                f"Arc ({arc.tail}, {arc.head}) {resource_name} resource dimension "
                f"{vector_dim} does not match the existing {resource_name} resource "
                f"dimension {observed_dimension}."
            )

        if limits and vector_dim != len(limits):
            raise ValueError(
                f"Arc ({arc.tail}, {arc.head}) {resource_name} resource dimension "
                f"{vector_dim} does not match {resource_name} limits dimension "
                f"{len(limits)}."
            )

    def _validate_resource_family(
        self,
        *,
        resource_name: str,
        limits: Sequence[float],
        arc_vectors: Sequence[Sequence[float]],
    ) -> None:
        observed_dimensions = {len(vector) for vector in arc_vectors}
        if len(observed_dimensions) > 1:
            raise ValueError(
                f"Arc {resource_name} resource dimensions are inconsistent across the "
                "instance."
            )

        observed_dimension = observed_dimensions.pop() if observed_dimensions else 0
        limit_dimension = len(limits)

        if observed_dimension > 0 and not limits:
            raise ValueError(
                f"{resource_name.capitalize()} resource limits must be defined when arcs "
                f"carry {resource_name} resource values."
            )

        if limit_dimension != observed_dimension:
            raise ValueError(
                f"{resource_name.capitalize()} resource limit dimension "
                f"{limit_dimension} does not match the arc {resource_name} resource "
                f"dimension {observed_dimension}."
            )

    def _validate_required_customers(self) -> None:
        if self._explicit_required_customers is None:
            return

        unknown = sorted(
            customer_id
            for customer_id in self._explicit_required_customers
            if customer_id not in self.nodes
        )
        if unknown:
            raise ValueError(
                "Required customers must be known customer nodes. "
                f"Unknown ids: {unknown}."
            )

        invalid = sorted(
            customer_id
            for customer_id in self._explicit_required_customers
            if self.nodes[customer_id].node_type != NodeType.CUSTOMER
        )
        if invalid:
            raise ValueError(
                "Required customers must have customer node type. "
                f"Invalid ids: {invalid}."
            )
