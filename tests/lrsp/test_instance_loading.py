from __future__ import annotations

from pathlib import Path

import pytest

from lrsp_solver import (
    Customer,
    Facility,
    LRSPInstance,
    load_instance_from_data,
    load_instance_from_module,
    synthetic_instance,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
SAMPLE_INSTANCE = (
    REPO_ROOT
    / "Akca Repo"
    / "routingproblems-lrp_dip_instances_samira-96a5850bf5af"
    / "Random_instances"
    / "r5x2_a_1.py"
)


def test_load_instance_from_data_basic_shape():
    instance = load_instance_from_data(
        demand={1: 10, 2: 20},
        nodes_location={1: (0.0, 0.0), 2: (1.0, 1.0), 3: (2.0, 2.0)},
        vehicle_capacity=50,
        fixed_cost={3: 5},
        facility_capacity={3: 100},
        name="tiny",
    )
    assert isinstance(instance, LRSPInstance)
    assert [c.id for c in instance.customers] == [1, 2]
    assert [f.id for f in instance.facilities] == [3]
    assert instance.facilities[0].opening_cost == 5
    assert instance.facilities[0].capacity == 100
    assert instance.vehicle_capacity == 50


def test_load_instance_from_data_string_keys_match_int_keys():
    instance = load_instance_from_data(
        demand={"1": 10, "2": 20},
        nodes_location={"1": [0.0, 0.0], "2": [1.0, 1.0], "3": [2.0, 2.0]},
        vehicle_capacity=50,
        fixed_cost={"3": 5},
        facility_capacity={"3": 100},
        name="tiny_str",
    )
    assert [c.id for c in instance.customers] == [1, 2]
    assert [f.id for f in instance.facilities] == [3]


@pytest.mark.skipif(not SAMPLE_INSTANCE.exists(), reason="sample Akca instance missing")
def test_load_real_akca_instance():
    instance = load_instance_from_module(SAMPLE_INSTANCE)
    assert len(instance.customers) == 5
    assert len(instance.facilities) == 2
    assert instance.vehicle_capacity > 0
    assert all(c.demand > 0 for c in instance.customers)


def test_synthetic_instance_is_self_consistent():
    instance = synthetic_instance(customer_count=4, facility_count=2)
    assert len(instance.customers) == 4
    assert len(instance.facilities) == 2
    assert instance.total_demand() > 0
    assert instance.minimum_required_open_facilities() >= 1
