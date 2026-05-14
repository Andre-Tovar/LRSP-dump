"""
ctypes binding for the mespprc_native C library.

The binding is intentionally low-level: it mirrors the C ABI declared in
`mespprc_native/include/mespprc.h` 1:1 and exposes function objects with
explicit `argtypes` / `restype`. Higher-level Python conveniences live in
`adapters.py`; nothing user-facing should import from this module directly.

A startup self-check (`_assert_layout_matches`) compares the size of every
struct mirrored here to the values reported by `mespprc_struct_sizes`. If the
binding ever drifts out of sync with the C side, the check fails loudly at
import time rather than silently corrupting memory mid-solve.
"""

from __future__ import annotations

import ctypes
import os
import sys
from ctypes import (
    POINTER,
    Structure,
    byref,
    c_char_p,
    c_double,
    c_int,
    c_uint64,
    c_void_p,
)
from pathlib import Path


# ---------- Library location ----------

_PACKAGE_DIR = Path(__file__).resolve().parent  # mespprc_native/


def _candidate_library_paths() -> list[Path]:
    candidates: list[Path] = []
    env_path = os.environ.get("MESPPRC_NATIVE_LIB")
    if env_path:
        candidates.append(Path(env_path))

    if sys.platform.startswith("win"):
        names = ["mespprc_native.dll"]
    elif sys.platform == "darwin":
        names = ["libmespprc_native.dylib", "mespprc_native.dylib"]
    else:
        names = ["libmespprc_native.so", "mespprc_native.so"]

    search_dirs = [
        _PACKAGE_DIR / "build" / "bin",
        _PACKAGE_DIR / "build" / "lib",
        _PACKAGE_DIR,
    ]
    for directory in search_dirs:
        for name in names:
            candidates.append(directory / name)
    return candidates


def _load_library() -> ctypes.CDLL:
    errors: list[str] = []
    for candidate in _candidate_library_paths():
        if not candidate or not candidate.exists():
            continue
        try:
            return ctypes.CDLL(str(candidate))
        except OSError as exc:  # pragma: no cover
            errors.append(f"{candidate}: {exc}")
    pretty = "\n  ".join(errors) if errors else "no library candidate found"
    raise OSError(
        "Could not load mespprc_native shared library. Build it via "
        "`mespprc_native\\scripts\\build.bat` (Windows) or your CMake of choice. "
        f"Tried:\n  {pretty}"
    )


# ---------- Status codes ----------

MESPPRC_OK = 0
MESPPRC_ERR_NOMEM = 1
MESPPRC_ERR_INVALID_ARG = 2
MESPPRC_ERR_NOT_IMPLEMENTED = 3
MESPPRC_ERR_INSTANCE_INVALID = 4
MESPPRC_ERR_BUFFER_TOO_SMALL = 5
MESPPRC_ERR_DUPLICATE = 6
MESPPRC_ERR_NOT_FINALIZED = 7

_STATUS_NAMES = {
    MESPPRC_OK: "OK",
    MESPPRC_ERR_NOMEM: "NOMEM",
    MESPPRC_ERR_INVALID_ARG: "INVALID_ARG",
    MESPPRC_ERR_NOT_IMPLEMENTED: "NOT_IMPLEMENTED",
    MESPPRC_ERR_INSTANCE_INVALID: "INSTANCE_INVALID",
    MESPPRC_ERR_BUFFER_TOO_SMALL: "BUFFER_TOO_SMALL",
    MESPPRC_ERR_DUPLICATE: "DUPLICATE",
    MESPPRC_ERR_NOT_FINALIZED: "NOT_FINALIZED",
}


def status_name(status: int) -> str:
    return _STATUS_NAMES.get(int(status), f"UNKNOWN({status})")


class MesspprcError(RuntimeError):
    """Raised when a C entry point returns a non-OK status."""

    def __init__(self, status: int, where: str) -> None:
        super().__init__(f"{where}: {status_name(status)} (status={status})")
        self.status = int(status)
        self.where = where


def _check(status: int, where: str) -> None:
    if status != MESPPRC_OK:
        raise MesspprcError(status, where)


# ---------- Node types ----------

NODE_TYPE_SOURCE = 0
NODE_TYPE_CUSTOMER = 1
NODE_TYPE_SINK = 2


# ---------- Mirrored structs ----------


class StructSizes(Structure):
    """Mirror of `mespprc_struct_sizes_t`.

    Field order and size MUST match include/mespprc.h. The startup self-check
    enforces this so any drift surfaces immediately.
    """

    _fields_ = [
        ("struct_sizes", c_uint64),
        ("status_t", c_uint64),
        ("pointer", c_uint64),
        ("double_", c_uint64),
        ("int_", c_uint64),
    ]


# ---------- Library object + function bindings ----------

_lib = _load_library()


def _bind(name: str, restype, argtypes):
    fn = getattr(_lib, name)
    fn.restype = restype
    fn.argtypes = argtypes
    return fn


# Layout self-check + version
mespprc_struct_sizes = _bind("mespprc_struct_sizes", None, [POINTER(StructSizes)])
mespprc_version = _bind("mespprc_version", c_char_p, [])

# Instance lifecycle
mespprc_instance_create = _bind(
    "mespprc_instance_create",
    c_int,
    [c_int, c_int, c_int, c_int, POINTER(c_void_p)],
)
mespprc_instance_destroy = _bind("mespprc_instance_destroy", None, [c_void_p])
mespprc_instance_set_local_limits = _bind(
    "mespprc_instance_set_local_limits", c_int, [c_void_p, POINTER(c_double), c_int]
)
mespprc_instance_set_global_limits = _bind(
    "mespprc_instance_set_global_limits", c_int, [c_void_p, POINTER(c_double), c_int]
)
mespprc_instance_add_node = _bind(
    "mespprc_instance_add_node", c_int, [c_void_p, c_int, c_int]
)
mespprc_instance_add_arc = _bind(
    "mespprc_instance_add_arc",
    c_int,
    [c_void_p, c_int, c_int, c_double, POINTER(c_double), POINTER(c_double)],
)
mespprc_instance_finalize = _bind("mespprc_instance_finalize", c_int, [c_void_p])

# Instance accessors
mespprc_instance_node_count = _bind("mespprc_instance_node_count", c_int, [c_void_p])
mespprc_instance_arc_count = _bind("mespprc_instance_arc_count", c_int, [c_void_p])
mespprc_instance_local_dim = _bind("mespprc_instance_local_dim", c_int, [c_void_p])
mespprc_instance_global_dim = _bind("mespprc_instance_global_dim", c_int, [c_void_p])
mespprc_instance_source_id = _bind("mespprc_instance_source_id", c_int, [c_void_p])
mespprc_instance_sink_id = _bind("mespprc_instance_sink_id", c_int, [c_void_p])
mespprc_instance_is_finalized = _bind(
    "mespprc_instance_is_finalized", c_int, [c_void_p]
)
mespprc_instance_get_arc = _bind(
    "mespprc_instance_get_arc",
    c_int,
    [
        c_void_p,
        c_int,
        POINTER(c_int),
        POINTER(c_int),
        POINTER(c_double),
        POINTER(c_double),
        POINTER(c_double),
    ],
)

# Phase 1 (Phase B implementation lives in src/phase1.c)
mespprc_solve_phase1 = _bind(
    "mespprc_solve_phase1", c_int, [c_void_p, c_int, POINTER(c_void_p)]
)
mespprc_phase1_result_destroy = _bind(
    "mespprc_phase1_result_destroy", None, [c_void_p]
)
mespprc_phase1_route_count = _bind(
    "mespprc_phase1_route_count", c_int, [c_void_p]
)
mespprc_phase1_num_customers = _bind(
    "mespprc_phase1_num_customers", c_int, [c_void_p]
)
mespprc_phase1_local_dim = _bind(
    "mespprc_phase1_local_dim", c_int, [c_void_p]
)
mespprc_phase1_global_dim = _bind(
    "mespprc_phase1_global_dim", c_int, [c_void_p]
)
mespprc_phase1_path_length = _bind(
    "mespprc_phase1_path_length", c_int, [c_void_p, c_int]
)
mespprc_phase1_route_cost = _bind(
    "mespprc_phase1_route_cost", c_int, [c_void_p, c_int, POINTER(c_double)]
)
mespprc_phase1_route_first_customer = _bind(
    "mespprc_phase1_route_first_customer", c_int, [c_void_p, c_int, POINTER(c_int)]
)
mespprc_phase1_route_path = _bind(
    "mespprc_phase1_route_path", c_int, [c_void_p, c_int, POINTER(c_int), c_int]
)
mespprc_phase1_route_local_resources = _bind(
    "mespprc_phase1_route_local_resources",
    c_int, [c_void_p, c_int, POINTER(c_double), c_int]
)
mespprc_phase1_route_global_resources = _bind(
    "mespprc_phase1_route_global_resources",
    c_int, [c_void_p, c_int, POINTER(c_double), c_int]
)
mespprc_phase1_route_customer_state_signature = _bind(
    "mespprc_phase1_route_customer_state_signature",
    c_int, [c_void_p, c_int, POINTER(c_int), c_int]
)

mespprc_solve_phase2_dp = _bind(
    "mespprc_solve_phase2_dp", c_int, [c_void_p, c_void_p, POINTER(c_void_p)]
)
mespprc_phase2_dp_result_destroy = _bind(
    "mespprc_phase2_dp_result_destroy", None, [c_void_p]
)
mespprc_phase2_dp_status = _bind(
    "mespprc_phase2_dp_status", c_int, [c_void_p])
mespprc_phase2_dp_infeasibility_reason = _bind(
    "mespprc_phase2_dp_infeasibility_reason", c_int, [c_void_p])
mespprc_phase2_dp_is_feasible = _bind(
    "mespprc_phase2_dp_is_feasible", c_int, [c_void_p])
mespprc_phase2_dp_coverage_complete = _bind(
    "mespprc_phase2_dp_coverage_complete", c_int, [c_void_p])
mespprc_phase2_dp_total_cost = _bind(
    "mespprc_phase2_dp_total_cost", c_int, [c_void_p, POINTER(c_double)])
mespprc_phase2_dp_selected_route_count = _bind(
    "mespprc_phase2_dp_selected_route_count", c_int, [c_void_p])
mespprc_phase2_dp_selected_routes = _bind(
    "mespprc_phase2_dp_selected_routes", c_int, [c_void_p, POINTER(c_int), c_int])

PHASE2_STATUS_OPTIMAL = 0
PHASE2_STATUS_INFEASIBLE = 1
PHASE2_INFEAS_NONE = 0
PHASE2_INFEAS_ROUTE_SET = 1
PHASE2_INFEAS_GLOBAL_LIMITS = 2

mespprc_solve_phase2_ip = _bind(
    "mespprc_solve_phase2_ip", c_int, [c_void_p, c_void_p, POINTER(c_void_p)]
)
mespprc_phase2_ip_result_destroy = _bind(
    "mespprc_phase2_ip_result_destroy", None, [c_void_p]
)
mespprc_phase2_ip_status = _bind(
    "mespprc_phase2_ip_status", c_int, [c_void_p])
mespprc_phase2_ip_infeasibility_reason = _bind(
    "mespprc_phase2_ip_infeasibility_reason", c_int, [c_void_p])
mespprc_phase2_ip_is_feasible = _bind(
    "mespprc_phase2_ip_is_feasible", c_int, [c_void_p])
mespprc_phase2_ip_coverage_complete = _bind(
    "mespprc_phase2_ip_coverage_complete", c_int, [c_void_p])
mespprc_phase2_ip_total_cost = _bind(
    "mespprc_phase2_ip_total_cost", c_int, [c_void_p, POINTER(c_double)])
mespprc_phase2_ip_selected_route_count = _bind(
    "mespprc_phase2_ip_selected_route_count", c_int, [c_void_p])
mespprc_phase2_ip_selected_routes = _bind(
    "mespprc_phase2_ip_selected_routes", c_int, [c_void_p, POINTER(c_int), c_int])
mespprc_phase2_ip_original_route_count = _bind(
    "mespprc_phase2_ip_original_route_count", c_int, [c_void_p])
mespprc_phase2_ip_reduced_route_count = _bind(
    "mespprc_phase2_ip_reduced_route_count", c_int, [c_void_p])


# ---------- Layout self-check ----------


def _assert_layout_matches() -> None:
    sizes = StructSizes()
    mespprc_struct_sizes(byref(sizes))

    expected = {
        "struct_sizes": ctypes.sizeof(StructSizes),
        "pointer": ctypes.sizeof(c_void_p),
        "double_": ctypes.sizeof(c_double),
        "int_": ctypes.sizeof(c_int),
    }
    actual = {
        "struct_sizes": int(sizes.struct_sizes),
        "pointer": int(sizes.pointer),
        "double_": int(sizes.double_),
        "int_": int(sizes.int_),
    }
    mismatches = {k: (actual[k], expected[k]) for k in expected if actual[k] != expected[k]}
    if mismatches:
        details = ", ".join(
            f"{k}: C={a}, py={e}" for k, (a, e) in mismatches.items()
        )
        raise RuntimeError(
            "mespprc_native ABI layout mismatch between C and Python ctypes. "
            f"Differences: {details}. Rebuild the library and the binding together."
        )


_assert_layout_matches()


def version() -> str:
    """Return the C library's reported version string."""

    raw = mespprc_version()
    return raw.decode("utf-8") if raw else ""
