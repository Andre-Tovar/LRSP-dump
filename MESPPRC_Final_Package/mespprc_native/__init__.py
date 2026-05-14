"""
mespprc_native — C-backed solver for the MESPPRC problem.

Public surface (Phase B):
- `version()`             — version string from the C library
- `NativeInstance`        — owning wrapper around a C-side instance handle
- `build_native_instance` — translate a Python MESPPRCInstance to C
- `solve_phase1`          — Phase 1 ESPPRC labeling DP, returns NativeRoute list
- `NativeRoute`           — one Phase 1 route
- `MesspprcError`         — raised when a C entry point returns a non-OK status

Phase 2 entry points are present in the binding but currently return
NOT_IMPLEMENTED. They will be filled in over Phases C-D.
"""

from . import _native
from ._native import MesspprcError, status_name, version
from .adapters import (
    NativeInstance,
    NativePhase2DPResult,
    NativePhase2IPResult,
    NativeReplicateTimings,
    NativeRoute,
    build_native_instance,
    solve_phase1,
    solve_phase2_dp,
    solve_phase2_ip,
    time_phase2_dp_vs_ip,
)

__all__ = [
    "MesspprcError",
    "NativeInstance",
    "NativePhase2DPResult",
    "NativePhase2IPResult",
    "NativeReplicateTimings",
    "NativeRoute",
    "build_native_instance",
    "solve_phase1",
    "solve_phase2_dp",
    "solve_phase2_ip",
    "time_phase2_dp_vs_ip",
    "status_name",
    "version",
]
