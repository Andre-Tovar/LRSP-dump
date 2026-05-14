"""
MESPPRC instance generator.

Builds a deterministic, fully-connected MESPPRC test instance. The instance has:

- one source node and one sink node, both placed at the depot coordinate
- N customer nodes placed uniformly at random in a square coordinate box
- every directed arc except self-loops, so the graph is the complete digraph
  on (source, customers, sink)
- arc cost equal to euclidean distance
- two local (per-route) resources: route_time, bus_capacity
- one global (per-pairing) resource: duty_time

Per-arc resource model
======================

For arc (i, j) with euclidean distance d(i, j) and speed v:

    cost(i, j)         = d(i, j)
    route_time(i, j)   = d(i, j) / v + s(j)
    bus_capacity(i, j) = q(j)
    duty_time(i, j)    = d(i, j) / v + s(j)

where s(j) is the service time at the head node (zero for source / sink) and
q(j) is the demand at the head node (zero for source / sink). This is the
standard CVRP load model: bus_capacity is node-additive, while route_time and
duty_time are arc-additive.

Service times are deterministic per instance and equal across customers:

    s(j) = service_time_base + service_time_slope_per_customer * N

so larger instances naturally have longer service stops, exactly what the user
asked for.

Customer demands are sampled integer-uniform on [demand_min, demand_max].

Resource limits
===============

Let r_i = 2 * d(depot, i) / v + s be the cheapest one-customer round-trip
route_time (depot -> customer -> depot). Then:

    T_route   = beta_route * max_i r_i               (per-route time limit)
    Q         = max(max_i q_i,
                    round(rho_capacity * mean_i q_i)) (per-route load limit)
    T_duty    = gamma_duty * sum_i r_i               (per-pairing duty limit)

beta_route is the "max trip duration as a multiple of the worst solo trip":
    beta_route = 1  -> only singleton trips allowed (degenerate)
    beta_route = 2  -> typical 2-3 customer trips
    beta_route = 3+ -> long trips, geometry rarely binding

rho_capacity is the "approximate customers per trip by demand":
    rho_capacity = 3-5 -> moderate
    rho_capacity >= 10 -> capacity barely binding

gamma_duty is the "fraction of all-singletons duty that the pairing has":
    gamma_duty = 1.0 -> all-singletons solution is feasible (loose)
    gamma_duty = 0.7 -> must save 30% by merging customers into shared trips
    gamma_duty < 0.5 -> tight; instance may be infeasible if geometry isn't
                       cluster-friendly

The clamp Q = max(max_demand, ...) guarantees every customer can be served by
its own singleton trip, which is required for full coverage in the worst case.

The instance is feasible-by-construction when beta_route >= 1 and
gamma_duty * (max r_i / sum r_i) * N >= 1 / beta_route. Default values pass
this check on uniformly placed customers in [0, 100] up to N = 50.
"""

from __future__ import annotations

from dataclasses import dataclass
from math import sqrt
from random import Random
from statistics import fmean
from typing import Dict, List, Tuple

from .instance import Arc, MESPPRCInstance, Node, NodeType

Coordinate = Tuple[float, float]


@dataclass(frozen=True, slots=True)
class GeneratorConfig:
    """
    Configuration for the MESPPRC instance generator.

    Tightness knobs:

    - beta_route   : T_route   = beta_route   * max_i (2 d(depot, i) / v + s)
    - rho_capacity : Q         = ceil(rho_capacity * mean_i q_i),
                                 clamped up to max_i q_i
    - gamma_duty   : T_duty    = gamma_duty   * sum_i (2 d(depot, i) / v + s)

    Reproducibility:

    - seed fully determines the customer placement and demand draws. Two runs
      with the same `(num_customers, seed, ...)` produce the same instance.
    """

    num_customers: int

    coordinate_min: float = 0.0
    coordinate_max: float = 100.0
    depot_coordinate: Coordinate = (50.0, 50.0)

    demand_min: int = 1
    demand_max: int = 10

    service_time_base: float = 0.0
    service_time_slope_per_customer: float = 0.1

    vehicle_speed: float = 1.0

    beta_route: float = 2.0
    rho_capacity: float = 5.0
    gamma_duty: float = 0.8

    seed: int | None = None

    def __post_init__(self) -> None:
        if self.num_customers <= 0:
            raise ValueError("num_customers must be positive.")
        if self.coordinate_max <= self.coordinate_min:
            raise ValueError("coordinate_max must be greater than coordinate_min.")
        if not (
            self.coordinate_min <= self.depot_coordinate[0] <= self.coordinate_max
            and self.coordinate_min <= self.depot_coordinate[1] <= self.coordinate_max
        ):
            raise ValueError("depot_coordinate must lie inside the coordinate box.")
        if self.demand_min <= 0:
            raise ValueError("demand_min must be positive (CVRP convention).")
        if self.demand_max < self.demand_min:
            raise ValueError("demand_max must be at least demand_min.")
        if self.service_time_base < 0.0:
            raise ValueError("service_time_base must be non-negative.")
        if self.service_time_slope_per_customer < 0.0:
            raise ValueError("service_time_slope_per_customer must be non-negative.")
        if self.vehicle_speed <= 0.0:
            raise ValueError("vehicle_speed must be positive.")
        if self.beta_route < 1.0:
            raise ValueError(
                "beta_route must be >= 1.0; otherwise the worst customer's "
                "singleton trip is infeasible."
            )
        if self.rho_capacity <= 0.0:
            raise ValueError("rho_capacity must be positive.")
        if self.gamma_duty <= 0.0:
            raise ValueError("gamma_duty must be positive.")

    def service_time(self) -> float:
        """Per-customer service time used at every customer in this instance."""

        return self.service_time_base + self.service_time_slope_per_customer * self.num_customers


def generate_instance(config: GeneratorConfig) -> MESPPRCInstance:
    """
    Build a MESPPRC instance from the given configuration.

    Returns the validated `MESPPRCInstance` directly. Coordinates and demands
    are deterministic functions of `config`, so a downstream caller can rebuild
    them by re-running the generator with the same config.
    """

    rng = Random(config.seed)
    source_id = 0
    sink_id = config.num_customers + 1
    customer_ids = list(range(1, config.num_customers + 1))

    coordinates = _sample_coordinates(config, customer_ids, source_id, sink_id, rng)
    demands = _sample_demands(config, customer_ids, rng)
    service_time = config.service_time()

    distance_to_depot = {
        customer_id: _euclidean(coordinates[customer_id], coordinates[source_id])
        for customer_id in customer_ids
    }
    singleton_route_times = {
        customer_id: 2.0 * distance_to_depot[customer_id] / config.vehicle_speed
        + service_time
        for customer_id in customer_ids
    }

    local_limits = _compute_local_limits(
        config=config,
        demands=demands,
        singleton_route_times=singleton_route_times,
    )
    global_limits = _compute_global_limits(
        config=config,
        singleton_route_times=singleton_route_times,
    )

    instance = MESPPRCInstance(
        local_limits=local_limits,
        global_limits=global_limits,
    )
    instance.add_node(Node(source_id, NodeType.SOURCE))
    for customer_id in customer_ids:
        instance.add_node(Node(customer_id, NodeType.CUSTOMER))
    instance.add_node(Node(sink_id, NodeType.SINK))

    for tail_id, head_id in _all_directed_arcs(source_id, customer_ids, sink_id):
        cost, local_res, global_res = _compute_arc_attributes(
            tail_id=tail_id,
            head_id=head_id,
            tail_coordinate=coordinates[tail_id],
            head_coordinate=coordinates[head_id],
            head_demand=demands.get(head_id, 0),
            head_service_time=service_time if head_id in demands else 0.0,
            vehicle_speed=config.vehicle_speed,
        )
        instance.add_arc(
            Arc(
                tail=tail_id,
                head=head_id,
                cost=cost,
                local_res=local_res,
                global_res=global_res,
            )
        )

    instance.validate()
    _assert_instance_is_feasible_by_construction(
        config=config,
        instance=instance,
        singleton_route_times=singleton_route_times,
        demands=demands,
    )
    return instance


def _sample_coordinates(
    config: GeneratorConfig,
    customer_ids: List[int],
    source_id: int,
    sink_id: int,
    rng: Random,
) -> Dict[int, Coordinate]:
    coordinates: Dict[int, Coordinate] = {
        source_id: config.depot_coordinate,
        sink_id: config.depot_coordinate,
    }
    for customer_id in customer_ids:
        x = rng.uniform(config.coordinate_min, config.coordinate_max)
        y = rng.uniform(config.coordinate_min, config.coordinate_max)
        coordinates[customer_id] = (x, y)
    return coordinates


def _sample_demands(
    config: GeneratorConfig,
    customer_ids: List[int],
    rng: Random,
) -> Dict[int, int]:
    return {
        customer_id: rng.randint(config.demand_min, config.demand_max)
        for customer_id in customer_ids
    }


def _all_directed_arcs(
    source_id: int,
    customer_ids: List[int],
    sink_id: int,
) -> List[Tuple[int, int]]:
    """
    "Fully connected" in the MESPPRC sense:

    - every directed arc between distinct customers
    - source -> customer for every customer
    - customer -> sink for every customer

    The source has no incoming arcs and the sink has no outgoing arcs, which
    is what Phase 1's residual-reachability proofs assume. The source -> sink
    direct arc is intentionally omitted because a zero-customer route is not
    a meaningful column in MESPPRC.
    """

    arcs: List[Tuple[int, int]] = []
    for customer_id in customer_ids:
        arcs.append((source_id, customer_id))
        arcs.append((customer_id, sink_id))
    for tail in customer_ids:
        for head in customer_ids:
            if tail != head:
                arcs.append((tail, head))
    return arcs


def _compute_arc_attributes(
    *,
    tail_id: int,
    head_id: int,
    tail_coordinate: Coordinate,
    head_coordinate: Coordinate,
    head_demand: int,
    head_service_time: float,
    vehicle_speed: float,
) -> tuple[float, List[float], List[float]]:
    """
    Evaluate the arc-additive resource model.

    `head_demand` and `head_service_time` are zero when head is the source or
    sink, so depot-incident arcs carry no service time and no load.
    """

    del tail_id, head_id
    distance = _euclidean(tail_coordinate, head_coordinate)
    travel_time = distance / vehicle_speed
    route_time = travel_time + head_service_time
    duty_time = travel_time + head_service_time
    return distance, [route_time, float(head_demand)], [duty_time]


def _compute_local_limits(
    *,
    config: GeneratorConfig,
    demands: Dict[int, int],
    singleton_route_times: Dict[int, float],
) -> List[float]:
    if not singleton_route_times:
        raise ValueError("Cannot compute limits without customers.")

    max_singleton = max(singleton_route_times.values())
    route_time_limit = config.beta_route * max_singleton

    max_demand = max(demands.values())
    mean_demand = fmean(demands.values())
    capacity_limit = max(
        float(max_demand),
        float(round(config.rho_capacity * mean_demand)),
    )
    return [route_time_limit, capacity_limit]


def _compute_global_limits(
    *,
    config: GeneratorConfig,
    singleton_route_times: Dict[int, float],
) -> List[float]:
    total_singleton_duty = sum(singleton_route_times.values())
    return [config.gamma_duty * total_singleton_duty]


def _assert_instance_is_feasible_by_construction(
    *,
    config: GeneratorConfig,
    instance: MESPPRCInstance,
    singleton_route_times: Dict[int, float],
    demands: Dict[int, int],
) -> None:
    """
    Cheap structural check: every customer's singleton round-trip must fit
    within the per-route limits. This catches bad knob combinations early
    rather than waiting for Phase 2 to declare infeasibility.

    If the singletons all fit individually but the per-pairing duty limit
    can't even hold the cheapest singleton, that's also fatal and is reported
    here.
    """

    route_time_limit, capacity_limit = instance.local_limits
    duty_time_limit = instance.global_limits[0]

    for customer_id, route_time in singleton_route_times.items():
        if route_time > route_time_limit + 1e-9:
            raise ValueError(
                f"Customer {customer_id} cannot be served by a singleton trip: "
                f"route_time {route_time:.4f} exceeds T_route {route_time_limit:.4f}. "
                f"Increase beta_route (currently {config.beta_route})."
            )
        if demands[customer_id] > capacity_limit + 1e-9:
            raise ValueError(
                f"Customer {customer_id} cannot be served by a singleton trip: "
                f"demand {demands[customer_id]} exceeds Q {capacity_limit}. "
                f"Increase rho_capacity (currently {config.rho_capacity}) or "
                f"widen the demand range."
            )

    cheapest_singleton = min(singleton_route_times.values())
    if duty_time_limit < cheapest_singleton - 1e-9:
        raise ValueError(
            f"Duty-time limit {duty_time_limit:.4f} is smaller than the "
            f"cheapest customer's singleton duty {cheapest_singleton:.4f}. "
            f"Increase gamma_duty (currently {config.gamma_duty})."
        )


def _euclidean(a: Coordinate, b: Coordinate) -> float:
    return sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2)
