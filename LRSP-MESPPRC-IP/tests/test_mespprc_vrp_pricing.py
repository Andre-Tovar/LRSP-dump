from mespprc_vrp import (
    Arc,
    GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING,
    MESPPRCInstance,
    NO_IMPROVING_PAIRING_STATUS,
    NO_NEGATIVE_ROUTES_FROM_PHASE1,
    Node,
    NodeType,
    Phase1Result,
    Phase1Solver,
    Phase2DPPricingSolver,
    Phase2DPSolver,
    Phase2IPPricingSolver,
    Phase2IPSolver,
    Route,
)
import mespprc_vrp


def build_phase1_pricing_instance() -> MESPPRCInstance:
    instance = MESPPRCInstance(local_limits=[5.0], global_limits=[10.0])

    instance.add_node(Node(0, NodeType.SOURCE))
    instance.add_node(Node(1, NodeType.CUSTOMER))
    instance.add_node(Node(2, NodeType.CUSTOMER))
    instance.add_node(Node(3, NodeType.SINK))

    instance.add_arc(Arc(0, 1, cost=-3.0, local_res=[2.0], global_res=[2.0]))
    instance.add_arc(Arc(1, 3, cost=0.0, local_res=[1.0], global_res=[1.0]))
    instance.add_arc(Arc(0, 2, cost=2.0, local_res=[2.0], global_res=[2.0]))
    instance.add_arc(Arc(2, 3, cost=0.0, local_res=[1.0], global_res=[1.0]))

    return instance


def build_phase2_pricing_instance(
    *,
    global_limits: list[float] | None = None,
) -> MESPPRCInstance:
    instance = MESPPRCInstance(global_limits=global_limits)

    instance.add_node(Node(0, NodeType.SOURCE))
    instance.add_node(Node(1, NodeType.CUSTOMER))
    instance.add_node(Node(2, NodeType.CUSTOMER))
    instance.add_node(Node(3, NodeType.CUSTOMER))
    instance.add_node(Node(4, NodeType.CUSTOMER))
    instance.add_node(Node(5, NodeType.SINK))

    zero_global = [0.0] * len(global_limits or [])
    for customer_id in (1, 2, 3, 4):
        instance.add_arc(Arc(0, customer_id, cost=0.0, global_res=zero_global))
        instance.add_arc(Arc(customer_id, 5, cost=0.0, global_res=zero_global))

    return instance


def pricing_routes() -> list[Route]:
    return [
        Route(1, -5.0, [], [3.0], {1}, [0, 1, 5], first_customer_in_route=1),
        Route(2, -4.0, [], [3.0], {2}, [0, 2, 5], first_customer_in_route=2),
        Route(3, -3.0, [], [2.0], {3}, [0, 3, 5], first_customer_in_route=3),
        Route(4, -7.0, [], [6.0], {1, 2}, [0, 1, 2, 5], first_customer_in_route=1),
        Route(5, 2.0, [], [1.0], {4}, [0, 4, 5], first_customer_in_route=4),
    ]


def test_phase1_pricing_filters_negative_routes_and_exports_them() -> None:
    phase1 = Phase1Solver(build_phase1_pricing_instance()).solve()

    assert sorted(route.cost for route in phase1.feasible_routes) == [-3.0, 2.0]
    assert [route.cost for route in phase1.negative_cost_routes] == [-3.0]
    assert phase1.best_negative_route is not None
    assert phase1.best_negative_route.cost == -3.0
    assert phase1.has_negative_route
    assert phase1.best_cost == -3.0
    assert phase1.exported_routes == phase1.negative_cost_routes


def test_phase2_dp_pricing_finds_best_negative_subset_without_full_cover() -> None:
    instance = build_phase2_pricing_instance(global_limits=[8.0])

    result = Phase2DPSolver(instance).solve(pricing_routes())

    assert result.status == "optimal"
    assert result.is_feasible
    assert result.total_cost == -12.0
    assert result.selected_route_ids == [1, 2, 3]
    assert result.covered_customers == {1, 2, 3}
    assert result.has_negative_pairing
    assert result.has_improving_pairing
    assert 4 not in result.covered_customers


def test_phase2_pricing_rejects_overlapping_routes_but_allows_singletons() -> None:
    instance = build_phase2_pricing_instance(global_limits=[5.0])
    routes = [
        Route(1, -5.0, [], [3.0], {1}, [0, 1, 5]),
        Route(2, -4.0, [], [3.0], {1, 2}, [0, 1, 2, 5]),
        Route(3, -3.0, [], [2.0], {2}, [0, 2, 5]),
    ]

    dp_result = Phase2DPPricingSolver(instance).solve(routes)
    ip_result = Phase2IPPricingSolver(instance).solve(routes)

    assert dp_result.selected_route_ids == [1, 3]
    assert dp_result.total_cost == -8.0
    assert ip_result.selected_route_ids == [1, 3]
    assert ip_result.total_cost == -8.0


def test_phase2_pricing_reports_no_improving_pairing_when_negative_routes_violate_global_limit() -> None:
    instance = build_phase2_pricing_instance(global_limits=[2.0])
    routes = [
        Route(1, -5.0, [], [3.0], {1}, [0, 1, 5]),
        Route(2, -4.0, [], [4.0], {2}, [0, 2, 5]),
    ]

    dp_result = Phase2DPPricingSolver(instance).solve(routes)
    ip_result = Phase2IPPricingSolver(instance).solve(routes)

    assert dp_result.status == NO_IMPROVING_PAIRING_STATUS
    assert not dp_result.is_feasible
    assert dp_result.infeasibility_reason == GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING
    assert not dp_result.has_improving_pairing

    assert ip_result.status == NO_IMPROVING_PAIRING_STATUS
    assert not ip_result.is_feasible
    assert ip_result.infeasibility_reason == GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING
    assert not ip_result.has_improving_pairing


def test_phase2_pricing_handles_empty_negative_pool_from_phase1_result() -> None:
    phase1 = Phase1Result(
        labels_at_sink=[],
        feasible_routes=[Route(1, 2.0, [], [], {1}, [0, 1, 5])],
        negative_cost_routes=[],
        best_negative_route=None,
        has_negative_route=False,
        best_cost=2.0,
        all_label_buckets={},
    )
    instance = build_phase2_pricing_instance()

    dp_result = Phase2DPPricingSolver(instance).solve(phase1)
    ip_result = Phase2IPPricingSolver(instance).solve(phase1)

    assert dp_result.status == NO_IMPROVING_PAIRING_STATUS
    assert dp_result.infeasibility_reason == NO_NEGATIVE_ROUTES_FROM_PHASE1
    assert not dp_result.has_improving_pairing

    assert ip_result.status == NO_IMPROVING_PAIRING_STATUS
    assert ip_result.infeasibility_reason == NO_NEGATIVE_ROUTES_FROM_PHASE1
    assert not ip_result.has_improving_pairing


def test_phase2_dp_and_ip_pricing_match_on_same_phase1_result() -> None:
    instance = build_phase2_pricing_instance(global_limits=[8.0])
    negative_routes = [route for route in pricing_routes() if route.cost < 0.0]
    phase1 = Phase1Result(
        labels_at_sink=[],
        feasible_routes=pricing_routes(),
        negative_cost_routes=negative_routes,
        best_negative_route=min(negative_routes, key=lambda route: route.cost),
        has_negative_route=True,
        best_cost=-7.0,
        all_label_buckets={},
    )

    dp_result = Phase2DPPricingSolver(instance).solve(phase1)
    ip_result = Phase2IPPricingSolver(instance).solve(phase1)

    assert dp_result.total_cost == ip_result.total_cost == -12.0
    assert dp_result.selected_route_ids == ip_result.selected_route_ids == [1, 2, 3]


def test_mespprc_vrp_exports_pricing_first_api() -> None:
    assert mespprc_vrp.Phase2DPPricingSolver is Phase2DPPricingSolver
    assert mespprc_vrp.Phase2IPPricingSolver is Phase2IPPricingSolver
    assert mespprc_vrp.Phase2DPSolver is Phase2DPPricingSolver
    assert mespprc_vrp.Phase2IPSolver is Phase2IPPricingSolver
    assert not hasattr(mespprc_vrp, "Phase2CoveringDPSolver")
    assert not hasattr(mespprc_vrp, "generate_benchmark_instance")
