"""Tests for the LRSP instance generator and library."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from lrsp_solver import (
    GeneratorConfig,
    LRSPGeneratorError,
    REGIMES,
    generate_instance,
    instance_from_dict,
    instance_to_dict,
    iter_database_instances,
    list_database_instances,
    load_lrsp_instance,
    read_lrsp_json,
    regime_config,
    validate_feasibility,
    write_akca_txt,
    write_lrsp_json,
)


# ---------------------------------------------------------------------------
# Generator basics
# ---------------------------------------------------------------------------


def test_generator_produces_valid_lrsp_instance():
    cfg = regime_config("moderate", num_customers=8, num_facilities=2, seed=42)
    inst = generate_instance(cfg)
    assert len(inst.customers) == 8
    assert len(inst.facilities) == 2
    # Critical: it must be a real LRSP instance, not LRP.
    assert inst.vehicle_time_limit is not None
    assert inst.vehicle_time_limit > 0
    # Customer ids 1..N, facility ids N+1..N+F
    assert [c.id for c in inst.customers] == list(range(1, 9))
    assert [f.id for f in inst.facilities] == [9, 10]


def test_generator_is_deterministic_for_same_seed():
    cfg = regime_config("tight", num_customers=10, num_facilities=3, seed=7)
    a = generate_instance(cfg)
    b = generate_instance(cfg)
    assert a.name == b.name
    assert a.vehicle_capacity == b.vehicle_capacity
    assert a.vehicle_time_limit == b.vehicle_time_limit
    assert [(c.id, c.x, c.y, c.demand) for c in a.customers] == \
           [(c.id, c.x, c.y, c.demand) for c in b.customers]
    assert [(f.id, f.x, f.y, f.opening_cost, f.capacity) for f in a.facilities] == \
           [(f.id, f.x, f.y, f.opening_cost, f.capacity) for f in b.facilities]


def test_generator_different_seeds_differ():
    a = generate_instance(regime_config("moderate", num_customers=10, num_facilities=3, seed=1))
    b = generate_instance(regime_config("moderate", num_customers=10, num_facilities=3, seed=2))
    # At least the customer demand list should differ.
    da = [c.demand for c in a.customers]
    db = [c.demand for c in b.customers]
    assert da != db


def test_tighter_regimes_produce_smaller_capacities():
    """Easy ⊃ moderate ⊃ tight in terms of slack."""
    easy = generate_instance(regime_config("easy", num_customers=15, num_facilities=4, seed=0))
    mod  = generate_instance(regime_config("moderate", num_customers=15, num_facilities=4, seed=0))
    tight = generate_instance(regime_config("tight", num_customers=15, num_facilities=4, seed=0))
    # Coordinates / demands must be identical (same seed) — only
    # capacity / time-limit knobs differ between regimes.
    assert easy.customers == mod.customers == tight.customers
    # Tighter regime → smaller vehicle_time_limit.
    assert easy.vehicle_time_limit > mod.vehicle_time_limit > tight.vehicle_time_limit
    # Tighter regime → smaller vehicle_capacity.
    assert easy.vehicle_capacity > mod.vehicle_capacity > tight.vehicle_capacity


def test_generator_rejects_invalid_config():
    with pytest.raises(ValueError):
        GeneratorConfig(num_customers=0, num_facilities=2)
    with pytest.raises(ValueError):
        GeneratorConfig(num_customers=5, num_facilities=2, demand_min=5, demand_max=3)
    with pytest.raises(ValueError):
        GeneratorConfig(num_customers=5, num_facilities=2, vehicle_capacity_factor=0)


# ---------------------------------------------------------------------------
# Feasibility
# ---------------------------------------------------------------------------


def test_validate_feasibility_passes_on_generated_instance():
    inst = generate_instance(
        regime_config("tight", num_customers=20, num_facilities=4, seed=1)
    )
    validate_feasibility(inst)


def test_validate_feasibility_rejects_negative_time_limit():
    inst = generate_instance(
        regime_config("moderate", num_customers=8, num_facilities=2, seed=0)
    )
    bad = type(inst)(
        name=inst.name,
        customers=inst.customers,
        facilities=inst.facilities,
        vehicle_capacity=inst.vehicle_capacity,
        vehicle_fixed_cost=inst.vehicle_fixed_cost,
        vehicle_operating_cost=inst.vehicle_operating_cost,
        vehicle_time_limit=-1.0,
        notes=inst.notes,
    )
    with pytest.raises(LRSPGeneratorError):
        validate_feasibility(bad)


# ---------------------------------------------------------------------------
# Round-trips
# ---------------------------------------------------------------------------


def test_json_roundtrip(tmp_path: Path):
    inst = generate_instance(
        regime_config("moderate", num_customers=10, num_facilities=3, seed=5)
    )
    p = tmp_path / "x.lrsp.json"
    write_lrsp_json(inst, p)
    back = read_lrsp_json(p)
    assert back.name == inst.name
    assert back.vehicle_time_limit == inst.vehicle_time_limit
    assert back.customers == inst.customers
    assert back.facilities == inst.facilities


def test_dict_roundtrip():
    inst = generate_instance(
        regime_config("easy", num_customers=12, num_facilities=3, seed=8)
    )
    d = instance_to_dict(inst)
    back = instance_from_dict(d)
    assert back.name == inst.name
    assert back.customers == inst.customers
    assert back.facilities == inst.facilities


def test_akca_txt_roundtrip_through_load_lrsp_instance(tmp_path: Path):
    inst = generate_instance(
        regime_config("moderate", num_customers=15, num_facilities=4, seed=11)
    )
    p = tmp_path / "x.txt"
    write_akca_txt(inst, p)
    back = load_lrsp_instance(p)
    assert len(back.customers) == len(inst.customers)
    assert len(back.facilities) == len(inst.facilities)
    assert back.vehicle_capacity == inst.vehicle_capacity
    assert back.vehicle_time_limit == inst.vehicle_time_limit
    # Customer round-trip preserves demand and coordinates exactly when
    # the values are integer-valued (which our generator's integer demands
    # always are; coordinates are floats and may differ in last digit).
    assert [c.demand for c in back.customers] == [c.demand for c in inst.customers]


def test_akca_txt_writer_rejects_lrp_instances(tmp_path: Path):
    """Akca .txt requires a finite vehicle_time_limit. The writer must not
    silently emit something the C runner would reject."""
    inst = generate_instance(
        regime_config("easy", num_customers=8, num_facilities=2, seed=0)
    )
    bad = type(inst)(
        name=inst.name,
        customers=inst.customers,
        facilities=inst.facilities,
        vehicle_capacity=inst.vehicle_capacity,
        vehicle_fixed_cost=inst.vehicle_fixed_cost,
        vehicle_operating_cost=inst.vehicle_operating_cost,
        vehicle_time_limit=None,
        notes=inst.notes,
    )
    p = tmp_path / "lrp.txt"
    with pytest.raises(ValueError):
        write_akca_txt(bad, p)


# ---------------------------------------------------------------------------
# Bundled library
# ---------------------------------------------------------------------------


def test_bundled_library_is_nonempty():
    records = list_database_instances()
    assert len(records) > 0


def test_every_bundled_instance_is_real_lrsp():
    """Every bundled instance must carry a finite, positive
    vehicle_time_limit. This test exists to defend the 'we ship LRSP, not
    LRP' invariant."""
    for record, instance in iter_database_instances():
        assert instance.vehicle_time_limit is not None, \
            f"{record.instance_id}: vehicle_time_limit is None (LRP, not LRSP)"
        assert instance.vehicle_time_limit > 0, \
            f"{record.instance_id}: non-positive vehicle_time_limit"


def test_every_bundled_instance_is_feasible():
    """Run validate_feasibility on every bundled instance."""
    for record, instance in iter_database_instances():
        validate_feasibility(instance)


def test_bundled_library_covers_all_regimes_and_sizes():
    records = list_database_instances()
    regimes = {r.regime for r in records}
    sizes = {(r.num_customers, r.num_facilities) for r in records}
    assert regimes == set(REGIMES.keys())
    # We expect at least 5 distinct (n, F) sizes.
    assert len(sizes) >= 5


def test_bundled_txt_companions_load_through_akca_loader():
    """Spot-check three bundled .txt files load through the Akca-format
    loader and agree with their JSON twins."""
    records = [r for r in list_database_instances() if r.txt_path is not None]
    assert records, "no .txt companions in bundled library"
    for record in records[:3]:
        from_json = read_lrsp_json(record.json_path)
        from_txt  = load_lrsp_instance(record.txt_path)
        assert len(from_json.customers) == len(from_txt.customers)
        assert len(from_json.facilities) == len(from_txt.facilities)
        assert from_json.vehicle_time_limit == pytest.approx(
            from_txt.vehicle_time_limit, rel=1e-6, abs=1e-6
        )
