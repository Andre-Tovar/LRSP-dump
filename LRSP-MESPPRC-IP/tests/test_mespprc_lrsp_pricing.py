from mespprc_lrsp import (
    Arc,
    GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING,
    LRSPPhase1Result,
    LRSPPhase1Solver,
    LRSPPhase2DPPricingSolver,
    LRSPPhase2IPPricingSolver,
    MESPPRCInstance,
    NO_IMPROVING_PAIRING_STATUS,
    NO_NEGATIVE_ROUTES_FROM_PHASE1,
    Node,
    NodeType,
    Phase1Result,
    Phase2DPSolver,
    Phase2IPSolver,
    Route,
)
import mespprc_lrsp


def build_phase1_lrsp_instance() -> MESPPRCInstance:
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


def build_phase2_lrsp_instance(
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


def lrsp_routes() -> list[Route]:
    return [
        Route(1, -5.0, [], [3.0], {1}, [0, 1, 5], first_customer_in_route=1),
        Route(2, -4.0, [], [3.0], {2}, [0, 2, 5], first_customer_in_route=2),
        Route(3, -3.0, [], [2.0], {3}, [0, 3, 5], first_customer_in_route=3),
        Route(4, -7.0, [], [6.0], {1, 2}, [0, 1, 2, 5], first_customer_in_route=1),
        Route(5, 2.0, [], [1.0], {4}, [0, 4, 5], first_customer_in_route=4),
    ]


def test_lrsp_phase1_exposes_negative_routes_and_best_route_cost() -> None:
    phase1 = LRSPPhase1Solver(build_phase1_lrsp_instance()).solve()

    assert isinstance(phase1, LRSPPhase1Result)
    assert sorted(route.cost for route in phase1.feasible_routes) == [-3.0, 2.0]
    assert [route.cost for route in phase1.negative_routes] == [-3.0]
    assert phase1.exported_routes == phase1.negative_routes
    assert phase1.best_negative_route is not None
    assert phase1.best_negative_route.cost == -3.0
    assert phase1.has_negative_route
    assert phase1.best_route_cost == -3.0


def test_lrsp_phase2_dp_and_ip_find_best_negative_pairing() -> None:
    instance = build_phase2_lrsp_instance(global_limits=[8.0])

    dp_result = LRSPPhase2DPPricingSolver(instance).solve(lrsp_routes())
    ip_result = LRSPPhase2IPPricingSolver(instance).solve(lrsp_routes())

    assert dp_result.total_cost == -12.0
    assert ip_result.total_cost == -12.0
    assert dp_result.selected_route_ids == [1, 2, 3]
    assert ip_result.selected_route_ids == [1, 2, 3]
    assert dp_result.best_pairing == dp_result.selected_routes
    assert ip_result.best_pairing == ip_result.selected_routes
    assert dp_result.has_negative_pairing
    assert ip_result.has_negative_pairing


def test_lrsp_phase2_reports_no_improving_pairing_when_global_limit_blocks_all_routes() -> None:
    instance = build_phase2_lrsp_instance(global_limits=[2.0])
    routes = [
        Route(1, -5.0, [], [3.0], {1}, [0, 1, 5]),
        Route(2, -4.0, [], [4.0], {2}, [0, 2, 5]),
    ]

    dp_result = LRSPPhase2DPPricingSolver(instance).solve(routes)
    ip_result = LRSPPhase2IPPricingSolver(instance).solve(routes)

    assert dp_result.status == NO_IMPROVING_PAIRING_STATUS
    assert dp_result.infeasibility_reason == GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING
    assert not dp_result.has_improving_pairing

    assert ip_result.status == NO_IMPROVING_PAIRING_STATUS
    assert ip_result.infeasibility_reason == GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING
    assert not ip_result.has_improving_pairing


def test_lrsp_phase2_handles_empty_negative_pool() -> None:
    phase1 = Phase1Result(
        labels_at_sink=[],
        feasible_routes=[Route(1, 2.0, [], [], {1}, [0, 1, 5])],
        negative_cost_routes=[],
        best_negative_route=None,
        has_negative_route=False,
        best_cost=2.0,
        all_label_buckets={},
    )
    instance = build_phase2_lrsp_instance()

    dp_result = LRSPPhase2DPPricingSolver(instance).solve(phase1)
    ip_result = LRSPPhase2IPPricingSolver(instance).solve(phase1)

    assert dp_result.infeasibility_reason == NO_NEGATIVE_ROUTES_FROM_PHASE1
    assert ip_result.infeasibility_reason == NO_NEGATIVE_ROUTES_FROM_PHASE1


def test_lrsp_phase2_dp_diagnostics_report_removed_route_ids_after_reduction() -> None:
    instance = build_phase2_lrsp_instance(global_limits=[10.0])
    routes = [
        Route(1, -5.0, [], [1.0], {1}, [0, 1, 5], first_customer_in_route=1),
        Route(2, -4.0, [], [1.0], {1}, [0, 1, 5], first_customer_in_route=1),
        Route(3, -3.0, [], [1.0], {2}, [0, 2, 5], first_customer_in_route=2),
        Route(4, -2.0, [], [1.0], {3}, [0, 3, 5], first_customer_in_route=3),
        Route(5, -1.0, [], [1.0], {4}, [0, 4, 5], first_customer_in_route=4),
    ]

    result = LRSPPhase2DPPricingSolver(instance).solve(routes)

    assert result.diagnostics is not None
    assert result.diagnostics.removed_route_ids == [2]
    assert result.diagnostics.reduction_records[0].kept_route_id == 1
    assert result.diagnostics.reduction_records[0].removed_route_id == 2


def test_mespprc_lrsp_exports_lrsp_primary_names_and_pricing_aliases() -> None:
    assert mespprc_lrsp.LRSPPhase1Solver is LRSPPhase1Solver
    assert mespprc_lrsp.LRSPPhase2DPPricingSolver is LRSPPhase2DPPricingSolver
    assert mespprc_lrsp.LRSPPhase2IPPricingSolver is LRSPPhase2IPPricingSolver
    assert mespprc_lrsp.Phase2DPSolver is Phase2DPSolver is LRSPPhase2DPPricingSolver
    assert mespprc_lrsp.Phase2IPSolver is Phase2IPSolver is LRSPPhase2IPPricingSolver
    assert not hasattr(mespprc_lrsp, "generate_benchmark_instance")
