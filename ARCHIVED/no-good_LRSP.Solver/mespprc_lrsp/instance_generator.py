from __future__ import annotations

from dataclasses import dataclass
from heapq import heappop, heappush
from math import cos, inf, pi, sin, sqrt
from random import Random
from statistics import mean
from typing import Callable, Dict, Iterable, List, Mapping, Sequence

from .instance import Arc, MESPPRCInstance, Node, NodeType

Coordinate = tuple[float, float]
ArcKey = tuple[int, int]
ArcMetrics = tuple[float, List[float], List[float]]

_TIME_FACTORS: dict[tuple[str, str], float] = {
    ("local", "suburban"): 1.0,
    ("local", "city"): 1.2,
    ("arterial", "suburban"): 0.8,
    ("arterial", "city"): 1.0,
    ("depot_connector", "depot"): 1.0,
}

_BURDEN_FACTORS: dict[tuple[str, str], float] = {
    ("local", "suburban"): 1.0,
    ("local", "city"): 1.3,
    ("arterial", "suburban"): 0.8,
    ("arterial", "city"): 0.9,
    ("depot_connector", "depot"): 1.0,
}


@dataclass(frozen=True, slots=True)
class GeneratorConfig:
    """
    Configuration for the benchmark instance generator.

    The generator keeps all path values arc-additive:
    - `cost(i, j)` depends only on the chosen arc
    - `route_time(i, j)` depends only on the chosen arc
    - `bus_capacity(i, j)` depends only on the chosen arc
    - `duty_time(i, j) = route_time(i, j)`

    There are no node-based penalties, service times, turn penalties, or hidden
    post-route adjustments.

    In `mespprc_lrsp`, this generator is best interpreted as a single-facility LRSP
    testbed: the current depot/source/sink abstraction stands in for one facility, and
    the generated routes are candidates for single-vehicle pairing generation.
    """

    num_customers: int
    source_sink_colocated: bool = True

    layout_type: str = "urban_suburban_mixture"
    city_center_share: float = 0.35
    city_center_radius: float = 4.0
    suburban_outer_radius: float = 12.0
    depot_coordinate: Coordinate = (0.0, 0.0)

    topology_type: str = "hybrid_local_plus_arterial"
    target_avg_out_degree: int = 5
    long_arc_probability: float = 0.08
    mutual_arc_probability: float = 0.75
    ensure_source_to_customer_reachability: bool = True
    ensure_customer_to_sink_reachability: bool = True

    cost_base: str = "euclidean_distance"
    cost_distance_multiplier: float = 1.0

    num_local_resources: int = 2
    local_resource_names: tuple[str, str] = ("route_time", "bus_capacity")

    num_global_resources: int = 1
    global_resource_names: tuple[str] = ("duty_time",)

    local_tightness: float = 0.55
    global_tightness: float = 0.70

    seed: int | None = None
    replicate_id: int | None = None

    def __post_init__(self) -> None:
        if self.num_customers <= 0:
            raise ValueError("num_customers must be positive.")
        if not self.source_sink_colocated:
            raise ValueError(
                "This generator currently requires source and sink to be co-located."
            )
        if self.layout_type != "urban_suburban_mixture":
            raise ValueError(
                "This generator currently supports layout_type='urban_suburban_mixture' only."
            )
        if self.topology_type != "hybrid_local_plus_arterial":
            raise ValueError(
                "This generator currently supports topology_type='hybrid_local_plus_arterial' only."
            )
        if self.cost_base != "euclidean_distance":
            raise ValueError(
                "This generator currently supports cost_base='euclidean_distance' only."
            )
        if not self.ensure_source_to_customer_reachability:
            raise ValueError(
                "This generator version requires ensure_source_to_customer_reachability=True."
            )
        if not self.ensure_customer_to_sink_reachability:
            raise ValueError(
                "This generator version requires ensure_customer_to_sink_reachability=True."
            )
        if not 0.0 <= self.city_center_share <= 1.0:
            raise ValueError("city_center_share must lie in [0, 1].")
        if self.city_center_radius <= 0.0:
            raise ValueError("city_center_radius must be positive.")
        if self.suburban_outer_radius <= self.city_center_radius:
            raise ValueError(
                "suburban_outer_radius must be larger than city_center_radius."
            )
        if self.target_avg_out_degree < 1:
            raise ValueError("target_avg_out_degree must be at least 1.")
        if not 0.0 <= self.long_arc_probability <= 1.0:
            raise ValueError("long_arc_probability must lie in [0, 1].")
        if not 0.0 <= self.mutual_arc_probability <= 1.0:
            raise ValueError("mutual_arc_probability must lie in [0, 1].")
        if self.cost_distance_multiplier <= 0.0:
            raise ValueError("cost_distance_multiplier must be positive.")
        if self.num_local_resources != 2:
            raise ValueError("This generator requires exactly 2 local resources.")
        if tuple(self.local_resource_names) != ("route_time", "bus_capacity"):
            raise ValueError(
                "This generator expects local_resource_names=('route_time', 'bus_capacity')."
            )
        if self.num_global_resources != 1:
            raise ValueError("This generator requires exactly 1 global resource.")
        if tuple(self.global_resource_names) != ("duty_time",):
            raise ValueError(
                "This generator expects global_resource_names=('duty_time',)."
            )
        if not 0.0 < self.local_tightness <= 1.0:
            raise ValueError("local_tightness must lie in (0, 1].")
        if not 0.0 < self.global_tightness <= 1.0:
            raise ValueError("global_tightness must lie in (0, 1].")

    def resolved_seed(self) -> int | None:
        if self.seed is None and self.replicate_id is None:
            return None
        seed = 0 if self.seed is None else self.seed
        replicate = 0 if self.replicate_id is None else self.replicate_id
        return (seed * 1_000_003 + replicate * 97_409) % (2**32)


@dataclass(frozen=True, slots=True)
class CalibrationConfig:
    """
    Controls the untimed calibration metadata.

    The LRSP pricing package does not calibrate around full-demand coverage. This config
    is kept only for API compatibility with related packages.
    """

    max_iterations: int = 15
    max_topology_iterations: int = 8
    local_relaxation_factor: float = 1.10
    global_relaxation_factor: float = 1.10

    def __post_init__(self) -> None:
        if self.max_iterations <= 0:
            raise ValueError("max_iterations must be positive.")
        if self.max_topology_iterations < 0:
            raise ValueError("max_topology_iterations must be non-negative.")
        if self.local_relaxation_factor <= 1.0:
            raise ValueError("local_relaxation_factor must be greater than 1.0.")
        if self.global_relaxation_factor <= 1.0:
            raise ValueError("global_relaxation_factor must be greater than 1.0.")


@dataclass(frozen=True, slots=True)
class CalibrationIteration:
    iteration: int
    local_limits: tuple[float, ...]
    global_limits: tuple[float, ...]
    phase1_route_count: int
    covered_customers: tuple[int, ...]
    local_support_feasible: bool
    relaxed_phase2_feasible: bool
    constrained_phase2_feasible: bool
    adjustment: str


@dataclass(frozen=True, slots=True)
class CalibrationReport:
    """
    Summary of the untimed calibration metadata.

    In the LRSP pricing package, generation is not calibrated around full-demand
    coverage, so this report is present only for structural compatibility.
    """

    converged: bool
    iterations: int
    topology_adjustments: int
    local_adjustments: int
    global_adjustments: int
    initial_local_limits: tuple[float, ...]
    initial_global_limits: tuple[float, ...]
    final_local_limits: tuple[float, ...]
    final_global_limits: tuple[float, ...]
    phase2_feasibility_solver: str
    history: tuple[CalibrationIteration, ...]


@dataclass(frozen=True, slots=True)
class GeneratedBenchmarkInstance:
    """
    Bundle returned by the benchmark generator.

    For LRSP pricing experiments, interpret the generated depot as the current single
    facility and use the instance as a route-generation/pairing-generation input rather
    than as a full-demand benchmark.
    """

    instance: MESPPRCInstance
    config: GeneratorConfig
    coordinates: Dict[int, Coordinate]
    node_zones: Dict[int, str]
    arc_kinds: Dict[ArcKey, str]
    local_resource_names: tuple[str, ...]
    global_resource_names: tuple[str, ...]
    source_id: int
    sink_id: int
    calibration_report: CalibrationReport | None = None


def generate_instance(config: GeneratorConfig) -> GeneratedBenchmarkInstance:
    rng = Random(config.resolved_seed())
    source_id = 0
    sink_id = config.num_customers + 1
    customer_ids = list(range(1, config.num_customers + 1))

    customer_coordinates, customer_zones = generate_customer_coordinates(config, rng)

    coordinates: Dict[int, Coordinate] = {
        source_id: config.depot_coordinate,
        sink_id: config.depot_coordinate,
    }
    coordinates.update(customer_coordinates)

    node_zones: Dict[int, str] = {
        source_id: "depot",
        sink_id: "depot",
    }
    node_zones.update(customer_zones)

    arc_kinds = generate_arc_set(
        config=config,
        customer_ids=customer_ids,
        coordinates=coordinates,
        node_zones=node_zones,
        source_id=source_id,
        sink_id=sink_id,
        rng=rng,
    )
    return _assemble_generated_instance(
        config=config,
        coordinates=coordinates,
        node_zones=node_zones,
        arc_kinds=arc_kinds,
        source_id=source_id,
        sink_id=sink_id,
        calibration_report=None,
    )


def calibrate_instance(
    generated: GeneratedBenchmarkInstance | GeneratorConfig,
    calibration_config: CalibrationConfig | None = None,
) -> GeneratedBenchmarkInstance:
    """
    Return the generated instance unchanged.

    The `mespprc_lrsp` package is intended to plug into an upstream LRSP workflow and is
    not calibrated around full-demand coverage. The function is kept only for package
    parity with related packages.
    """
    del calibration_config
    if isinstance(generated, GeneratedBenchmarkInstance):
        return generated
    return generate_instance(generated)


def generate_benchmark_instance(
    config: GeneratorConfig,
    *,
    calibrate: bool = False,
    calibration_config: CalibrationConfig | None = None,
) -> GeneratedBenchmarkInstance:
    del calibrate
    del calibration_config
    return generate_instance(config)


def generate_customer_coordinates(
    config: GeneratorConfig,
    rng: Random,
) -> tuple[Dict[int, Coordinate], Dict[int, str]]:
    depot_x, depot_y = config.depot_coordinate
    city_count = int(round(config.num_customers * config.city_center_share))
    if 0 < city_count < config.num_customers:
        pass
    elif config.num_customers == 1:
        city_count = 1
    else:
        city_count = min(max(city_count, 1), config.num_customers - 1)
    suburban_count = config.num_customers - city_count

    coordinates: Dict[int, Coordinate] = {}
    zones: Dict[int, str] = {}

    customer_id = 1
    for _ in range(city_count):
        radius = config.city_center_radius * sqrt(rng.random())
        angle = 2.0 * pi * rng.random()
        coordinates[customer_id] = (
            depot_x + radius * cos(angle),
            depot_y + radius * sin(angle),
        )
        zones[customer_id] = "city_center"
        customer_id += 1

    if suburban_count > 0:
        cluster_count = max(3, min(7, int(round(sqrt(suburban_count)))))
        cluster_centers = _generate_suburban_cluster_centers(config, cluster_count, rng)
        suburban_band = config.suburban_outer_radius - config.city_center_radius
        jitter_scale = max(0.6, 0.16 * suburban_band)

        for _ in range(suburban_count):
            base_x, base_y = cluster_centers[rng.randrange(len(cluster_centers))]
            sample_x = base_x + rng.gauss(0.0, jitter_scale)
            sample_y = base_y + rng.gauss(0.0, jitter_scale)
            sample_x, sample_y = _project_to_annulus(
                point=(sample_x, sample_y),
                center=config.depot_coordinate,
                inner_radius=config.city_center_radius * 1.05,
                outer_radius=config.suburban_outer_radius,
            )
            coordinates[customer_id] = (sample_x, sample_y)
            zones[customer_id] = "suburban"
            customer_id += 1

    return coordinates, zones


def generate_arc_set(
    *,
    config: GeneratorConfig,
    customer_ids: Sequence[int],
    coordinates: Mapping[int, Coordinate],
    node_zones: Mapping[int, str],
    source_id: int,
    sink_id: int,
    rng: Random,
    topology_boost_level: int = 0,
) -> Dict[ArcKey, str]:
    arc_kinds: Dict[ArcKey, str] = {}
    customers = list(customer_ids)
    local_degree = min(
        max(2, config.target_avg_out_degree + topology_boost_level),
        max(len(customers) - 1, 0),
    )
    effective_long_arc_probability = min(
        1.0,
        config.long_arc_probability + 0.05 * topology_boost_level,
    )
    effective_mutual_arc_probability = min(
        1.0,
        config.mutual_arc_probability + 0.04 * topology_boost_level,
    )

    distance_lookup = {
        (i, j): euclidean_distance(coordinates[i], coordinates[j])
        for i in customers
        for j in customers
        if i != j
    }
    distance_to_depot = {
        customer_id: euclidean_distance(coordinates[customer_id], coordinates[source_id])
        for customer_id in customers
    }

    access_hub_ids = _select_depot_access_hubs(
        customer_ids=customers,
        distance_to_depot=distance_to_depot,
        topology_boost_level=topology_boost_level,
    )
    exit_hub_ids = _select_depot_exit_hubs(
        customer_ids=customers,
        distance_to_depot=distance_to_depot,
        topology_boost_level=topology_boost_level,
    )
    _add_depot_access_arcs(
        arc_kinds=arc_kinds,
        source_id=source_id,
        sink_id=sink_id,
        access_hub_ids=access_hub_ids,
        exit_hub_ids=exit_hub_ids,
    )
    _add_reachability_backbone_arcs(
        arc_kinds=arc_kinds,
        customer_ids=customers,
        distance_to_depot=distance_to_depot,
        distance_lookup=distance_lookup,
        access_hub_ids=access_hub_ids,
        exit_hub_ids=exit_hub_ids,
    )

    for customer_id in customers:
        ordered_neighbors = sorted(
            (other for other in customers if other != customer_id),
            key=lambda other: (distance_lookup[(customer_id, other)], other),
        )
        for neighbor_id in ordered_neighbors[:local_degree]:
            _add_arc_kind(arc_kinds, customer_id, neighbor_id, "local")
            if rng.random() < effective_mutual_arc_probability:
                _add_arc_kind(arc_kinds, neighbor_id, customer_id, "local")

    central_hubs, suburban_hubs = _select_arterial_hubs(
        customer_ids=customers,
        coordinates=coordinates,
        node_zones=node_zones,
        depot_coordinate=config.depot_coordinate,
        topology_boost_level=topology_boost_level,
    )
    arterial_target_count = max(1, 1 + topology_boost_level // 2)
    for customer_id in customers:
        if rng.random() >= effective_long_arc_probability:
            continue

        zone = node_zones[customer_id]
        if zone == "suburban" and central_hubs:
            candidate_targets = sorted(
                (hub_id for hub_id in central_hubs if hub_id != customer_id),
                key=lambda hub_id: (
                    distance_lookup.get((customer_id, hub_id), 0.0),
                    hub_id,
                ),
            )
        else:
            candidate_targets = sorted(
                (hub_id for hub_id in suburban_hubs if hub_id != customer_id),
                key=lambda hub_id: (
                    distance_lookup.get((customer_id, hub_id), 0.0),
                    hub_id,
                ),
            )

        if not candidate_targets:
            continue

        for target_id in candidate_targets[:arterial_target_count]:
            _add_arc_kind(arc_kinds, customer_id, target_id, "arterial")
            if rng.random() < effective_mutual_arc_probability:
                _add_arc_kind(arc_kinds, target_id, customer_id, "arterial")

    return arc_kinds


def compute_arc_attributes(
    *,
    config: GeneratorConfig,
    tail_id: int,
    head_id: int,
    tail_coordinate: Coordinate,
    head_coordinate: Coordinate,
    tail_zone: str,
    head_zone: str,
    arc_kind: str,
) -> tuple[float, List[float], List[float]]:
    """
    Compute purely arc-based cost and resource vectors.

    `duty_time` is deliberately equal to `route_time` on every arc, so the global
    cross-route duty-time limit is exactly the sum of route-time consumption across the
    selected routes.
    """

    del tail_id, head_id
    distance = euclidean_distance(tail_coordinate, head_coordinate)
    zone_category = _zone_category_for_arc(
        arc_kind=arc_kind,
        tail_zone=tail_zone,
        head_zone=head_zone,
    )

    cost = distance * config.cost_distance_multiplier
    route_time = distance * _TIME_FACTORS[(arc_kind, zone_category)]
    bus_capacity = distance * _BURDEN_FACTORS[(arc_kind, zone_category)]
    duty_time = route_time

    return cost, [route_time, bus_capacity], [duty_time]


def build_instance(
    *,
    config: GeneratorConfig,
    coordinates: Mapping[int, Coordinate],
    node_zones: Mapping[int, str],
    arc_kinds: Mapping[ArcKey, str],
    source_id: int,
    sink_id: int,
    local_limits_override: Sequence[float] | None = None,
    global_limits_override: Sequence[float] | None = None,
) -> MESPPRCInstance:
    arc_metrics = compute_arc_metrics(
        config=config,
        coordinates=coordinates,
        node_zones=node_zones,
        arc_kinds=arc_kinds,
    )

    if local_limits_override is None or global_limits_override is None:
        local_limits, global_limits = derive_resource_limits(
            config=config,
            coordinates=coordinates,
            arc_metrics=arc_metrics,
            source_id=source_id,
            sink_id=sink_id,
        )
    else:
        local_limits = list(local_limits_override)
        global_limits = list(global_limits_override)

    instance = MESPPRCInstance(
        local_limits=local_limits,
        global_limits=global_limits,
    )

    instance.add_node(Node(source_id, NodeType.SOURCE))
    for customer_id in sorted(
        node_id for node_id in coordinates if node_id not in {source_id, sink_id}
    ):
        instance.add_node(Node(customer_id, NodeType.CUSTOMER))
    instance.add_node(Node(sink_id, NodeType.SINK))

    for (tail_id, head_id), (cost, local_res, global_res) in sorted(arc_metrics.items()):
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
    return instance


def compute_arc_metrics(
    *,
    config: GeneratorConfig,
    coordinates: Mapping[int, Coordinate],
    node_zones: Mapping[int, str],
    arc_kinds: Mapping[ArcKey, str],
) -> Dict[ArcKey, ArcMetrics]:
    arc_metrics: Dict[ArcKey, ArcMetrics] = {}
    for (tail_id, head_id), arc_kind in sorted(arc_kinds.items()):
        arc_metrics[(tail_id, head_id)] = compute_arc_attributes(
            config=config,
            tail_id=tail_id,
            head_id=head_id,
            tail_coordinate=coordinates[tail_id],
            head_coordinate=coordinates[head_id],
            tail_zone=node_zones[tail_id],
            head_zone=node_zones[head_id],
            arc_kind=arc_kind,
        )
    return arc_metrics


def derive_resource_limits(
    *,
    config: GeneratorConfig,
    coordinates: Mapping[int, Coordinate],
    arc_metrics: Mapping[ArcKey, ArcMetrics],
    source_id: int,
    sink_id: int,
) -> tuple[List[float], List[float]]:
    customer_ids = sorted(
        node_id for node_id in coordinates if node_id not in {source_id, sink_id}
    )
    node_ids = sorted(coordinates)

    route_time_from_source = _resource_shortest_paths(
        start_node=source_id,
        node_ids=node_ids,
        arc_metrics=arc_metrics,
        value_getter=lambda metrics: metrics[1][0],
    )
    route_time_to_sink = _resource_shortest_paths(
        start_node=sink_id,
        node_ids=node_ids,
        arc_metrics=arc_metrics,
        value_getter=lambda metrics: metrics[1][0],
        reverse=True,
    )
    bus_capacity_from_source = _resource_shortest_paths(
        start_node=source_id,
        node_ids=node_ids,
        arc_metrics=arc_metrics,
        value_getter=lambda metrics: metrics[1][1],
    )
    bus_capacity_to_sink = _resource_shortest_paths(
        start_node=sink_id,
        node_ids=node_ids,
        arc_metrics=arc_metrics,
        value_getter=lambda metrics: metrics[1][1],
        reverse=True,
    )

    shortest_route_time_roundtrips: List[float] = []
    shortest_bus_capacity_roundtrips: List[float] = []
    for customer_id in customer_ids:
        source_route_time = route_time_from_source[customer_id]
        sink_route_time = route_time_to_sink[customer_id]
        source_bus_capacity = bus_capacity_from_source[customer_id]
        sink_bus_capacity = bus_capacity_to_sink[customer_id]
        if any(
            value == inf
            for value in (
                source_route_time,
                sink_route_time,
                source_bus_capacity,
                sink_bus_capacity,
            )
        ):
            raise ValueError(
                "Generated graph failed to provide source/customer/sink reachability "
                "needed for resource-limit derivation."
            )

        shortest_route_time_roundtrips.append(source_route_time + sink_route_time)
        shortest_bus_capacity_roundtrips.append(source_bus_capacity + sink_bus_capacity)

    route_time_hop = _minimum_customer_hop_values(
        customer_ids=customer_ids,
        arc_metrics=arc_metrics,
        resource_index=0,
    )
    bus_capacity_hop = _minimum_customer_hop_values(
        customer_ids=customer_ids,
        arc_metrics=arc_metrics,
        resource_index=1,
    )

    local_slack = 1.0 - config.local_tightness
    route_time_limit = max(shortest_route_time_roundtrips) + local_slack * (
        mean(shortest_route_time_roundtrips) + 2.0 * route_time_hop
    )
    bus_capacity_limit = max(shortest_bus_capacity_roundtrips) + local_slack * (
        mean(shortest_bus_capacity_roundtrips) + 2.0 * bus_capacity_hop
    )

    direct_cover_duty = sum(shortest_route_time_roundtrips)
    global_slack_multiplier = 1.0 + 0.30 * (1.0 - config.global_tightness)
    duty_time_limit = direct_cover_duty * global_slack_multiplier

    return [route_time_limit, bus_capacity_limit], [duty_time_limit]


def euclidean_distance(a: Coordinate, b: Coordinate) -> float:
    dx = a[0] - b[0]
    dy = a[1] - b[1]
    return sqrt(dx * dx + dy * dy)


def _phase2_feasibility_checker():
    try:
        from .phase2_ip import Phase2IPSolver

        def _check(instance: MESPPRCInstance, routes: Sequence[object], *, ignore_global_limits: bool) -> bool:
            return _solve_phase2_exact_cover(
                instance=instance,
                routes=routes,
                solver_factory=Phase2IPSolver,
                ignore_global_limits=ignore_global_limits,
            )

        return "ip", _check
    except ModuleNotFoundError:
        from .phase2_dp import Phase2DPSolver

        def _check(instance: MESPPRCInstance, routes: Sequence[object], *, ignore_global_limits: bool) -> bool:
            return _solve_phase2_exact_cover(
                instance=instance,
                routes=routes,
                solver_factory=Phase2DPSolver,
                ignore_global_limits=ignore_global_limits,
            )

        return "dp", _check


def _solve_phase1(instance: MESPPRCInstance):
    from .phase1 import Phase1Solver

    return Phase1Solver(instance).solve()


def _solve_phase2_exact_cover(
    *,
    instance: MESPPRCInstance,
    routes: Sequence[object],
    solver_factory,
    ignore_global_limits: bool,
) -> bool:
    current_local_limits = list(instance.local_limits)
    current_global_limits = list(instance.global_limits)
    if ignore_global_limits:
        relaxed_global_limits = _relaxed_global_limits(routes, current_global_limits)
        instance.set_resource_limits(
            local_limits=current_local_limits,
            global_limits=relaxed_global_limits,
        )

    try:
        solver = solver_factory(instance)
        if solver_factory.__name__ == "Phase2IPSolver":
            result = solver.solve(routes, collect_diagnostics=False)
        else:
            result = solver.solve(routes)
    finally:
        if ignore_global_limits:
            instance.set_resource_limits(
                local_limits=current_local_limits,
                global_limits=current_global_limits,
            )

    return bool(result.is_feasible and result.coverage_complete)


def _relaxed_global_limits(
    routes: Sequence[object],
    current_global_limits: Sequence[float],
) -> List[float]:
    if not current_global_limits:
        return []

    relaxed = [0.0] * len(current_global_limits)
    for route in routes:
        for dim, value in enumerate(getattr(route, "global_resources", [])):
            relaxed[dim] += float(value)
    return [
        max(current_limit, total + 1.0)
        for current_limit, total in zip(current_global_limits, relaxed)
    ]


def _covered_customers_from_routes(routes: Sequence[object]) -> set[int]:
    covered: set[int] = set()
    for route in routes:
        covered.update(set(getattr(route, "covered_customers", set())))
    return covered


def _topology_calibrate_base_instance(
    *,
    base: GeneratedBenchmarkInstance,
    calibration: CalibrationConfig,
    phase2_feasible,
) -> tuple[GeneratedBenchmarkInstance, int]:
    required_customers = set(base.instance.required_customers())
    customer_ids = base.instance.customers()
    last_candidate = base

    for topology_boost_level in range(calibration.max_topology_iterations + 1):
        if topology_boost_level == 0:
            arc_kinds = dict(base.arc_kinds)
        else:
            arc_kinds = _regenerate_arc_set_with_topology_boost(
                base=base,
                customer_ids=customer_ids,
                topology_boost_level=topology_boost_level,
            )

        loose_local_limits, loose_global_limits = _very_loose_limits(
            config=base.config,
            coordinates=base.coordinates,
            node_zones=base.node_zones,
            arc_kinds=arc_kinds,
            source_id=base.source_id,
            sink_id=base.sink_id,
        )
        candidate = _assemble_generated_instance(
            config=base.config,
            coordinates=base.coordinates,
            node_zones=base.node_zones,
            arc_kinds=arc_kinds,
            source_id=base.source_id,
            sink_id=base.sink_id,
            local_limits_override=loose_local_limits,
            global_limits_override=loose_global_limits,
            calibration_report=None,
        )
        last_candidate = candidate

        phase1_result = _solve_phase1(candidate.instance)
        if not required_customers.issubset(
            _covered_customers_from_routes(phase1_result.feasible_routes)
        ):
            continue
        if not phase2_feasible(
            candidate.instance,
            phase1_result.feasible_routes,
            ignore_global_limits=True,
        ):
            continue

        calibrated = _assemble_generated_instance(
            config=base.config,
            coordinates=base.coordinates,
            node_zones=base.node_zones,
            arc_kinds=arc_kinds,
            source_id=base.source_id,
            sink_id=base.sink_id,
            calibration_report=None,
        )
        return calibrated, topology_boost_level

    raise RuntimeError(
        "Topology calibration failed to find a loose-limit full-cover graph. "
        f"Maximum topology boost level tried: {calibration.max_topology_iterations}. "
        f"Last attempted graph had {len(last_candidate.arc_kinds)} directed arcs."
    )


def _regenerate_arc_set_with_topology_boost(
    *,
    base: GeneratedBenchmarkInstance,
    customer_ids: Sequence[int],
    topology_boost_level: int,
) -> Dict[ArcKey, str]:
    boost_seed = _topology_boost_seed(base.config, topology_boost_level)
    rng = Random(boost_seed)
    return generate_arc_set(
        config=base.config,
        customer_ids=customer_ids,
        coordinates=base.coordinates,
        node_zones=base.node_zones,
        source_id=base.source_id,
        sink_id=base.sink_id,
        rng=rng,
        topology_boost_level=topology_boost_level,
    )


def _topology_boost_seed(config: GeneratorConfig, topology_boost_level: int) -> int:
    base_seed = config.resolved_seed()
    resolved = 0 if base_seed is None else base_seed
    return (resolved + 1_315_423 * topology_boost_level) % (2**32)


def _very_loose_limits(
    *,
    config: GeneratorConfig,
    coordinates: Mapping[int, Coordinate],
    node_zones: Mapping[int, str],
    arc_kinds: Mapping[ArcKey, str],
    source_id: int,
    sink_id: int,
) -> tuple[List[float], List[float]]:
    arc_metrics = compute_arc_metrics(
        config=config,
        coordinates=coordinates,
        node_zones=node_zones,
        arc_kinds=arc_kinds,
    )
    customer_count = max(
        1,
        len([node_id for node_id in coordinates if node_id not in {source_id, sink_id}]),
    )
    derived_local_limits, derived_global_limits = derive_resource_limits(
        config=config,
        coordinates=coordinates,
        arc_metrics=arc_metrics,
        source_id=source_id,
        sink_id=sink_id,
    )
    local_scale = max(3.0, 1.5 + sqrt(float(customer_count)))
    global_scale = max(4.0, 1.0 + float(customer_count))
    return (
        [limit * local_scale for limit in derived_local_limits],
        [limit * global_scale for limit in derived_global_limits],
    )


def _relax_limits_exponentially(
    limits: Sequence[float],
    base_factor: float,
    relaxation_step: int,
    problem_size: int,
) -> List[float]:
    """
    Loosen limits with a size-aware escalating schedule.

    The first failed attempt multiplies by `base_factor ** 1`, the second by
    `base_factor ** 2`, and so on. The exponent is also scaled by `sqrt(problem_size)`,
    so small instances still tune fairly gently while larger instances loosen much
    faster when they need more room to become benchmark-feasible.
    """
    size_scale = max(1.0, sqrt(float(problem_size)))
    scaled_exponent = relaxation_step * size_scale
    return [limit * (base_factor**scaled_exponent) for limit in limits]


def _assemble_generated_instance(
    *,
    config: GeneratorConfig,
    coordinates: Mapping[int, Coordinate],
    node_zones: Mapping[int, str],
    arc_kinds: Mapping[ArcKey, str],
    source_id: int,
    sink_id: int,
    local_limits_override: Sequence[float] | None = None,
    global_limits_override: Sequence[float] | None = None,
    calibration_report: CalibrationReport | None,
) -> GeneratedBenchmarkInstance:
    instance = build_instance(
        config=config,
        coordinates=coordinates,
        node_zones=node_zones,
        arc_kinds=arc_kinds,
        source_id=source_id,
        sink_id=sink_id,
        local_limits_override=local_limits_override,
        global_limits_override=global_limits_override,
    )
    return GeneratedBenchmarkInstance(
        instance=instance,
        config=config,
        coordinates=dict(coordinates),
        node_zones=dict(node_zones),
        arc_kinds=dict(arc_kinds),
        local_resource_names=config.local_resource_names,
        global_resource_names=config.global_resource_names,
        source_id=source_id,
        sink_id=sink_id,
        calibration_report=calibration_report,
    )


def _generate_suburban_cluster_centers(
    config: GeneratorConfig,
    cluster_count: int,
    rng: Random,
) -> List[Coordinate]:
    centers: List[Coordinate] = []
    depot_x, depot_y = config.depot_coordinate
    inner_radius = config.city_center_radius * 1.35
    for cluster_idx in range(cluster_count):
        base_angle = 2.0 * pi * cluster_idx / cluster_count
        angle = base_angle + rng.uniform(-0.35, 0.35)
        radius = rng.uniform(inner_radius, config.suburban_outer_radius * 0.9)
        centers.append(
            (
                depot_x + radius * cos(angle),
                depot_y + radius * sin(angle),
            )
        )
    return centers


def _project_to_annulus(
    *,
    point: Coordinate,
    center: Coordinate,
    inner_radius: float,
    outer_radius: float,
) -> Coordinate:
    dx = point[0] - center[0]
    dy = point[1] - center[1]
    radius = sqrt(dx * dx + dy * dy)
    if radius == 0.0:
        return (center[0] + inner_radius, center[1])
    if inner_radius <= radius <= outer_radius:
        return point

    clamped_radius = min(max(radius, inner_radius), outer_radius)
    scale = clamped_radius / radius
    return (center[0] + dx * scale, center[1] + dy * scale)


def _select_arterial_hubs(
    *,
    customer_ids: Sequence[int],
    coordinates: Mapping[int, Coordinate],
    node_zones: Mapping[int, str],
    depot_coordinate: Coordinate,
    topology_boost_level: int = 0,
) -> tuple[List[int], List[int]]:
    sorted_by_depot = sorted(
        customer_ids,
        key=lambda customer_id: (
            euclidean_distance(coordinates[customer_id], depot_coordinate),
            customer_id,
        ),
    )
    city_customers = [
        customer_id
        for customer_id in sorted_by_depot
        if node_zones[customer_id] == "city_center"
    ]
    suburban_customers = [
        customer_id
        for customer_id in reversed(sorted_by_depot)
        if node_zones[customer_id] == "suburban"
    ]

    central_hub_count = max(
        1,
        min(
            len(city_customers) if city_customers else 1,
            int(round(sqrt(max(len(city_customers), 1)))) + topology_boost_level,
        ),
    )
    suburban_hub_count = max(
        1,
        min(
            len(suburban_customers) if suburban_customers else 1,
            int(round(sqrt(max(len(suburban_customers), 1)))) + topology_boost_level,
        ),
    )
    return city_customers[:central_hub_count], suburban_customers[:suburban_hub_count]


def _select_depot_access_hubs(
    *,
    customer_ids: Sequence[int],
    distance_to_depot: Mapping[int, float],
    topology_boost_level: int = 0,
) -> List[int]:
    ordered = sorted(
        customer_ids,
        key=lambda customer_id: (distance_to_depot[customer_id], customer_id),
    )
    if len(ordered) <= 2:
        return ordered

    base_hub_count = max(3, min(6, int(round(sqrt(len(ordered)))) + 1))
    hub_count = min(len(ordered), base_hub_count + 2 * topology_boost_level)
    inner_count = max(1, hub_count // 2)
    outer_count = hub_count - inner_count
    selected = ordered[:inner_count] + ordered[-outer_count:]
    return sorted(
        set(selected),
        key=lambda customer_id: (distance_to_depot[customer_id], customer_id),
    )


def _select_depot_exit_hubs(
    *,
    customer_ids: Sequence[int],
    distance_to_depot: Mapping[int, float],
    topology_boost_level: int = 0,
) -> List[int]:
    return _select_depot_access_hubs(
        customer_ids=customer_ids,
        distance_to_depot=distance_to_depot,
        topology_boost_level=topology_boost_level,
    )


def _add_depot_access_arcs(
    *,
    arc_kinds: Dict[ArcKey, str],
    source_id: int,
    sink_id: int,
    access_hub_ids: Sequence[int],
    exit_hub_ids: Sequence[int],
) -> None:
    for customer_id in access_hub_ids:
        _add_arc_kind(arc_kinds, source_id, customer_id, "depot_connector")
    for customer_id in exit_hub_ids:
        _add_arc_kind(arc_kinds, customer_id, sink_id, "depot_connector")


def _add_reachability_backbone_arcs(
    *,
    arc_kinds: Dict[ArcKey, str],
    customer_ids: Sequence[int],
    distance_to_depot: Mapping[int, float],
    distance_lookup: Mapping[ArcKey, float],
    access_hub_ids: Sequence[int],
    exit_hub_ids: Sequence[int],
) -> None:
    access_hub_set = set(access_hub_ids)
    exit_hub_set = set(exit_hub_ids)
    ordered_outward = sorted(
        customer_ids,
        key=lambda customer_id: (distance_to_depot[customer_id], customer_id),
    )

    earlier_customers: List[int] = []
    for customer_id in ordered_outward:
        if customer_id not in access_hub_set and earlier_customers:
            predecessor_id = min(
                earlier_customers,
                key=lambda other_id: (distance_lookup[(other_id, customer_id)], other_id),
            )
            _add_arc_kind(arc_kinds, predecessor_id, customer_id, "local")
        earlier_customers.append(customer_id)

    closer_customers: List[int] = []
    for customer_id in ordered_outward:
        if customer_id not in exit_hub_set and closer_customers:
            successor_id = min(
                closer_customers,
                key=lambda other_id: (distance_lookup[(customer_id, other_id)], other_id),
            )
            _add_arc_kind(arc_kinds, customer_id, successor_id, "local")
        closer_customers.append(customer_id)


def _minimum_customer_hop_values(
    *,
    customer_ids: Iterable[int],
    arc_metrics: Mapping[ArcKey, ArcMetrics],
    resource_index: int,
) -> float:
    minima: List[float] = []
    customer_set = set(customer_ids)
    for customer_id in customer_set:
        outgoing_values = [
            local_res[resource_index]
            for (tail_id, head_id), (_, local_res, _) in arc_metrics.items()
            if tail_id == customer_id and head_id in customer_set
        ]
        if outgoing_values:
            minima.append(min(outgoing_values))

    if not minima:
        return 0.0
    return mean(minima)


def _resource_shortest_paths(
    *,
    start_node: int,
    node_ids: Sequence[int],
    arc_metrics: Mapping[ArcKey, ArcMetrics],
    value_getter: Callable[[ArcMetrics], float],
    reverse: bool = False,
) -> Dict[int, float]:
    adjacency: Dict[int, List[tuple[int, float]]] = {node_id: [] for node_id in node_ids}
    for (tail_id, head_id), metrics in arc_metrics.items():
        weight = float(value_getter(metrics))
        if reverse:
            adjacency[head_id].append((tail_id, weight))
        else:
            adjacency[tail_id].append((head_id, weight))

    best = {node_id: inf for node_id in node_ids}
    best[start_node] = 0.0
    heap: List[tuple[float, int]] = [(0.0, start_node)]
    while heap:
        distance, node_id = heappop(heap)
        if distance > best[node_id]:
            continue
        for next_node, weight in adjacency[node_id]:
            candidate = distance + weight
            if candidate < best[next_node]:
                best[next_node] = candidate
                heappush(heap, (candidate, next_node))

    return best


def _zone_category_for_arc(
    *,
    arc_kind: str,
    tail_zone: str,
    head_zone: str,
) -> str:
    if arc_kind == "depot_connector":
        return "depot"
    if "city_center" in {tail_zone, head_zone}:
        return "city"
    return "suburban"


def _add_arc_kind(
    arc_kinds: Dict[ArcKey, str],
    tail_id: int,
    head_id: int,
    arc_kind: str,
) -> None:
    if tail_id == head_id:
        return
    priority = {"local": 0, "depot_connector": 1, "arterial": 2}
    key = (tail_id, head_id)
    existing = arc_kinds.get(key)
    if existing is None or priority[arc_kind] > priority[existing]:
        arc_kinds[key] = arc_kind
