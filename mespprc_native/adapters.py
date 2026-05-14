"""
High-level adapters that bridge `mespprc.MESPPRCInstance` with the C library.

`build_native_instance(...)` is the canonical bridge: it takes a Python
`MESPPRCInstance` (the same one the existing solver consumes) and returns a
`NativeInstance` context object that owns the C handle and frees it on
`close()` or `__exit__`.

This is the only module higher-level callers should import. `_native.py` is
deliberately untyped at the Python level so this module can layer ergonomic
behaviour on top.
"""

from __future__ import annotations

from contextlib import AbstractContextManager
from ctypes import POINTER, byref, c_double, c_int, c_void_p
from dataclasses import dataclass
from typing import Iterable, List, Sequence

from mespprc.instance import MESPPRCInstance, NodeType

from . import _native as _n


class NativeInstance(AbstractContextManager["NativeInstance"]):
    """
    Owning wrapper around a C-side instance handle.

    Use as a context manager, or call `close()` explicitly. The handle is
    invalid after close.
    """

    def __init__(self, handle: int) -> None:
        self._handle = c_void_p(handle)

    @property
    def handle(self) -> c_void_p:
        if not self._handle:
            raise RuntimeError("NativeInstance has been closed.")
        return self._handle

    def close(self) -> None:
        if self._handle:
            _n.mespprc_instance_destroy(self._handle)
            self._handle = c_void_p(0)

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def __del__(self) -> None:  # pragma: no cover
        self.close()

    # ---- Convenience accessors ----

    def node_count(self) -> int:
        return int(_n.mespprc_instance_node_count(self.handle))

    def arc_count(self) -> int:
        return int(_n.mespprc_instance_arc_count(self.handle))

    def local_dim(self) -> int:
        return int(_n.mespprc_instance_local_dim(self.handle))

    def global_dim(self) -> int:
        return int(_n.mespprc_instance_global_dim(self.handle))

    def source_id(self) -> int:
        return int(_n.mespprc_instance_source_id(self.handle))

    def sink_id(self) -> int:
        return int(_n.mespprc_instance_sink_id(self.handle))

    def is_finalized(self) -> bool:
        return bool(_n.mespprc_instance_is_finalized(self.handle))

    def get_arc(self, index: int) -> tuple[int, int, float, list[float], list[float]]:
        ld = self.local_dim()
        gd = self.global_dim()
        tail = c_int(0)
        head = c_int(0)
        cost = c_double(0.0)
        local_buf = (c_double * max(ld, 1))()
        global_buf = (c_double * max(gd, 1))()
        status = _n.mespprc_instance_get_arc(
            self.handle,
            int(index),
            byref(tail),
            byref(head),
            byref(cost),
            local_buf,
            global_buf,
        )
        _n._check(status, "mespprc_instance_get_arc")
        return (
            int(tail.value),
            int(head.value),
            float(cost.value),
            [float(local_buf[i]) for i in range(ld)],
            [float(global_buf[i]) for i in range(gd)],
        )


def build_native_instance(instance: MESPPRCInstance) -> NativeInstance:
    """
    Build a NativeInstance from a Python `MESPPRCInstance`.

    The translation is the obvious one: each node and each arc is forwarded
    via the C add_* functions, and the instance is finalized before being
    handed back to the caller.

    Caller is responsible for calling `close()` (or using the context-manager
    form). The original Python instance is not retained.
    """

    instance.validate()
    local_dim = len(instance.local_limits)
    global_dim = len(instance.global_limits)
    num_nodes = len(instance.nodes)
    num_arcs = len(instance.arcs)

    handle_holder = c_void_p(0)
    status = _n.mespprc_instance_create(
        c_int(num_nodes),
        c_int(local_dim),
        c_int(global_dim),
        c_int(num_arcs),
        byref(handle_holder),
    )
    _n._check(status, "mespprc_instance_create")
    native = NativeInstance(handle_holder.value or 0)

    try:
        if local_dim > 0:
            local_arr = (c_double * local_dim)(*[float(x) for x in instance.local_limits])
            status = _n.mespprc_instance_set_local_limits(
                native.handle, local_arr, c_int(local_dim)
            )
            _n._check(status, "mespprc_instance_set_local_limits")
        if global_dim > 0:
            global_arr = (c_double * global_dim)(
                *[float(x) for x in instance.global_limits]
            )
            status = _n.mespprc_instance_set_global_limits(
                native.handle, global_arr, c_int(global_dim)
            )
            _n._check(status, "mespprc_instance_set_global_limits")

        # Add the source first, then customers in id order, then the sink.
        # The dense layout produced by `mespprc.MESPPRCInstance` already iterates
        # nodes in this order, so we follow it directly.
        ordered_node_ids = sorted(
            instance.nodes,
            key=lambda nid: (
                _node_order(instance.nodes[nid].node_type),
                nid,
            ),
        )
        for node_id in ordered_node_ids:
            node_type = _native_node_type(instance.nodes[node_id].node_type)
            status = _n.mespprc_instance_add_node(native.handle, c_int(node_id), c_int(node_type))
            _n._check(status, f"mespprc_instance_add_node({node_id})")

        for (tail, head), arc in instance.arcs.items():
            local_arr = _to_c_doubles(arc.local_res, local_dim)
            global_arr = _to_c_doubles(arc.global_res, global_dim)
            status = _n.mespprc_instance_add_arc(
                native.handle,
                c_int(int(tail)),
                c_int(int(head)),
                c_double(float(arc.cost)),
                local_arr,
                global_arr,
            )
            _n._check(status, f"mespprc_instance_add_arc({tail}->{head})")

        status = _n.mespprc_instance_finalize(native.handle)
        _n._check(status, "mespprc_instance_finalize")
    except Exception:
        native.close()
        raise

    return native


def _native_node_type(node_type: NodeType) -> int:
    if node_type == NodeType.SOURCE:
        return _n.NODE_TYPE_SOURCE
    if node_type == NodeType.CUSTOMER:
        return _n.NODE_TYPE_CUSTOMER
    if node_type == NodeType.SINK:
        return _n.NODE_TYPE_SINK
    raise ValueError(f"Unknown node type {node_type!r}")


def _node_order(node_type: NodeType) -> int:
    """Source < customers < sink. Customers tie-break on id."""

    if node_type == NodeType.SOURCE:
        return 0
    if node_type == NodeType.SINK:
        return 2
    return 1


def _to_c_doubles(values: Sequence[float] | Iterable[float], dim: int):
    if dim <= 0:
        return None
    arr = (c_double * dim)()
    for i, v in enumerate(values):
        if i >= dim:
            break
        arr[i] = float(v)
    return arr


# ---------- Phase 1 high-level wrapper ----------


@dataclass(frozen=True, slots=True)
class NativeRoute:
    """One Phase 1 route as read out of the C library."""

    cost: float
    path: tuple[int, ...]
    local_resources: tuple[float, ...]
    global_resources: tuple[float, ...]
    first_customer_in_route: int | None
    customer_state_signature: tuple[int, ...]
    covered_customers: frozenset[int]


def solve_phase1(
    instance: MESPPRCInstance,
    *,
    label_limit: int | None = None,
) -> List[NativeRoute]:
    """
    Run Phase 1 in C and return the routes as Python objects.

    The returned `NativeRoute` list is the same content the Python
    `Phase1Solver.solve().feasible_routes` list would carry, with the same
    ordering and the same per-route metadata. Equivalence is asserted by
    `tests/test_phase1_equivalence.py`.
    """

    native_inst = build_native_instance(instance)
    try:
        out = c_void_p(0)
        status = _n.mespprc_solve_phase1(
            native_inst.handle,
            c_int(0 if label_limit is None or label_limit <= 0 else int(label_limit)),
            byref(out),
        )
        _n._check(status, "mespprc_solve_phase1")
        result_handle = c_void_p(out.value or 0)
        try:
            return _phase1_result_to_routes(result_handle, instance)
        finally:
            if result_handle:
                _n.mespprc_phase1_result_destroy(result_handle)
    finally:
        native_inst.close()


# ---------- Phase 2 DP wrapper ----------


@dataclass(frozen=True, slots=True)
class NativePhase2DPResult:
    """Phase 2 DP outcome translated back to a Python-friendly form."""

    is_feasible: bool
    coverage_complete: bool
    total_cost: float | None
    infeasibility_reason: str | None
    selected_phase1_indices: tuple[int, ...]


def solve_phase2_dp(
    instance: MESPPRCInstance,
    *,
    label_limit: int | None = None,
) -> NativePhase2DPResult:
    """
    Run Phase 1 + Phase 2 DP entirely in C and return the cover.

    The Python `Phase2DPSolver` accepts a list of routes; this wrapper instead
    produces the route pool by calling C Phase 1 directly so we never round-trip
    routes through Python in the timed path.
    """

    native_inst = build_native_instance(instance)
    try:
        # Run Phase 1 in C, keep the result handle alive across the Phase 2 call.
        phase1_handle = c_void_p(0)
        status = _n.mespprc_solve_phase1(
            native_inst.handle,
            c_int(0 if label_limit is None or label_limit <= 0 else int(label_limit)),
            byref(phase1_handle),
        )
        _n._check(status, "mespprc_solve_phase1")
        try:
            return _solve_phase2_dp_with_phase1(native_inst.handle, phase1_handle)
        finally:
            if phase1_handle:
                _n.mespprc_phase1_result_destroy(phase1_handle)
    finally:
        native_inst.close()


_INFEAS_REASON_BY_CODE = {
    _n.PHASE2_INFEAS_NONE: None,
    _n.PHASE2_INFEAS_ROUTE_SET: "generated_routes_cannot_cover_required_customers",
    _n.PHASE2_INFEAS_GLOBAL_LIMITS: "global_resource_limits_prevent_full_coverage",
}


def _solve_phase2_dp_with_phase1(
    instance_handle: c_void_p,
    phase1_handle: c_void_p,
) -> NativePhase2DPResult:
    out = c_void_p(0)
    status = _n.mespprc_solve_phase2_dp(instance_handle, phase1_handle, byref(out))
    _n._check(status, "mespprc_solve_phase2_dp")
    handle = c_void_p(out.value or 0)
    try:
        is_feasible = bool(_n.mespprc_phase2_dp_is_feasible(handle))
        coverage_complete = bool(_n.mespprc_phase2_dp_coverage_complete(handle))
        infeas_code = int(_n.mespprc_phase2_dp_infeasibility_reason(handle))
        infeasibility_reason = _INFEAS_REASON_BY_CODE.get(infeas_code, None)

        total_cost: float | None = None
        if is_feasible:
            cost = c_double(0.0)
            _n._check(
                _n.mespprc_phase2_dp_total_cost(handle, byref(cost)),
                "mespprc_phase2_dp_total_cost",
            )
            total_cost = float(cost.value)

        n_selected = int(_n.mespprc_phase2_dp_selected_route_count(handle))
        selected: tuple[int, ...] = ()
        if n_selected > 0:
            buf = (c_int * n_selected)()
            _n._check(
                _n.mespprc_phase2_dp_selected_routes(handle, buf, c_int(n_selected)),
                "mespprc_phase2_dp_selected_routes",
            )
            selected = tuple(int(buf[i]) for i in range(n_selected))

        return NativePhase2DPResult(
            is_feasible=is_feasible,
            coverage_complete=coverage_complete,
            total_cost=total_cost,
            infeasibility_reason=infeasibility_reason,
            selected_phase1_indices=selected,
        )
    finally:
        if handle:
            _n.mespprc_phase2_dp_result_destroy(handle)


# ---------- Phase 2 IP wrapper ----------


@dataclass(frozen=True, slots=True)
class NativePhase2IPResult:
    """Phase 2 IP outcome translated back to a Python-friendly form."""

    is_feasible: bool
    coverage_complete: bool
    total_cost: float | None
    infeasibility_reason: str | None
    selected_phase1_indices: tuple[int, ...]
    original_route_count: int
    reduced_route_count: int


def solve_phase2_ip(
    instance: MESPPRCInstance,
    *,
    label_limit: int | None = None,
) -> NativePhase2IPResult:
    """
    Run Phase 1 + Phase 2 IP entirely in C and return the cover.

    Mirrors `solve_phase2_dp` in shape and intent, but the Phase 2 step here
    is a set-partitioning MIP solved by the bundled HiGHS C library.
    """

    native_inst = build_native_instance(instance)
    try:
        phase1_handle = c_void_p(0)
        status = _n.mespprc_solve_phase1(
            native_inst.handle,
            c_int(0 if label_limit is None or label_limit <= 0 else int(label_limit)),
            byref(phase1_handle),
        )
        _n._check(status, "mespprc_solve_phase1")
        try:
            return _solve_phase2_ip_with_phase1(native_inst.handle, phase1_handle)
        finally:
            if phase1_handle:
                _n.mespprc_phase1_result_destroy(phase1_handle)
    finally:
        native_inst.close()


def _solve_phase2_ip_with_phase1(
    instance_handle: c_void_p,
    phase1_handle: c_void_p,
) -> NativePhase2IPResult:
    out = c_void_p(0)
    status = _n.mespprc_solve_phase2_ip(instance_handle, phase1_handle, byref(out))
    _n._check(status, "mespprc_solve_phase2_ip")
    handle = c_void_p(out.value or 0)
    try:
        is_feasible = bool(_n.mespprc_phase2_ip_is_feasible(handle))
        coverage_complete = bool(_n.mespprc_phase2_ip_coverage_complete(handle))
        infeas_code = int(_n.mespprc_phase2_ip_infeasibility_reason(handle))
        infeasibility_reason = _INFEAS_REASON_BY_CODE.get(infeas_code, None)

        total_cost: float | None = None
        if is_feasible:
            cost = c_double(0.0)
            _n._check(
                _n.mespprc_phase2_ip_total_cost(handle, byref(cost)),
                "mespprc_phase2_ip_total_cost",
            )
            total_cost = float(cost.value)

        n_selected = int(_n.mespprc_phase2_ip_selected_route_count(handle))
        selected: tuple[int, ...] = ()
        if n_selected > 0:
            buf = (c_int * n_selected)()
            _n._check(
                _n.mespprc_phase2_ip_selected_routes(handle, buf, c_int(n_selected)),
                "mespprc_phase2_ip_selected_routes",
            )
            selected = tuple(int(buf[i]) for i in range(n_selected))

        original = int(_n.mespprc_phase2_ip_original_route_count(handle))
        reduced = int(_n.mespprc_phase2_ip_reduced_route_count(handle))

        return NativePhase2IPResult(
            is_feasible=is_feasible,
            coverage_complete=coverage_complete,
            total_cost=total_cost,
            infeasibility_reason=infeasibility_reason,
            selected_phase1_indices=selected,
            original_route_count=original,
            reduced_route_count=reduced,
        )
    finally:
        if handle:
            _n.mespprc_phase2_ip_result_destroy(handle)


@dataclass(frozen=True, slots=True)
class NativeReplicateTimings:
    """Side-by-side DP vs IP timings on a shared Phase 1 run, all in C."""

    phase1_ms: float
    dp_ms: float
    ip_ms: float
    dp_objective: float | None
    ip_objective: float | None
    dp_feasible: bool
    ip_feasible: bool
    dp_infeasibility_reason: str | None
    ip_infeasibility_reason: str | None
    phase1_route_count: int
    ip_reduced_route_count: int


def time_phase2_dp_vs_ip(instance: MESPPRCInstance) -> NativeReplicateTimings:
    """
    Run Phase 1 once in C, then run both Phase 2 DP and Phase 2 IP against the
    same C-side Phase 1 result. Returns wall-clock times in milliseconds for
    each step plus the objective values for cross-checking.
    """

    from time import perf_counter

    native_inst = build_native_instance(instance)
    try:
        phase1_handle = c_void_p(0)
        t0 = perf_counter()
        status = _n.mespprc_solve_phase1(
            native_inst.handle, c_int(0), byref(phase1_handle)
        )
        _n._check(status, "mespprc_solve_phase1")
        phase1_ms = (perf_counter() - t0) * 1000.0
        try:
            phase1_route_count = int(_n.mespprc_phase1_route_count(phase1_handle))

            t0 = perf_counter()
            dp = _solve_phase2_dp_with_phase1(native_inst.handle, phase1_handle)
            dp_ms = (perf_counter() - t0) * 1000.0

            t0 = perf_counter()
            ip = _solve_phase2_ip_with_phase1(native_inst.handle, phase1_handle)
            ip_ms = (perf_counter() - t0) * 1000.0
        finally:
            if phase1_handle:
                _n.mespprc_phase1_result_destroy(phase1_handle)
    finally:
        native_inst.close()

    return NativeReplicateTimings(
        phase1_ms=phase1_ms,
        dp_ms=dp_ms,
        ip_ms=ip_ms,
        dp_objective=dp.total_cost,
        ip_objective=ip.total_cost,
        dp_feasible=dp.is_feasible,
        ip_feasible=ip.is_feasible,
        dp_infeasibility_reason=dp.infeasibility_reason,
        ip_infeasibility_reason=ip.infeasibility_reason,
        phase1_route_count=phase1_route_count,
        ip_reduced_route_count=ip.reduced_route_count,
    )


def _phase1_result_to_routes(
    result_handle: c_void_p,
    instance: MESPPRCInstance,
) -> List[NativeRoute]:
    count = int(_n.mespprc_phase1_route_count(result_handle))
    if count == 0:
        return []

    local_dim = int(_n.mespprc_phase1_local_dim(result_handle))
    global_dim = int(_n.mespprc_phase1_global_dim(result_handle))
    n_cust = int(_n.mespprc_phase1_num_customers(result_handle))

    # `instance.customers()` returns a sorted list of customer node ids — same
    # ordering the C side uses for its dense customer index.
    customer_ids_in_dense_order = list(instance.customers())

    routes: List[NativeRoute] = []
    for idx in range(count):
        path_len = int(_n.mespprc_phase1_path_length(result_handle, idx))
        path_buf = (c_int * max(path_len, 1))()
        _n._check(
            _n.mespprc_phase1_route_path(result_handle, idx, path_buf, c_int(path_len)),
            f"mespprc_phase1_route_path({idx})",
        )
        path = tuple(int(path_buf[i]) for i in range(path_len))

        cost = c_double(0.0)
        _n._check(
            _n.mespprc_phase1_route_cost(result_handle, idx, byref(cost)),
            f"mespprc_phase1_route_cost({idx})",
        )

        first = c_int(-1)
        _n._check(
            _n.mespprc_phase1_route_first_customer(result_handle, idx, byref(first)),
            f"mespprc_phase1_route_first_customer({idx})",
        )

        local_buf = (c_double * max(local_dim, 1))()
        if local_dim > 0:
            _n._check(
                _n.mespprc_phase1_route_local_resources(
                    result_handle, idx, local_buf, c_int(local_dim)
                ),
                f"mespprc_phase1_route_local_resources({idx})",
            )

        global_buf = (c_double * max(global_dim, 1))()
        if global_dim > 0:
            _n._check(
                _n.mespprc_phase1_route_global_resources(
                    result_handle, idx, global_buf, c_int(global_dim)
                ),
                f"mespprc_phase1_route_global_resources({idx})",
            )

        sig_buf = (c_int * max(n_cust, 1))()
        if n_cust > 0:
            _n._check(
                _n.mespprc_phase1_route_customer_state_signature(
                    result_handle, idx, sig_buf, c_int(n_cust)
                ),
                f"mespprc_phase1_route_customer_state_signature({idx})",
            )

        covered = frozenset(
            customer_ids_in_dense_order[i]
            for i in range(n_cust)
            if int(sig_buf[i]) > 0
        )

        routes.append(
            NativeRoute(
                cost=float(cost.value),
                path=path,
                local_resources=tuple(float(local_buf[i]) for i in range(local_dim)),
                global_resources=tuple(float(global_buf[i]) for i in range(global_dim)),
                first_customer_in_route=(int(first.value) if first.value >= 0 else None),
                customer_state_signature=tuple(int(sig_buf[i]) for i in range(n_cust)),
                covered_customers=covered,
            )
        )
    return routes
