from mespprc import (
    Arc,
    Label,
    MESPPRCInstance,
    Node,
    NodeType,
    PERM_UNREACHABLE,
    Phase1Solver,
    REACHABLE,
    TEMP_UNREACHABLE,
)


def build_transition_instance() -> MESPPRCInstance:
    instance = MESPPRCInstance(local_limits=[6.0], global_limits=[10.0])

    instance.add_node(Node(0, NodeType.SOURCE))
    instance.add_node(Node(1, NodeType.CUSTOMER))
    instance.add_node(Node(2, NodeType.CUSTOMER))
    instance.add_node(Node(3, NodeType.CUSTOMER))
    instance.add_node(Node(4, NodeType.SINK))

    instance.add_arc(Arc(0, 1, cost=-3.0, local_res=[2.0], global_res=[2.0]))
    instance.add_arc(Arc(1, 2, cost=-4.0, local_res=[2.0], global_res=[2.0]))
    instance.add_arc(Arc(1, 4, cost=0.0, local_res=[2.0], global_res=[2.0]))
    instance.add_arc(Arc(2, 4, cost=0.0, local_res=[2.0], global_res=[2.0]))

    return instance


def test_phase1_state_transitions_support_temp_and_perm_states() -> None:
    instance = build_transition_instance()
    solver = Phase1Solver(instance)

    source = instance.source
    assert source is not None
    initial = solver._initial_state(source)
    assert initial.label.unreachable_vector == [
        REACHABLE,
        TEMP_UNREACHABLE,
        PERM_UNREACHABLE,
    ]
    assert initial.label.unreachable_count == 1

    after_first = solver._try_extend(initial, 1)
    assert after_first is not None
    assert after_first.label.unreachable_vector == [
        1,
        REACHABLE,
        PERM_UNREACHABLE,
    ]
    assert after_first.label.unreachable_count == 2

    after_second = solver._try_extend(after_first, 2)
    assert after_second is not None
    assert after_second.label.unreachable_vector == [
        1,
        2,
        PERM_UNREACHABLE,
    ]
    assert after_second.label.unreachable_count == 3


def test_phase1_dominance_respects_richer_customer_states() -> None:
    solver = Phase1Solver(build_transition_instance())

    stronger = Label(
        current_node=1,
        cost=5.0,
        resources=[2.0],
        unreachable_vector=[1, REACHABLE, PERM_UNREACHABLE],
    )
    weaker = Label(
        current_node=1,
        cost=5.0,
        resources=[2.0],
        unreachable_vector=[1, TEMP_UNREACHABLE, PERM_UNREACHABLE],
    )

    assert solver._dominates(stronger, weaker)
    assert not solver._dominates(weaker, stronger)


def test_phase1_first_customer_symmetry_breaking_prunes_redundant_permutations() -> None:
    instance = MESPPRCInstance(local_limits=[10.0], global_limits=[10.0])

    instance.add_node(Node(0, NodeType.SOURCE))
    instance.add_node(Node(1, NodeType.CUSTOMER))
    instance.add_node(Node(2, NodeType.CUSTOMER))
    instance.add_node(Node(3, NodeType.SINK))

    instance.add_arc(Arc(0, 1, cost=1.0, local_res=[2.0], global_res=[1.0]))
    instance.add_arc(Arc(0, 2, cost=1.0, local_res=[2.0], global_res=[1.0]))
    instance.add_arc(Arc(1, 2, cost=1.0, local_res=[2.0], global_res=[1.0]))
    instance.add_arc(Arc(2, 1, cost=1.0, local_res=[2.0], global_res=[1.0]))
    instance.add_arc(Arc(1, 3, cost=0.0, local_res=[2.0], global_res=[1.0]))
    instance.add_arc(Arc(2, 3, cost=0.0, local_res=[2.0], global_res=[1.0]))

    result = Phase1Solver(instance).solve()
    paths = [route.path for route in result.feasible_routes]

    assert [0, 1, 2, 3] in paths
    assert [0, 2, 1, 3] not in paths
    assert len(result.feasible_routes) == 3

    two_customer_route = next(
        route for route in result.feasible_routes if route.path == [0, 1, 2, 3]
    )
    assert two_customer_route.first_customer_in_route == 1
