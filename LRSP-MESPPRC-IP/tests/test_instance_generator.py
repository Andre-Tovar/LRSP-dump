import pytest

from mespprc import (
    CalibrationConfig,
    GeneratorConfig,
    Phase1Solver,
    Phase2DPSolver,
    Phase2IPSolver,
    calibrate_instance,
    generate_benchmark_instance,
    generate_instance,
)


def _reachable_from(start_node: int, successors: dict[int, list[int]]) -> set[int]:
    seen = {start_node}
    stack = [start_node]
    while stack:
        node_id = stack.pop()
        for succ in successors.get(node_id, []):
            if succ in seen:
                continue
            seen.add(succ)
            stack.append(succ)
    return seen


def _expected_zone_bucket(arc_kind: str, tail_zone: str, head_zone: str) -> str:
    if arc_kind == "depot_connector":
        return "depot"
    if "city_center" in {tail_zone, head_zone}:
        return "city"
    return "suburban"


def test_generated_instance_has_colocated_depot_nodes_and_valid_arc_dimensions() -> None:
    generated = generate_instance(GeneratorConfig(num_customers=12, seed=7))
    instance = generated.instance

    assert generated.source_id != generated.sink_id
    assert generated.coordinates[generated.source_id] == generated.coordinates[generated.sink_id]
    assert (generated.source_id, generated.sink_id) not in generated.arc_kinds
    assert generated.local_resource_names == ("route_time", "bus_capacity")
    assert generated.global_resource_names == ("duty_time",)
    assert len(instance.local_limits) == 2
    assert len(instance.global_limits) == 1

    direct_source_arcs = {
        head_id
        for (tail_id, head_id) in generated.arc_kinds
        if tail_id == generated.source_id
    }
    direct_sink_predecessors = {
        tail_id
        for (tail_id, head_id) in generated.arc_kinds
        if head_id == generated.sink_id
    }
    assert 0 < len(direct_source_arcs) < len(instance.customers())
    assert 0 < len(direct_sink_predecessors) < len(instance.customers())

    reachable_from_source = _reachable_from(generated.source_id, instance.out_neighbors)
    reachable_to_sink = _reachable_from(generated.sink_id, instance.in_neighbors)
    for customer_id in instance.customers():
        assert customer_id in reachable_from_source
        assert customer_id in reachable_to_sink

    customer_count = len(instance.customers())
    customer_customer_arc_count = sum(
        1
        for tail_id, head_id in generated.arc_kinds
        if tail_id in instance.customers() and head_id in instance.customers()
    )
    assert customer_customer_arc_count < customer_count * (customer_count - 1)

    instance.validate()


def test_generated_arc_attributes_follow_deterministic_multipliers() -> None:
    generated = generate_instance(GeneratorConfig(num_customers=10, seed=5))
    instance = generated.instance

    expected_time = {
        ("local", "suburban"): 1.0,
        ("local", "city"): 1.2,
        ("arterial", "suburban"): 0.8,
        ("arterial", "city"): 1.0,
        ("depot_connector", "depot"): 1.0,
    }
    expected_burden = {
        ("local", "suburban"): 1.0,
        ("local", "city"): 1.3,
        ("arterial", "suburban"): 0.8,
        ("arterial", "city"): 0.9,
        ("depot_connector", "depot"): 1.0,
    }

    for (tail_id, head_id), arc in instance.arcs.items():
        tail = generated.coordinates[tail_id]
        head = generated.coordinates[head_id]
        distance = ((tail[0] - head[0]) ** 2 + (tail[1] - head[1]) ** 2) ** 0.5
        arc_kind = generated.arc_kinds[(tail_id, head_id)]
        zone_bucket = _expected_zone_bucket(
            arc_kind,
            generated.node_zones[tail_id],
            generated.node_zones[head_id],
        )

        assert distance > 0.0
        assert arc.cost == pytest.approx(distance * generated.config.cost_distance_multiplier)
        assert arc.local_res[0] == pytest.approx(distance * expected_time[(arc_kind, zone_bucket)])
        assert arc.local_res[1] == pytest.approx(distance * expected_burden[(arc_kind, zone_bucket)])
        assert arc.global_res[0] == pytest.approx(arc.local_res[0])


def test_generator_is_reproducible_from_seed_and_replicate() -> None:
    config = GeneratorConfig(num_customers=10, seed=11, replicate_id=3)
    first = generate_instance(config)
    second = generate_instance(config)

    assert first.coordinates == second.coordinates
    assert first.node_zones == second.node_zones
    assert first.arc_kinds == second.arc_kinds
    assert first.instance.local_limits == second.instance.local_limits
    assert first.instance.global_limits == second.instance.global_limits
    assert sorted(first.instance.arcs) == sorted(second.instance.arcs)


def test_calibration_produces_solver_compatible_instance() -> None:
    raw = generate_instance(
        GeneratorConfig(
            num_customers=5,
            seed=2,
            local_tightness=0.95,
            global_tightness=0.95,
        )
    )
    calibrated = calibrate_instance(
        raw,
        CalibrationConfig(max_iterations=12, local_relaxation_factor=1.10, global_relaxation_factor=1.10),
    )

    assert calibrated.calibration_report is not None
    assert calibrated.calibration_report.converged
    assert calibrated.calibration_report.topology_adjustments >= 0
    assert (
        calibrated.calibration_report.topology_adjustments
        + calibrated.calibration_report.local_adjustments
        + calibrated.calibration_report.global_adjustments
        > 0
    )
    assert all(
        final_limit >= initial_limit
        for final_limit, initial_limit in zip(
            calibrated.calibration_report.final_local_limits,
            calibrated.calibration_report.initial_local_limits,
        )
    )
    assert all(
        final_limit >= initial_limit
        for final_limit, initial_limit in zip(
            calibrated.calibration_report.final_global_limits,
            calibrated.calibration_report.initial_global_limits,
        )
    )

    phase1 = Phase1Solver(calibrated.instance).solve()
    phase2_dp = Phase2DPSolver(calibrated.instance).solve(phase1.feasible_routes)
    phase2_ip = Phase2IPSolver(calibrated.instance).solve(
        phase1.feasible_routes,
        collect_diagnostics=False,
    )

    assert phase1.feasible_routes
    assert phase2_dp.is_feasible
    assert phase2_dp.coverage_complete
    assert phase2_ip.is_feasible
    assert phase2_ip.coverage_complete
    assert phase2_dp.total_cost == phase2_ip.total_cost


def test_generate_benchmark_instance_can_calibrate_in_one_call() -> None:
    generated = generate_benchmark_instance(
        GeneratorConfig(num_customers=5, seed=2, local_tightness=0.95, global_tightness=0.95),
        calibrate=True,
        calibration_config=CalibrationConfig(max_iterations=8),
    )

    assert generated.calibration_report is not None
    assert generated.calibration_report.converged
    assert generated.calibration_report.topology_adjustments >= 0
