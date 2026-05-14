from __future__ import annotations

from math import sqrt

import pytest

from mespprc import (
    GeneratorConfig,
    NodeType,
    Phase1Solver,
    Phase2DPSolver,
    Phase2IPSolver,
    generate_instance,
)


def _euclidean(a: tuple[float, float], b: tuple[float, float]) -> float:
    return sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2)


def test_default_config_produces_validated_instance():
    instance = generate_instance(GeneratorConfig(num_customers=8, seed=1))
    instance.validate()

    assert len(instance.customers()) == 8
    assert instance.source is not None
    assert instance.sink is not None
    assert len(instance.local_limits) == 2
    assert len(instance.global_limits) == 1


def test_graph_is_fully_connected_in_the_mespprc_sense():
    """
    Customers are pairwise connected; source connects to every customer;
    every customer connects to the sink. The source has no incoming arcs and
    the sink has no outgoing arcs, which is what Phase 1 expects.
    """

    n = 6
    config = GeneratorConfig(num_customers=n, seed=2)
    instance = generate_instance(config)
    customers = instance.customers()
    source = instance.source
    sink = instance.sink

    expected_arc_count = 2 * n + n * (n - 1)
    assert len(instance.arcs) == expected_arc_count

    # source -> every customer
    for c in customers:
        assert (source, c) in instance.arcs
    # every customer -> sink
    for c in customers:
        assert (c, sink) in instance.arcs
    # customer-customer pairs
    for tail in customers:
        for head in customers:
            if tail != head:
                assert (tail, head) in instance.arcs

    # negative space: no anything-into-source, no anything-out-of-sink, no
    # source -> sink direct arc, no self-loops.
    for (tail, head) in instance.arcs:
        assert head != source
        assert tail != sink
        assert tail != head
    assert (source, sink) not in instance.arcs


def test_arc_costs_are_pure_euclidean_distance():
    config = GeneratorConfig(num_customers=5, seed=3)
    instance = generate_instance(config)

    for (tail, head), arc in instance.arcs.items():
        assert arc.cost >= 0.0
        # Customer-customer arcs in opposite directions have equal cost
        # because cost is purely euclidean distance.
        if tail in instance.customers() and head in instance.customers():
            assert arc.cost == pytest.approx(
                instance.get_arc(head, tail).cost, abs=1e-9
            )
    # Source -> customer and customer -> sink across the same customer have
    # equal cost (depot is at one place).
    for c in instance.customers():
        assert instance.get_arc(instance.source, c).cost == pytest.approx(
            instance.get_arc(c, instance.sink).cost, abs=1e-9
        )


def test_arc_resource_model_matches_specification():
    """
    For arc (i, j):
      route_time   = d / v + s(j)
      bus_capacity = q(j)
      duty_time    = d / v + s(j)
    """

    config = GeneratorConfig(
        num_customers=4,
        seed=4,
        service_time_base=1.0,
        service_time_slope_per_customer=0.0,
        vehicle_speed=1.0,
    )
    instance = generate_instance(config)
    customer_ids = set(instance.customers())

    for (tail, head), arc in instance.arcs.items():
        cost = arc.cost
        route_time, bus_capacity = arc.local_res
        duty_time = arc.global_res[0]

        if head in customer_ids:
            assert route_time == pytest.approx(cost / 1.0 + 1.0, abs=1e-9)
            assert duty_time == pytest.approx(cost / 1.0 + 1.0, abs=1e-9)
            assert bus_capacity > 0.0
        else:
            assert route_time == pytest.approx(cost, abs=1e-9)
            assert duty_time == pytest.approx(cost, abs=1e-9)
            assert bus_capacity == pytest.approx(0.0, abs=1e-9)


def test_service_time_scales_linearly_with_customer_count():
    small = GeneratorConfig(
        num_customers=5,
        seed=5,
        service_time_base=0.0,
        service_time_slope_per_customer=0.2,
    )
    large = GeneratorConfig(
        num_customers=20,
        seed=5,
        service_time_base=0.0,
        service_time_slope_per_customer=0.2,
    )
    assert small.service_time() == pytest.approx(1.0, abs=1e-9)
    assert large.service_time() == pytest.approx(4.0, abs=1e-9)


def test_resource_limit_formulas_are_what_we_advertise():
    config = GeneratorConfig(
        num_customers=10,
        seed=42,
        beta_route=2.0,
        rho_capacity=4.0,
        gamma_duty=0.8,
    )
    instance = generate_instance(config)
    service_time = config.service_time()

    customer_ids = instance.customers()
    depot = instance.nodes[instance.source]
    # Reconstruct distance-to-depot from arc costs (cost == euclidean distance).
    distance_to_depot = {
        cid: instance.get_arc(instance.source, cid).cost for cid in customer_ids
    }
    singleton_route_time = {
        cid: 2.0 * distance_to_depot[cid] + service_time for cid in customer_ids
    }
    expected_route_time_limit = config.beta_route * max(singleton_route_time.values())

    assert instance.local_limits[0] == pytest.approx(expected_route_time_limit, rel=1e-9)

    # Capacity is non-negative and at least max demand.
    demands = [
        instance.get_arc(instance.source, cid).local_res[1] for cid in customer_ids
    ]
    assert instance.local_limits[1] >= max(demands) - 1e-9

    expected_global = config.gamma_duty * sum(singleton_route_time.values())
    assert instance.global_limits[0] == pytest.approx(expected_global, rel=1e-9)
    del depot


def test_same_seed_produces_identical_instances():
    a = generate_instance(GeneratorConfig(num_customers=12, seed=99))
    b = generate_instance(GeneratorConfig(num_customers=12, seed=99))
    assert a.local_limits == b.local_limits
    assert a.global_limits == b.global_limits
    for key, arc in a.arcs.items():
        other = b.arcs[key]
        assert arc.cost == pytest.approx(other.cost, rel=1e-12)
        assert list(arc.local_res) == pytest.approx(list(other.local_res), rel=1e-12)
        assert list(arc.global_res) == pytest.approx(list(other.global_res), rel=1e-12)


def test_different_seeds_produce_different_instances():
    a = generate_instance(GeneratorConfig(num_customers=12, seed=1))
    b = generate_instance(GeneratorConfig(num_customers=12, seed=2))
    different = any(
        a.arcs[key].cost != b.arcs[key].cost for key in a.arcs
    )
    assert different


def test_each_customer_has_a_feasible_singleton_round_trip():
    config = GeneratorConfig(num_customers=10, seed=7)
    instance = generate_instance(config)
    route_time_limit, capacity_limit = instance.local_limits

    for cid in instance.customers():
        forward = instance.get_arc(instance.source, cid)
        backward = instance.get_arc(cid, instance.sink)
        round_trip_route_time = forward.local_res[0] + backward.local_res[0]
        round_trip_load = forward.local_res[1] + backward.local_res[1]
        assert round_trip_route_time <= route_time_limit + 1e-9
        assert round_trip_load <= capacity_limit + 1e-9


def test_invalid_configurations_raise():
    with pytest.raises(ValueError):
        GeneratorConfig(num_customers=0)
    with pytest.raises(ValueError):
        GeneratorConfig(num_customers=5, beta_route=0.5)  # below 1.0
    with pytest.raises(ValueError):
        GeneratorConfig(num_customers=5, demand_min=0)
    with pytest.raises(ValueError):
        GeneratorConfig(
            num_customers=5,
            coordinate_min=0.0,
            coordinate_max=10.0,
            depot_coordinate=(50.0, 50.0),  # outside box
        )


def test_full_pipeline_runs_phase1_and_both_phase2_solvers():
    """Smoke test: a generated instance must be solvable end to end.

    `gamma_duty=1.0` makes the all-singletons solution feasible by construction,
    so the smoke test exercises the pipeline rather than the duty-time tightness
    knob.
    """

    instance = generate_instance(
        GeneratorConfig(num_customers=5, seed=11, gamma_duty=1.0)
    )

    phase1 = Phase1Solver(instance)
    phase1_result = phase1.solve()
    assert phase1_result.feasible_routes, "Phase 1 produced no feasible routes."

    dp = Phase2DPSolver(instance).solve(phase1_result.feasible_routes)
    assert dp.is_feasible
    assert dp.coverage_complete

    ip = Phase2IPSolver(instance).solve(
        phase1_result.feasible_routes,
        collect_diagnostics=False,
    )
    assert ip.is_feasible
    assert ip.coverage_complete

    assert dp.total_cost == pytest.approx(ip.objective_value, rel=1e-6, abs=1e-6)


def test_node_types_match_id_layout():
    config = GeneratorConfig(num_customers=4, seed=8)
    instance = generate_instance(config)

    assert instance.nodes[0].node_type == NodeType.SOURCE
    assert instance.nodes[config.num_customers + 1].node_type == NodeType.SINK
    for cid in range(1, config.num_customers + 1):
        assert instance.nodes[cid].node_type == NodeType.CUSTOMER


def test_depot_coordinate_can_be_relocated():
    config = GeneratorConfig(
        num_customers=5,
        seed=13,
        coordinate_min=0.0,
        coordinate_max=100.0,
        depot_coordinate=(0.0, 0.0),
    )
    instance = generate_instance(config)
    # source -> customer and customer -> sink share the same depot endpoint,
    # so the two arc costs are equal across every customer.
    for c in instance.customers():
        assert instance.get_arc(instance.source, c).cost == pytest.approx(
            instance.get_arc(c, instance.sink).cost, abs=1e-9
        )
