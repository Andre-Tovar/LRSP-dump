from pathlib import Path

import mespprc
import ARCHIVED.mespprc_c as mespprc_c


def build_small_instance(module) -> object:
    instance = module.MESPPRCInstance(
        local_limits=[30.0],
        global_limits=[60.0],
    )

    instance.add_node(module.Node(0, module.NodeType.SOURCE))
    instance.add_node(module.Node(1, module.NodeType.CUSTOMER))
    instance.add_node(module.Node(2, module.NodeType.CUSTOMER))
    instance.add_node(module.Node(3, module.NodeType.SINK))

    instance.add_arc(module.Arc(0, 1, cost=10.0, local_res=[30.0], global_res=[30.0]))
    instance.add_arc(module.Arc(0, 2, cost=7.0, local_res=[30.0], global_res=[30.0]))
    instance.add_arc(module.Arc(1, 3, cost=0.0, local_res=[0.0], global_res=[0.0]))
    instance.add_arc(module.Arc(2, 3, cost=0.0, local_res=[0.0], global_res=[0.0]))

    return instance


def test_mespprc_c_exports_reference_surface() -> None:
    missing = [name for name in mespprc.__all__ if not hasattr(mespprc_c, name)]
    assert not missing
    assert hasattr(mespprc_c, "C_BINDINGS_AVAILABLE")
    assert hasattr(mespprc_c, "C_BINDINGS_STATUS")


def test_mespprc_c_contains_transferable_native_port_files() -> None:
    root = Path(__file__).resolve().parents[1] / "mespprc_c"
    expected = {
        "__init__.py",
        "README.md",
        "pyproject.toml",
        "setup.py",
        "instance.py",
        "label.py",
        "route.py",
        "phase1.py",
        "phase2_dp.py",
        "phase2_ip.py",
        "instance_generator.py",
        "instance.c",
        "label.c",
        "route.c",
        "phase1.c",
        "phase2_dp.c",
        "phase2_ip.c",
        "instance_generator.c",
    }
    present = {path.name for path in root.iterdir() if path.is_file()}
    assert expected.issubset(present)
    assert "_instance_test.c" not in present


def test_mespprc_c_generator_parity_matches_reference() -> None:
    config = mespprc.GeneratorConfig(num_customers=5, seed=21)

    generated_py = mespprc.generate_instance(config)
    generated_c = mespprc_c.generate_instance(config)

    assert generated_py.coordinates == generated_c.coordinates
    assert generated_py.node_zones == generated_c.node_zones
    assert generated_py.arc_kinds == generated_c.arc_kinds
    assert generated_py.instance.local_limits == generated_c.instance.local_limits
    assert generated_py.instance.global_limits == generated_c.instance.global_limits


def test_mespprc_c_phase1_and_phase2_match_reference_on_small_instance() -> None:
    instance_py = build_small_instance(mespprc)
    instance_c = build_small_instance(mespprc_c)

    phase1_py = mespprc.Phase1Solver(instance_py).solve()
    phase1_c = mespprc_c.Phase1Solver(instance_c).solve()

    assert [route.path for route in phase1_py.feasible_routes] == [
        route.path for route in phase1_c.feasible_routes
    ]
    assert [route.cost for route in phase1_py.feasible_routes] == [
        route.cost for route in phase1_c.feasible_routes
    ]

    phase2_dp_py = mespprc.Phase2DPSolver(instance_py).solve(phase1_py.feasible_routes)
    phase2_dp_c = mespprc_c.Phase2DPSolver(instance_c).solve(phase1_c.feasible_routes)
    assert phase2_dp_py.status == phase2_dp_c.status
    assert phase2_dp_py.total_cost == phase2_dp_c.total_cost
    assert phase2_dp_py.selected_route_ids == phase2_dp_c.selected_route_ids

    phase2_ip_py = mespprc.Phase2IPSolver(instance_py).solve(
        phase1_py.feasible_routes,
        collect_diagnostics=False,
    )
    phase2_ip_c = mespprc_c.Phase2IPSolver(instance_c).solve(
        phase1_c.feasible_routes,
        collect_diagnostics=False,
    )
    assert phase2_ip_py.status == phase2_ip_c.status
    assert phase2_ip_py.total_cost == phase2_ip_c.total_cost
    assert phase2_ip_py.selected_route_ids == phase2_ip_c.selected_route_ids
