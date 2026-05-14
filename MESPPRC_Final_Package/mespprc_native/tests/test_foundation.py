"""
Phase A foundation smoke tests.

These tests verify that:
1. The Python ctypes binding loads the C library and the layout self-check passes.
2. A native instance can be constructed from a Python `MESPPRCInstance` and
   round-trips back the same node count, arc count, and per-arc data.
3. Phase 1 / 2 entry points return MESPPRC_ERR_NOT_IMPLEMENTED — the placeholder
   behaviour during Phases A-C, before the algorithmic ports land.
"""

from __future__ import annotations

import sys
from ctypes import POINTER, byref, c_void_p
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mespprc import GeneratorConfig, generate_instance
from mespprc_native import MesspprcError, build_native_instance, version
from mespprc_native import _native as _n


def test_version_string_is_non_empty():
    assert version()
    assert isinstance(version(), str)


def test_layout_self_check_passes_at_import_time():
    # If the binding loaded and reached this test, _assert_layout_matches() ran
    # successfully. Re-run it explicitly so this test fails loudly if anything
    # changes the matching logic later.
    _n._assert_layout_matches()


def test_build_native_instance_round_trips_arcs():
    py_instance = generate_instance(
        GeneratorConfig(num_customers=5, seed=2024)
    )
    py_arc_count = len(py_instance.arcs)
    py_node_count = len(py_instance.nodes)

    with build_native_instance(py_instance) as native:
        assert native.is_finalized()
        assert native.node_count() == py_node_count
        assert native.arc_count() == py_arc_count
        assert native.local_dim() == len(py_instance.local_limits)
        assert native.global_dim() == len(py_instance.global_limits)
        assert native.source_id() == py_instance.source
        assert native.sink_id() == py_instance.sink

        # Spot-check a handful of arcs against the Python source of truth.
        for index in (0, py_arc_count // 2, py_arc_count - 1):
            tail, head, cost, lr, gr = native.get_arc(index)
            py_arc = py_instance.arcs[(tail, head)]
            assert pytest.approx(py_arc.cost, rel=1e-12) == cost
            assert pytest.approx(list(py_arc.local_res), rel=1e-12) == lr
            assert pytest.approx(list(py_arc.global_res), rel=1e-12) == gr


def test_native_instance_close_is_idempotent():
    py_instance = generate_instance(GeneratorConfig(num_customers=3, seed=7))
    native = build_native_instance(py_instance)
    native.close()
    native.close()
    with pytest.raises(RuntimeError):
        native.handle  # access after close


def test_phase1_solver_returns_a_result_handle():
    py_instance = generate_instance(GeneratorConfig(num_customers=3, seed=7))
    with build_native_instance(py_instance) as native:
        out = c_void_p(0)
        status = _n.mespprc_solve_phase1(native.handle, 0, byref(out))
    assert status == _n.MESPPRC_OK
    assert out.value
    _n.mespprc_phase1_result_destroy(out)


def test_phase2_dp_solver_currently_returns_not_implemented():
    py_instance = generate_instance(GeneratorConfig(num_customers=3, seed=7))
    with build_native_instance(py_instance) as native:
        out = c_void_p(0)
        status = _n.mespprc_solve_phase2_dp(native.handle, c_void_p(0), byref(out))
    assert status == _n.MESPPRC_ERR_NOT_IMPLEMENTED


def test_phase2_ip_solver_currently_returns_not_implemented():
    py_instance = generate_instance(GeneratorConfig(num_customers=3, seed=7))
    with build_native_instance(py_instance) as native:
        out = c_void_p(0)
        status = _n.mespprc_solve_phase2_ip(native.handle, c_void_p(0), byref(out))
    assert status == _n.MESPPRC_ERR_NOT_IMPLEMENTED


def test_mesppprc_error_carries_status_and_where():
    err = MesspprcError(_n.MESPPRC_ERR_INVALID_ARG, "test")
    assert err.status == _n.MESPPRC_ERR_INVALID_ARG
    assert err.where == "test"
    assert "INVALID_ARG" in str(err)
