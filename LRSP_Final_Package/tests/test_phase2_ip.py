from mespprc import (
    Arc,
    GLOBAL_LIMITS_INFEASIBLE,
    MESPPRCInstance,
    Node,
    NodeType,
    Phase1Solver,
    Phase2DPSolver,
    Phase2IPSolver,
    REACHABLE,
    ROUTE_SET_INFEASIBLE,
    Route,
)


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


def build_two_customer_phase1_instance() -> MESPPRCInstance:
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


def build_many_customer_instance(customer_count: int) -> MESPPRCInstance:
    instance = MESPPRCInstance()

    source = 0
    sink = customer_count + 1
    instance.add_node(Node(source, NodeType.SOURCE))
    for customer_id in range(1, customer_count + 1):
        instance.add_node(Node(customer_id, NodeType.CUSTOMER))
    instance.add_node(Node(sink, NodeType.SINK))

    for customer_id in range(1, customer_count + 1):
        instance.add_arc(Arc(source, customer_id, cost=0.0))
        instance.add_arc(Arc(customer_id, sink, cost=0.0))

    return instance


def test_phase2_ip_matches_dp_on_small_phase1_route_pool() -> None:
    instance = build_two_customer_phase1_instance()
    phase1 = Phase1Solver(instance).solve()

    dp_result = Phase2DPSolver(instance).solve(phase1.exported_routes)
    ip_result = Phase2IPSolver(instance).solve(phase1)

    assert dp_result.is_feasible
    assert ip_result.is_feasible
    assert dp_result.total_cost == ip_result.total_cost
    assert set(dp_result.selected_route_ids) == set(ip_result.selected_route_ids)
    assert dp_result.covered_customers == ip_result.covered_customers


def test_phase2_ip_detects_missing_customer_support() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, 1.0, [], [], {1}, [0, 1, 3]),
    ]

    result = Phase2IPSolver(instance).solve(routes)

    assert not result.is_feasible
    assert result.infeasibility_reason == ROUTE_SET_INFEASIBLE
    assert result.diagnostics.customers_with_no_supporting_routes == {2}
    assert result.diagnostics.uncovered_customers == {2}
    assert (
        result.diagnostics.route_pool_diagnostics.reduced_route_pool_uncovered_customers
        == {2}
    )


def test_phase2_ip_detects_global_limit_infeasibility() -> None:
    instance = build_phase2_only_instance(global_limits=[7.0])
    routes = [
        Route(1, 1.0, [], [4.0], {1}, [0, 1, 3]),
        Route(2, 2.0, [], [4.0], {2}, [0, 2, 3]),
    ]

    result = Phase2IPSolver(instance).solve(routes)

    assert not result.is_feasible
    assert result.infeasibility_reason == GLOBAL_LIMITS_INFEASIBLE
    assert result.diagnostics.minimum_resource_usage_to_cover == [8.0]
    assert result.diagnostics.resource_constraint_violations == {0: 1.0}


def test_phase2_ip_enforces_exact_once_service() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, -5.0, [], [], {1}, [0, 1, 3]),
        Route(2, -5.0, [], [], {2}, [0, 2, 3]),
        Route(3, -20.0, [], [], {1, 2}, [0, 1, 2, 3]),
    ]

    result = Phase2IPSolver(instance).solve(routes)

    assert result.is_feasible
    assert result.total_cost == -20.0
    assert result.selected_route_ids == [3]
    assert result.diagnostics.selected_customer_coverage_counts == {1: 1, 2: 1}


def test_phase2_ip_uses_positive_cost_route_when_needed() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, -3.0, [], [], {1}, [0, 1, 3]),
        Route(2, 5.0, [], [], {2}, [0, 2, 3]),
    ]

    result = Phase2IPSolver(instance).solve(routes)

    assert result.is_feasible
    assert result.total_cost == 2.0
    assert set(result.selected_route_ids) == {1, 2}


def test_phase2_ip_uses_safely_reduced_route_pool() -> None:
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

    result = Phase2IPSolver(instance).solve(routes)

    assert result.is_feasible
    assert result.total_cost == 3.0
    assert result.selected_route_ids == [1, 3]
    assert result.original_route_count == 3
    assert result.reduced_route_count == 2
    assert result.variable_count == 2
    assert result.diagnostics.reduced_route_count == 2
    assert result.diagnostics.route_pool_diagnostics.removed_route_ids == [2]


def test_phase2_ip_removes_cost_resource_dominated_routes_safely() -> None:
    instance = build_phase2_only_instance(global_limits=[10.0])
    routes = [
        Route(1, 1.0, [], [1.0], {1}, [0, 1, 3]),
        Route(2, 2.0, [], [2.0], {1}, [0, 1, 3]),
        Route(3, 3.0, [], [1.0], {2}, [0, 2, 3]),
    ]

    result = Phase2IPSolver(instance).solve(routes)
    reduced_only_result = Phase2IPSolver(instance).solve([routes[0], routes[2]])

    assert result.is_feasible
    assert result.total_cost == 4.0
    assert result.total_cost == reduced_only_result.total_cost
    assert result.selected_route_ids == [1, 3]
    assert result.original_route_count == 3
    assert result.reduced_route_count == 2
    assert result.diagnostics.route_pool_diagnostics.removed_route_ids == [2]


def test_phase2_ip_large_synthetic_instance_uses_small_reduced_model() -> None:
    customer_count = 30
    instance = build_many_customer_instance(customer_count)
    routes = []
    route_id = 1
    sink = customer_count + 1
    for customer_id in range(1, customer_count + 1):
        routes.append(Route(route_id, 1.0, [], [], {customer_id}, [0, customer_id, sink]))
        route_id += 1
        routes.append(Route(route_id, 2.0, [], [], {customer_id}, [0, customer_id, sink]))
        route_id += 1
        routes.append(Route(route_id, 1.0, [], [], {customer_id}, [0, customer_id, sink]))
        route_id += 1

    result = Phase2IPSolver(instance).solve(routes, collect_diagnostics=False)

    assert result.is_feasible
    assert result.original_route_count == customer_count * 3
    assert result.reduced_route_count == customer_count
    assert result.variable_count == customer_count
    assert result.coverage_constraint_count == customer_count
    assert result.global_constraint_count == 0
    assert result.diagnostics is None


def test_phase2_ip_handles_missing_signatures_without_diagnostics_overhead() -> None:
    instance = build_phase2_only_instance()
    routes = [
        Route(1, 1.0, [], [], {1}, [0, 1, 3]),
        Route(2, 2.0, [], [], {2}, [0, 2, 3]),
    ]

    result = Phase2IPSolver(instance).solve(routes, collect_diagnostics=False)

    assert result.is_feasible
    assert result.total_cost == 3.0
    assert result.selected_route_ids == [1, 2]
    assert result.diagnostics is None
