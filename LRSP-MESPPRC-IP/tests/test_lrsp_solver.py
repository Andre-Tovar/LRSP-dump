from types import SimpleNamespace

import lrsp_solver.pricing_adapter as pricing_adapter_module
from lrsp_solver import (
    AkcaLRSPInstance,
    BackendPricingResult,
    LRSPBranchAndPriceSkeleton,
    LRSPCustomer,
    LRSPFacility,
    LRSPSolver,
    MasterDuals,
    NodeConstraints,
    PricingColumnCandidate,
    benchmark_dp_vs_ip,
    build_akca_style_instance,
    build_facility_pricing_instance,
)
from lrsp_solver.lrsp_column_generation import ColumnGenerationConfig
from lrsp_solver.lrsp_master import MasterModelConfig, RestrictedMasterProblem
from mespprc_lrsp import Phase1Result, Route


def build_small_lrsp_instance() -> AkcaLRSPInstance:
    return AkcaLRSPInstance(
        name="tiny-lrsp",
        reference_group="synthetic",
        marker="x",
        customer_count=3,
        vehicle_capacity_variant="v1",
        time_limit_variant="t1",
        customers=[
            LRSPCustomer(1, 1.0, 0.0, 4.0),
            LRSPCustomer(2, 2.0, 0.0, 4.0),
            LRSPCustomer(3, 24.0, 0.0, 4.0),
        ],
        facilities=[
            LRSPFacility(1, 0.0, 0.0, 100.0, 20.0),
            LRSPFacility(2, 25.0, 0.0, 100.0, 20.0),
        ],
        vehicle_capacity=8.0,
        vehicle_time_limit=30.0,
        vehicle_fixed_cost=10.0,
        vehicle_operating_cost=1.0,
        reconstruction_notes=["synthetic unit test instance"],
    )


def test_build_akca_style_instance_matches_dissertation_parameters() -> None:
    instance = build_akca_style_instance("p01", "f", 25, "v1", "t1")

    assert instance.name == "p01-f25-v1t1"
    assert len(instance.customers) == 25
    assert len(instance.facilities) == 5
    assert instance.vehicle_capacity == 50.0
    assert instance.vehicle_time_limit == 140.0
    assert [facility.opening_cost for facility in instance.facilities] == [
        1615.0,
        1845.0,
        1781.0,
        1975.0,
        1753.0,
    ]
    assert [(facility.x, facility.y) for facility in instance.facilities] == [
        (20.0, 20.0),
        (30.0, 40.0),
        (15.0, 40.0),
        (45.0, 40.0),
        (55.0, 50.0),
    ]
    assert instance.customers[0].customer_id == 1
    assert instance.customers[-1].customer_id == 25


def test_lrsp_root_column_generation_matches_between_dp_and_ip_pricing() -> None:
    instance = build_small_lrsp_instance()

    dp_result = LRSPSolver(pricing_phase2="dp", max_iterations=5).solve_root_node(instance)
    ip_result = LRSPSolver(pricing_phase2="ip", max_iterations=5).solve_root_node(instance)

    assert dp_result.integer_objective is not None
    assert ip_result.integer_objective is not None
    assert abs(dp_result.integer_objective - ip_result.integer_objective) <= 1e-6
    assert dp_result.integer_objective == 226.0
    assert dp_result.master_iterations >= 1
    assert ip_result.master_iterations >= 1
    assert any(len(column.covered_customers) > 1 for column in dp_result.column_pool)
    assert any(len(column.covered_customers) > 1 for column in ip_result.column_pool)


def test_lrsp_benchmark_harness_runs_on_small_instance() -> None:
    records = benchmark_dp_vs_ip([build_small_lrsp_instance()])

    assert len(records) == 2
    assert {record.pricing_phase2 for record in records} == {"dp", "ip"}
    assert all(record.integer_objective is not None for record in records)


def test_build_facility_pricing_instance_respects_branching_forbidden_assignments() -> None:
    instance = build_small_lrsp_instance()
    facility = instance.facilities[0]
    duals = MasterDuals(
        coverage_duals={1: 0.0, 2: 0.0, 3: 0.0},
        facility_capacity_duals={1: 0.0, 2: 0.0},
        facility_linking_duals={1: 0.0, 2: 0.0},
        required_assignment_duals={},
        forbidden_assignment_duals={},
    )
    constraints = NodeConstraints.root()
    constraints.forbid_customer_at_facility(1, facility.facility_id)

    build = build_facility_pricing_instance(
        instance,
        facility,
        duals,
        node_constraints=constraints,
    )

    assert 1 not in build.pricing_instance.customers()
    assert set(build.pricing_instance.customers()) == {2, 3}


def test_build_facility_pricing_instance_uses_facility_customer_link_duals() -> None:
    instance = build_small_lrsp_instance()
    facility = instance.facilities[0]
    duals = MasterDuals(
        coverage_duals={1: 0.0, 2: 0.0, 3: 0.0},
        facility_capacity_duals={1: 0.0, 2: 0.0},
        facility_linking_duals={1: 0.0, 2: 0.0},
        required_assignment_duals={},
        forbidden_assignment_duals={},
        facility_customer_link_duals={(1, facility.facility_id): 3.5},
    )

    build = build_facility_pricing_instance(instance, facility, duals)

    arc = build.pricing_instance.get_arc(0, 1)
    assert arc.cost == -2.5


def test_facility_pricing_adapter_calls_mespprc_lrsp_phase1_and_phase2(monkeypatch) -> None:
    instance = build_small_lrsp_instance()
    facility = instance.facilities[0]
    duals = MasterDuals(
        coverage_duals={1: 0.0, 2: 0.0, 3: 0.0},
        facility_capacity_duals={1: 0.0, 2: 0.0},
        facility_linking_duals={1: 0.0, 2: 0.0},
        required_assignment_duals={},
        forbidden_assignment_duals={},
    )
    calls: list[str] = []
    route = Route(
        route_id=1,
        cost=-12.0,
        local_resources=[4.0, 2.0],
        global_resources=[2.0],
        covered_customers={1},
        path=[0, 1, 4],
        first_customer_in_route=1,
        customer_state_signature=[1],
    )

    class FakePhase1Solver:
        def __init__(self, pricing_instance, label_limit=None):
            del pricing_instance, label_limit
            calls.append("phase1_init")

        def solve(self):
            calls.append("phase1_solve")
            return Phase1Result(
                labels_at_sink=[],
                feasible_routes=[route],
                negative_cost_routes=[route],
                best_negative_route=route,
                has_negative_route=True,
                best_cost=-12.0,
                all_label_buckets={},
            )

    class FakePhase2Solver:
        def __init__(self, pricing_instance):
            del pricing_instance
            calls.append("phase2_init")

        def solve(self, phase1_result):
            del phase1_result
            calls.append("phase2_solve")
            return SimpleNamespace(
                status="optimal",
                total_cost=-12.0,
                selected_routes=[route],
            )

    monkeypatch.setattr(pricing_adapter_module, "LRSPPhase1Solver", FakePhase1Solver)
    monkeypatch.setattr(
        pricing_adapter_module,
        "LRSPPhase2DPPricingSolver",
        FakePhase2Solver,
    )

    adapter = pricing_adapter_module.FacilityPricingAdapter(
        pricing_adapter_module.PricingEngineConfig(phase2_method="dp")
    )
    result = adapter.solve_facility(
        instance,
        facility,
        duals,
        column_id_start=1,
        iteration_index=0,
    )

    assert calls == ["phase1_init", "phase1_solve", "phase2_init", "phase2_solve"]
    assert result.has_improving_pairing
    assert len(result.generated_columns) == 1


def test_facility_pricing_adapter_accepts_custom_backend() -> None:
    instance = build_small_lrsp_instance()
    facility = instance.facilities[0]
    duals = MasterDuals(
        coverage_duals={1: 0.0, 2: 0.0, 3: 0.0},
        facility_capacity_duals={1: 0.0, 2: 0.0},
        facility_linking_duals={1: 0.0, 2: 0.0},
        required_assignment_duals={},
        forbidden_assignment_duals={},
    )
    seen_facilities: list[int] = []
    route = Route(
        route_id=99,
        cost=-5.0,
        local_resources=[4.0, 2.0],
        global_resources=[2.0],
        covered_customers={1},
        path=[0, 1, 4],
        first_customer_in_route=1,
        customer_state_signature=[1],
    )

    class FakeBackend:
        backend_name = "fake-backend"

        def solve(self, problem, *, config):
            del config
            seen_facilities.append(problem.facility.facility_id)
            return BackendPricingResult(
                phase1_time=0.0,
                phase2_time=0.0,
                route_count=1,
                negative_route_count=1,
                generated_columns=[
                    PricingColumnCandidate(
                        routes=(route,),
                        reduced_cost=-5.0,
                        pricing_engine="fake-backend",
                    )
                ],
                best_reduced_cost=-5.0,
                phase1_status="custom_phase1",
                phase2_status="custom_phase2",
                diagnostic_note="fake backend",
            )

    adapter = pricing_adapter_module.FacilityPricingAdapter(
        pricing_adapter_module.PricingEngineConfig(
            phase2_method="dp",
            pricing_backend=FakeBackend(),
        )
    )
    result = adapter.solve_facility(
        instance,
        facility,
        duals,
        column_id_start=1,
        iteration_index=0,
    )

    assert seen_facilities == [facility.facility_id]
    assert result.generated_columns[0].pricing_engine == "fake-backend"
    assert result.phase1_status == "custom_phase1"
    assert result.phase2_status == "custom_phase2"


def test_stronger_master_exposes_facility_customer_link_duals() -> None:
    instance = build_small_lrsp_instance()
    master = RestrictedMasterProblem(
        instance,
        MasterModelConfig(
            use_customer_facility_linking=True,
            use_minimum_open_facilities_bound=True,
        ),
    )

    seed_result = LRSPSolver(max_iterations=1).solve_root_node(instance)
    master.add_columns(seed_result.column_pool)
    solution = master.solve(relax=True, node_constraints=NodeConstraints.root())

    assert solution.duals is not None
    assert len(solution.duals.facility_customer_link_duals) == (
        len(instance.customers) * len(instance.facilities)
    )
    assert solution.minimum_required_open_facilities == 1


def test_branch_and_price_branches_on_fractional_customer_facility_assignment() -> None:
    instance = AkcaLRSPInstance(
        name="branch-test",
        reference_group="synthetic",
        marker="b",
        customer_count=1,
        vehicle_capacity_variant="v1",
        time_limit_variant="t1",
        customers=[LRSPCustomer(1, 5.0, 0.0, 2.0)],
        facilities=[
            LRSPFacility(1, 0.0, 0.0, 0.0, 5.0),
            LRSPFacility(2, 10.0, 0.0, 0.0, 5.0),
        ],
        vehicle_capacity=5.0,
        vehicle_time_limit=20.0,
        vehicle_fixed_cost=0.0,
        vehicle_operating_cost=1.0,
        reconstruction_notes=["branching test"],
    )

    brancher = LRSPBranchAndPriceSkeleton(
        ColumnGenerationConfig(pricing_phase2="dp", max_iterations=3),
        max_nodes=5,
    )
    result = brancher.solve(instance)

    assert result.root_result is not None
    assert result.processed_node_count >= 1
    assert result.incumbent_result is not None
