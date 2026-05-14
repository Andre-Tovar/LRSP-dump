from mespprc import Arc, MESPPRCInstance, Node, NodeType, Phase1Solver, Phase2DPSolver


def build_small_two_phase_instance() -> MESPPRCInstance:
    instance = MESPPRCInstance(
        local_limits=[30.0],
        global_limits=[90.0],
    )

    instance.add_node(Node(0, NodeType.SOURCE))
    instance.add_node(Node(1, NodeType.CUSTOMER))
    instance.add_node(Node(2, NodeType.CUSTOMER))
    instance.add_node(Node(3, NodeType.CUSTOMER))
    instance.add_node(Node(4, NodeType.SINK))

    instance.add_arc(Arc(0, 1, cost=10.0, local_res=[30.0], global_res=[30.0]))
    instance.add_arc(Arc(0, 2, cost=7.0, local_res=[30.0], global_res=[30.0]))
    instance.add_arc(Arc(0, 3, cost=4.0, local_res=[30.0], global_res=[30.0]))

    instance.add_arc(Arc(1, 4, cost=0.0, local_res=[0.0], global_res=[0.0]))
    instance.add_arc(Arc(2, 4, cost=0.0, local_res=[0.0], global_res=[0.0]))
    instance.add_arc(Arc(3, 4, cost=0.0, local_res=[0.0], global_res=[0.0]))

    return instance


def test_two_phase_solver_finds_the_exact_full_cover() -> None:
    instance = build_small_two_phase_instance()

    phase1 = Phase1Solver(instance).solve()
    assert len(phase1.feasible_routes) == 3
    assert sorted(route.path for route in phase1.feasible_routes) == [
        [0, 1, 4],
        [0, 2, 4],
        [0, 3, 4],
    ]

    phase2 = Phase2DPSolver(instance).solve(phase1.feasible_routes)
    assert phase2.is_feasible
    assert phase2.coverage_complete
    assert phase2.total_cost == 21.0
    assert phase2.selected_route_ids == [1, 2, 3]
    assert sorted(route.path for route in phase2.selected_routes) == [
        [0, 1, 4],
        [0, 2, 4],
        [0, 3, 4],
    ]
    assert phase2.best_label is not None
    assert phase2.best_label.resources == [90.0]
