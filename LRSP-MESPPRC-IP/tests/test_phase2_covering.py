from mespprc import (
    Arc,
    GLOBAL_LIMITS_INFEASIBLE,
    Label,
    MESPPRCInstance,
    Node,
    NodeType,
    Phase1Solver,
    Phase2DPSolver,
    Phase2CoveringDPSolver,
    REACHABLE,
    ROUTE_SET_INFEASIBLE,
    Route,
    TEMP_UNREACHABLE,
)
from mespprc.phase2_dp import _StateRecord


def build_phase2_only_instance(
    *,
    global_limits: list[float] | None = None,
) -> MESPPRCInstance:
    instance = MESPPRCInstance(global_limits=global_limits)

    instance.add_node(Node(0, NodeType.SOURCE))
    instance.add_node(Node(1, NodeType.CUSTOMER))
    instance.add_node(Node(2, NodeType.CUSTOMER))
    instance.add_node(Node(3, NodeType.SINK))

    zero_global = [0.0] * len(global_limits or [])
    instance.add_arc(Arc(0, 1, cost=0.0, global_res=zero_global))
    instance.add_arc(Arc(1, 3, cost=0.0, global_res=zero_global))
    instance.add_arc(Arc(0, 2, cost=0.0, global_res=zero_global))
    instance.add_arc(Arc(2, 3, cost=0.0, global_res=zero_global))

    return instance


def build_positive_route_generation_instance() -> MESPPRCInstance:
    instance = MESPPRCInstance(local_limits=[5.0], global_limits=[10.0])

    instance.add_node(Node(0, NodeType.SOURCE))
    instance.add_node(Node(1, NodeType.CUSTOMER))
    instance.add_node(Node(2, NodeType.CUSTOMER))
    instance.add_node(Node(3, NodeType.SINK))

    instance.add_arc(Arc(0, 1, cost=4.0, local_res=[2.0], global_res=[2.0]))
    instance.add_arc(Arc(1, 3, cost=0.0, local_res=[1.0], global_res=[1.0]))
    instance.add_arc(Arc(0, 2, cost=1.0, local_res=[2.0], global_res=[2.0]))
    instance.add_arc(Arc(2, 3, cost=0.0, local_res=[1.0], global_res=[1.0]))

    return instance


def test_full_coverage_is_required_even_when_partial_solution_is_cheaper() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, 1.0, [], [], {1}, [0, 1, 3]),
        Route(2, 10.0, [], [], {2}, [0, 2, 3]),
    ]

    result = Phase2DPSolver(instance).solve(routes)
    assert result.is_feasible
    assert result.coverage_complete
    assert result.total_cost == 11.0
    assert result.selected_route_ids == [1, 2]
    assert result.covered_customers == {1, 2}


def test_positive_cost_route_can_be_required_for_full_coverage() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, -3.0, [], [], {1}, [0, 1, 3]),
        Route(2, 5.0, [], [], {2}, [0, 2, 3]),
    ]

    result = Phase2DPSolver(instance).solve(routes)
    assert result.is_feasible
    assert result.total_cost == 2.0
    assert result.selected_route_ids == [1, 2]


def test_partial_coverage_is_no_longer_accepted() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, -10.0, [], [], {1}, [0, 1, 3]),
        Route(2, 9.0, [], [], {2}, [0, 2, 3]),
    ]

    result = Phase2DPSolver(instance).solve(routes)
    assert result.is_feasible
    assert result.coverage_complete
    assert result.total_cost == -1.0
    assert result.selected_route_ids == [1, 2]


def test_infeasible_when_generated_routes_cannot_cover_all_customers() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, 1.0, [], [], {1}, [0, 1, 3]),
    ]

    result = Phase2DPSolver(instance).solve(routes)
    assert not result.is_feasible
    assert result.infeasibility_reason == ROUTE_SET_INFEASIBLE
    assert not result.coverage_complete
    assert result.total_cost is None


def test_infeasible_when_global_limits_block_full_coverage() -> None:
    instance = build_phase2_only_instance(global_limits=[7.0])
    routes = [
        Route(1, 1.0, [], [4.0], {1}, [0, 1, 3]),
        Route(2, 2.0, [], [4.0], {2}, [0, 2, 3]),
    ]

    result = Phase2DPSolver(instance).solve(routes)
    assert not result.is_feasible
    assert result.infeasibility_reason == GLOBAL_LIMITS_INFEASIBLE


def test_exact_once_service_blocks_overlapping_routes() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, 1.0, [], [], {1}, [0, 1, 3]),
        Route(2, 10.0, [], [], {1, 2}, [0, 1, 2, 3]),
        Route(3, 3.0, [], [], {2}, [0, 2, 3]),
    ]

    result = Phase2DPSolver(instance).solve(routes)
    assert result.is_feasible
    assert result.coverage_complete
    assert result.total_cost == 4.0
    assert result.selected_route_ids == [1, 3]


def test_phase1_exports_positive_cost_routes_without_negativity_screening() -> None:
    instance = build_positive_route_generation_instance()

    phase1 = Phase1Solver(instance).solve()
    assert len(phase1.feasible_routes) == 2
    assert sorted(route.cost for route in phase1.feasible_routes) == [1.0, 4.0]
    assert sorted(route.cost for route in phase1.negative_cost_routes) == [1.0, 4.0]


def test_phase2_builds_explicit_route_network_and_diagnostics() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, 1.0, [], [], {1}, [0, 1, 3], first_customer_in_route=1),
        Route(2, 2.0, [], [], {2}, [0, 2, 3], first_customer_in_route=2),
        Route(3, 5.0, [], [], {1, 2}, [0, 1, 2, 3], first_customer_in_route=1),
    ]

    result = Phase2DPSolver(instance).solve(routes)
    route_network = result.route_network

    assert route_network.nodes[route_network.source].kind == "source"
    assert route_network.nodes[route_network.sink].kind == "sink"
    assert len(route_network.route_node_to_route) == 3
    assert set(route_network.successors(route_network.source)) == {1, 2, 3, 4}

    node_by_route_id = {
        route.route_id: node_id
        for node_id, route in route_network.route_node_to_route.items()
    }
    assert node_by_route_id[2] in route_network.successors(node_by_route_id[1])
    assert node_by_route_id[3] not in route_network.successors(node_by_route_id[1])

    assert result.diagnostics.customer_to_route_ids == {1: [1, 3], 2: [2, 3]}
    assert result.diagnostics.route_ids_by_first_customer == {1: [1, 3], 2: [2]}
    assert result.diagnostics.route_ids_by_covered_customer_set == {
        (1,): [1],
        (2,): [2],
        (1, 2): [3],
    }


def test_phase2_customer_states_keep_temp_vs_perm_information() -> None:
    instance = build_phase2_only_instance(global_limits=[3.0])
    solver = Phase2CoveringDPSolver(instance)
    routes = [
        Route(1, 1.0, [], [4.0], {1}, [0, 1, 3], first_customer_in_route=1),
        Route(2, 2.0, [], [1.0], {2}, [0, 2, 3], first_customer_in_route=2),
    ]

    normalized_routes = solver._normalize_routes(routes)
    route_network = solver.build_route_network(normalized_routes)
    solver._route_network = route_network
    solver._enforce_global_limits = True

    initial = solver._initial_state(route_network.source)
    assert initial.label.unreachable_vector == [TEMP_UNREACHABLE, REACHABLE]

    result = solver.solve(routes)
    assert not result.is_feasible
    assert result.infeasibility_reason == GLOBAL_LIMITS_INFEASIBLE
    assert result.diagnostics.uncovered_by_individually_global_feasible_routes == {1}


def build_signature_sensitive_instance() -> MESPPRCInstance:
    instance = MESPPRCInstance()

    instance.add_node(Node(0, NodeType.SOURCE))
    instance.add_node(Node(1, NodeType.CUSTOMER))
    instance.add_node(Node(2, NodeType.CUSTOMER))
    instance.add_node(Node(3, NodeType.SINK))

    instance.add_arc(Arc(0, 1, cost=0.0))
    instance.add_arc(Arc(1, 3, cost=0.0))
    instance.add_arc(Arc(0, 2, cost=0.0))
    instance.add_arc(Arc(2, 3, cost=0.0))

    return instance


def test_structural_route_reduction_uses_phase1_signatures() -> None:
    instance = build_signature_sensitive_instance()
    routes = [
        Route(
            1,
            1.0,
            [],
            [],
            {1},
            [0, 1, 3],
            first_customer_in_route=1,
            customer_state_signature=[1, REACHABLE],
        ),
        Route(
            2,
            1.0,
            [],
            [],
            {1},
            [0, 1, 3],
            first_customer_in_route=1,
            customer_state_signature=[1, -1],
        ),
        Route(
            3,
            2.0,
            [],
            [],
            {2},
            [0, 2, 3],
            first_customer_in_route=2,
            customer_state_signature=[REACHABLE, 1],
        ),
    ]

    result = Phase2DPSolver(instance).solve(routes)

    assert result.is_feasible
    assert result.total_cost == 3.0
    assert result.selected_route_ids == [1, 3]
    assert result.diagnostics.removed_route_ids == [2]
    assert result.diagnostics.reduction_records[0].kept_route_id == 1
    assert result.diagnostics.reduction_records[0].removed_route_id == 2
    assert result.diagnostics.original_route_pool_customer_to_route_ids == {
        1: [1, 2],
        2: [3],
    }
    assert result.diagnostics.reduced_route_pool_customer_to_route_ids == {
        1: [1],
        2: [3],
    }
    assert result.diagnostics.customer_to_route_ids == {
        1: [1],
        2: [3],
    }
    assert result.diagnostics.original_route_pool_route_ids_by_cover_and_structural_signature == {
        ((1,), (0, 1)): [1],
        ((1,), (0, 3)): [2],
        ((2,), (1, 0)): [3],
    }
    assert result.diagnostics.reduced_route_pool_route_ids_by_cover_and_structural_signature == {
        ((1,), (0, 1)): [1],
        ((2,), (1, 0)): [3],
    }
    assert {
        route.route_id for route in result.route_network.route_node_to_route.values()
    } == {1, 3}


def test_signature_sensitive_reduction_does_not_prune_cheaper_weaker_route() -> None:
    instance = build_signature_sensitive_instance()
    routes = [
        Route(
            1,
            5.0,
            [],
            [],
            {1},
            [0, 1, 3],
            first_customer_in_route=1,
            customer_state_signature=[1, REACHABLE],
        ),
        Route(
            2,
            1.0,
            [],
            [],
            {1},
            [0, 1, 3],
            first_customer_in_route=1,
            customer_state_signature=[1, -1],
        ),
        Route(
            3,
            1.0,
            [],
            [],
            {2},
            [0, 2, 3],
            first_customer_in_route=2,
            customer_state_signature=[REACHABLE, 1],
        ),
    ]

    result = Phase2DPSolver(instance).solve(routes)

    assert result.is_feasible
    assert result.total_cost == 2.0
    assert result.selected_route_ids == [2, 3]
    assert result.diagnostics.removed_route_ids == []
    assert set(result.diagnostics.kept_route_ids) == {1, 2, 3}


def test_phase2_state_dominance_uses_structural_profiles_beyond_ordering() -> None:
    instance = build_signature_sensitive_instance()
    solver = Phase2CoveringDPSolver(instance)

    label = Label(
        current_node=1,
        cost=3.0,
        resources=[],
        unreachable_vector=[1, REACHABLE],
    )
    stronger = _StateRecord(
        label=label,
        route_sequence=[1],
        sequence_structural_profile=(0, 1),
        residual_structural_support=(0, 1),
    )
    weaker = _StateRecord(
        label=label,
        route_sequence=[2],
        sequence_structural_profile=(0, 3),
        residual_structural_support=(0, 2),
    )

    assert solver._state_dominates(stronger, weaker)
    assert not solver._state_dominates(weaker, stronger)


def test_feedback_diagnostics_expose_route_pool_weaknesses() -> None:
    instance = build_signature_sensitive_instance()
    routes = [
        Route(
            1,
            1.0,
            [],
            [],
            {1},
            [0, 1, 3],
            first_customer_in_route=1,
            customer_state_signature=[1, REACHABLE],
        ),
        Route(
            2,
            1.0,
            [],
            [],
            {1},
            [0, 1, 3],
            first_customer_in_route=1,
            customer_state_signature=[1, -1],
        ),
        Route(
            3,
            2.0,
            [],
            [],
            {2},
            [0, 2, 3],
            first_customer_in_route=2,
            customer_state_signature=[REACHABLE, 1],
        ),
    ]

    result = Phase2DPSolver(instance).solve(routes)
    diagnostics = result.diagnostics

    assert diagnostics.customers_with_low_route_support == {1, 2}
    assert diagnostics.bottleneck_customers == {1, 2}
    assert diagnostics.customers_only_in_structurally_weak_routes == set()
    assert diagnostics.route_classes_by_covered_set[(1,)][0]["route_ids"] == [1]
    assert diagnostics.route_classes_by_covered_set[(2,)][0]["route_ids"] == [3]
    assert diagnostics.dominance_reduction_summary["removed_route_count"] == 1
    assert diagnostics.dominance_reduction_summary["removed_route_ids"] == [2]
    assert "Low-support customers" in diagnostics.diagnostic_summary
