from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Tuple

from mespprc import Arc, MESPPRCInstance, Node, NodeType

from .column import MasterDuals
from .instance import Customer, Facility, LRSPInstance


SOURCE_NODE_ID = 0


@dataclass(slots=True)
class FacilityPricingGraph:
    """
    Per-facility reduced-cost graph passed to a MESPPRC engine.

    `pricing_instance` is the `MESPPRCInstance` consumed by the user's MESPPRC code.
    Arc costs in `pricing_instance` already encode the dual-adjusted reduced cost.
    `base_arc_cost` keeps the raw (pre-dual) travel cost on each arc so the LRSP
    layer can reconstruct the true pairing cost after the pricing engine returns.
    """

    pricing_instance: MESPPRCInstance
    source_node: int
    sink_node: int
    customer_by_id: Dict[int, Customer]
    base_arc_cost: Dict[Tuple[int, int], float]
    pairing_constant: float


def build_facility_pricing_graph(
    instance: LRSPInstance,
    facility: Facility,
    duals: MasterDuals,
) -> FacilityPricingGraph:
    """
    Build the per-facility reduced-cost graph for the pricing subproblem.

    Reduced-cost convention. The master rows are written in the canonical
    `==` / `<=` / `>=` form, so the dual sign returned by PuLP is the same one we
    subtract here:

        rc(arc into customer i, vehicle from facility j) =
              base_travel_cost
            - coverage_dual[i]
            - facility_capacity_dual[j] * demand[i]
            - facility_customer_link_dual[(i, j)]

    The arc-into-sink keeps its raw travel cost: returning to the facility neither
    serves a new customer nor adds demand.

    The pairing constant (a per-pairing dual offset that does not depend on the
    chosen route) is reported separately so the LRSP layer can compute the column
    reduced cost as `column_total_arc_cost + pairing_constant`.
    """

    customer_by_id = {customer.id: customer for customer in instance.customers}
    sink_node = max(customer_by_id) + 1 if customer_by_id else SOURCE_NODE_ID + 1
    if sink_node == SOURCE_NODE_ID:
        sink_node = SOURCE_NODE_ID + 1

    has_time_limit = instance.vehicle_time_limit is not None
    local_limits: List[float] = [instance.vehicle_capacity]
    global_limits: List[float] = []
    if has_time_limit:
        local_limits.append(float(instance.vehicle_time_limit))
        global_limits.append(float(instance.vehicle_time_limit))

    pricing_instance = MESPPRCInstance(
        local_limits=local_limits,
        global_limits=global_limits,
    )
    pricing_instance.add_node(Node(SOURCE_NODE_ID, NodeType.SOURCE))
    for customer in instance.customers:
        pricing_instance.add_node(Node(customer.id, NodeType.CUSTOMER))
    pricing_instance.add_node(Node(sink_node, NodeType.SINK))

    facility_capacity_dual = duals.facility_capacity.get(facility.id, 0.0)
    base_arc_cost: Dict[Tuple[int, int], float] = {}

    def coords(node_id: int) -> Tuple[float, float]:
        if node_id == SOURCE_NODE_ID or node_id == sink_node:
            return facility.x, facility.y
        customer = customer_by_id[node_id]
        return customer.x, customer.y

    def reduced_cost_into(head: int, base_cost: float) -> float:
        if head == sink_node:
            return base_cost
        customer = customer_by_id[head]
        coverage = duals.coverage.get(customer.id, 0.0)
        link = duals.facility_customer_link.get((customer.id, facility.id), 0.0)
        return base_cost - coverage - facility_capacity_dual * customer.demand - link

    def make_local(head: int, base_cost: float) -> List[float]:
        demand_inc = 0.0 if head == sink_node else customer_by_id[head].demand
        if has_time_limit:
            return [demand_inc, base_cost]
        return [demand_inc]

    def make_global(base_cost: float) -> List[float]:
        if has_time_limit:
            return [base_cost]
        return []

    customer_ids = [customer.id for customer in instance.customers]

    for head in customer_ids:
        x1, y1 = coords(SOURCE_NODE_ID)
        x2, y2 = coords(head)
        base = instance.travel_cost(x1, y1, x2, y2)
        base_arc_cost[(SOURCE_NODE_ID, head)] = base
        pricing_instance.add_arc(
            Arc(
                tail=SOURCE_NODE_ID,
                head=head,
                cost=reduced_cost_into(head, base),
                local_res=make_local(head, base),
                global_res=make_global(base),
            )
        )

        x1, y1 = coords(head)
        x2, y2 = coords(sink_node)
        base = instance.travel_cost(x1, y1, x2, y2)
        base_arc_cost[(head, sink_node)] = base
        pricing_instance.add_arc(
            Arc(
                tail=head,
                head=sink_node,
                cost=reduced_cost_into(sink_node, base),
                local_res=make_local(sink_node, base),
                global_res=make_global(base),
            )
        )

    for tail in customer_ids:
        for head in customer_ids:
            if tail == head:
                continue
            x1, y1 = coords(tail)
            x2, y2 = coords(head)
            base = instance.travel_cost(x1, y1, x2, y2)
            base_arc_cost[(tail, head)] = base
            pricing_instance.add_arc(
                Arc(
                    tail=tail,
                    head=head,
                    cost=reduced_cost_into(head, base),
                    local_res=make_local(head, base),
                    global_res=make_global(base),
                )
            )

    pairing_constant = instance.vehicle_fixed_cost - duals.min_open_facilities * 0.0

    return FacilityPricingGraph(
        pricing_instance=pricing_instance,
        source_node=SOURCE_NODE_ID,
        sink_node=sink_node,
        customer_by_id=customer_by_id,
        base_arc_cost=base_arc_cost,
        pairing_constant=pairing_constant,
    )


def actual_route_travel_cost(
    instance: LRSPInstance,
    facility: Facility,
    path: List[int],
    sink_node: int,
) -> float:
    """Compute the true (un-discounted) travel cost of a route path."""

    customer_by_id = {customer.id: customer for customer in instance.customers}

    def coords(node_id: int) -> Tuple[float, float]:
        if node_id == SOURCE_NODE_ID or node_id == sink_node:
            return facility.x, facility.y
        customer = customer_by_id[node_id]
        return customer.x, customer.y

    total = 0.0
    for tail, head in zip(path, path[1:]):
        x1, y1 = coords(tail)
        x2, y2 = coords(head)
        total += instance.travel_cost(x1, y1, x2, y2)
    return total
