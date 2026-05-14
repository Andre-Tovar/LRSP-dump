"""
Head-to-head Phase 2 micro-benchmark: DP vs IP route-combiner on the LRSP
pricing graph.

Why a separate script:

The full LRSP column-generation comparison runs Phase 1 + Phase 2 inside each
pricing call, but Phase 1 (the ESPPRC labeling DP) is identical across the DP
and IP adapters. It dominates wall-clock and washes out the actual DP-vs-IP
difference in the route combiner. This script isolates Phase 2 by:

1. Loading a real LRSP instance.
2. Building the per-facility reduced-cost pricing graph from the instance.
3. Running Phase 1 once (and caching its routes).
4. Timing Phase 2 DP on those routes.
5. Timing Phase 2 IP on those routes.
6. Aggregating per-instance and writing JSON / CSV / a printable table.

The duals can be either zero (no-op pricing) or generated from a quick warm
LP solve so Phase 1 returns more interesting routes. Default is "warm".

Output folder (default): results/lrsp_pricing_comparison/phase2_microbench/

Usage:
    python scripts/benchmark_pricing_phase2.py
    python scripts/benchmark_pricing_phase2.py --max-customers 30 --duals warm
    python scripts/benchmark_pricing_phase2.py --include p01-f25-v1t1
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
import traceback
from dataclasses import asdict, dataclass, field
from pathlib import Path
from time import perf_counter
from typing import List, Sequence

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mespprc import Phase1Solver, Phase2DPSolver, Phase2IPSolver, Route

from lrsp_solver import (
    LRSPInstance,
    LRSPSolverConfig,
    LRSPSolver,
    load_lrsp_instance,
)
from lrsp_solver.column import MasterDuals
from lrsp_solver.pricing_graph import (
    SOURCE_NODE_ID,
    build_facility_pricing_graph,
)

from scripts.discover_lrsp_instances import InstanceReport, discover  # type: ignore[import]


_DEFAULT_OUTPUT_DIR = REPO_ROOT / "results" / "lrsp_pricing_comparison" / "phase2_microbench"


@dataclass(slots=True)
class FacilityPhase2Record:
    facility_id: int
    phase1_route_count: int
    phase1_time: float
    phase2_dp_time: float
    phase2_ip_time: float
    phase2_dp_status: str
    phase2_ip_status: str
    phase2_dp_objective: float | None
    phase2_ip_objective: float | None
    phase2_dp_route_count: int
    phase2_ip_route_count: int


@dataclass(slots=True)
class InstancePhase2Record:
    instance_name: str
    instance_path: str
    num_customers: int
    num_facilities: int
    vehicle_time_limit: float
    duals_source: str
    facilities: List[FacilityPhase2Record] = field(default_factory=list)
    failed: bool = False
    failure_message: str | None = None

    def aggregated(self) -> dict[str, object]:
        total_p1 = sum(f.phase1_time for f in self.facilities)
        total_dp = sum(f.phase2_dp_time for f in self.facilities)
        total_ip = sum(f.phase2_ip_time for f in self.facilities)
        return {
            "instance_name": self.instance_name,
            "num_customers": self.num_customers,
            "num_facilities": self.num_facilities,
            "vehicle_time_limit": self.vehicle_time_limit,
            "duals_source": self.duals_source,
            "phase1_total_time": total_p1,
            "phase2_dp_total_time": total_dp,
            "phase2_ip_total_time": total_ip,
            "phase2_speedup_dp_over_ip": (total_ip / total_dp) if total_dp > 0 else None,
            "phase1_avg_time_per_facility": total_p1 / max(len(self.facilities), 1),
            "phase2_dp_avg_time_per_facility": total_dp / max(len(self.facilities), 1),
            "phase2_ip_avg_time_per_facility": total_ip / max(len(self.facilities), 1),
        }


def _zero_duals(instance: LRSPInstance) -> MasterDuals:
    return MasterDuals(
        coverage={c.id: 0.0 for c in instance.customers},
        facility_capacity={f.id: 0.0 for f in instance.facilities},
        facility_customer_link={
            (c.id, f.id): 0.0 for f in instance.facilities for c in instance.customers
        },
        min_open_facilities=0.0,
    )


def _warm_duals(instance: LRSPInstance, *, label_limit: int) -> tuple[MasterDuals, str]:
    """
    Run a single LRSP CG iteration with the DP engine just to obtain LP duals
    that are typical of an actively-being-solved LRSP problem. The duals make
    Phase 1 produce a richer set of negative-reduced-cost routes than zero
    duals would.
    """

    config = LRSPSolverConfig(
        pricing="dp",
        max_iterations=1,
        max_columns_per_facility=3,
        phase1_label_limit=label_limit,
        solve_integer_master=False,
    )
    result = LRSPSolver(config).solve(instance)
    if result.final_master is None or result.final_master.duals is None:
        return _zero_duals(instance), "warm_unavailable_fallback_zero"
    return result.final_master.duals, "warm_after_one_cg_iteration"


def _build_synthetic_singleton_routes(graph) -> list[Route]:
    """
    Build a singleton route per customer directly from the pricing graph,
    bypassing Phase 1. The route is `source -> customer -> sink` with arc
    costs read from the dual-adjusted graph.

    This lets the microbench feed Phase 2 with a non-trivial route pool whose
    size is exactly `num_customers`, while keeping the total construction time
    O(num_customers). It is the right input shape to time the Phase 2 DP vs IP
    route-combiner head-to-head without paying the Phase 1 labeling tax.
    """

    pricing_instance = graph.pricing_instance
    sink = graph.sink_node
    routes: list[Route] = []
    for customer_id in pricing_instance.customers():
        if (SOURCE_NODE_ID, customer_id) not in pricing_instance.arcs:
            continue
        if (customer_id, sink) not in pricing_instance.arcs:
            continue
        a = pricing_instance.get_arc(SOURCE_NODE_ID, customer_id)
        b = pricing_instance.get_arc(customer_id, sink)
        cost = float(a.cost) + float(b.cost)
        local = [
            float(a.local_res[i]) + float(b.local_res[i])
            for i in range(len(a.local_res))
        ]
        glob = [
            float(a.global_res[i]) + float(b.global_res[i])
            for i in range(len(a.global_res))
        ]
        # local feasibility check against per-route limits
        if pricing_instance.local_limits and any(
            local[i] > pricing_instance.local_limits[i]
            for i in range(len(local))
        ):
            continue
        routes.append(
            Route(
                route_id=len(routes) + 1,
                cost=cost,
                local_resources=local,
                global_resources=glob,
                covered_customers={customer_id},
                path=[SOURCE_NODE_ID, customer_id, sink],
            )
        )
    return routes


def benchmark_instance(
    *,
    report: InstanceReport,
    duals_mode: str,
    label_limit: int,
    skip_phase1: bool = False,
) -> InstancePhase2Record:
    instance_path = (
        REPO_ROOT / report.path
        if not Path(report.path).is_absolute()
        else Path(report.path)
    )
    record = InstancePhase2Record(
        instance_name=report.name,
        instance_path=report.path,
        num_customers=report.num_customers or 0,
        num_facilities=report.num_facilities or 0,
        vehicle_time_limit=report.vehicle_time_limit or 0.0,
        duals_source=duals_mode,
    )

    try:
        instance = load_lrsp_instance(instance_path)
        if duals_mode == "warm":
            duals, source_label = _warm_duals(instance, label_limit=label_limit)
            record.duals_source = source_label
        else:
            duals = _zero_duals(instance)
            record.duals_source = "zero"

        for facility in instance.facilities:
            graph = build_facility_pricing_graph(instance, facility, duals)
            if skip_phase1:
                t0 = perf_counter()
                routes_for_phase2 = _build_synthetic_singleton_routes(graph)
                phase1_time = perf_counter() - t0
            else:
                phase1_solver = Phase1Solver(
                    graph.pricing_instance, label_limit=label_limit
                )
                t0 = perf_counter()
                phase1_result = phase1_solver.solve()
                phase1_time = perf_counter() - t0
                routes_for_phase2 = list(phase1_result.feasible_routes)
            phase2_dp_solver = Phase2DPSolver(graph.pricing_instance)
            t0 = perf_counter()
            try:
                dp_result = phase2_dp_solver.solve(routes_for_phase2)
                dp_status = str(dp_result.status)
                dp_obj = (
                    float(dp_result.total_cost)
                    if dp_result.total_cost is not None
                    else None
                )
                dp_routes = len(dp_result.selected_routes)
            except Exception as exc:
                dp_status = f"error:{type(exc).__name__}"
                dp_obj = None
                dp_routes = 0
            phase2_dp_time = perf_counter() - t0

            phase2_ip_solver = Phase2IPSolver(graph.pricing_instance)
            t0 = perf_counter()
            try:
                ip_result = phase2_ip_solver.solve(routes_for_phase2, collect_diagnostics=False)
                ip_status = str(ip_result.status)
                ip_obj = (
                    float(ip_result.objective_value)
                    if ip_result.objective_value is not None
                    else None
                )
                ip_routes = len(ip_result.selected_routes)
            except Exception as exc:
                ip_status = f"error:{type(exc).__name__}"
                ip_obj = None
                ip_routes = 0
            phase2_ip_time = perf_counter() - t0

            record.facilities.append(
                FacilityPhase2Record(
                    facility_id=facility.id,
                    phase1_route_count=len(routes_for_phase2),
                    phase1_time=phase1_time,
                    phase2_dp_time=phase2_dp_time,
                    phase2_ip_time=phase2_ip_time,
                    phase2_dp_status=dp_status,
                    phase2_ip_status=ip_status,
                    phase2_dp_objective=dp_obj,
                    phase2_ip_objective=ip_obj,
                    phase2_dp_route_count=dp_routes,
                    phase2_ip_route_count=ip_routes,
                )
            )

    except Exception as exc:
        record.failed = True
        record.failure_message = f"{type(exc).__name__}: {exc}\n{traceback.format_exc(limit=5)}"

    return record


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--root", action="append", type=Path, default=None,
                        help="Repository folder(s) to scan; default: 'Akca Repo'.")
    parser.add_argument("--include", action="append", default=None,
                        help="Optional substring filter on instance name or path.")
    parser.add_argument("--max-customers", type=int, default=None)
    parser.add_argument("--limit", type=int, default=None)
    parser.add_argument("--label-limit", type=int, default=20,
                        help="Phase 1 label limit (caps Phase 1 runtime).")
    parser.add_argument("--duals", choices=("zero", "warm"), default="warm",
                        help="Dual source for the pricing graph.")
    parser.add_argument("--skip-phase1", action="store_true",
                        help="Skip Phase 1 entirely; feed Phase 2 with synthetic singleton "
                             "routes built directly from the pricing graph. Useful when the "
                             "goal is to time the DP-vs-IP route-combiner head-to-head.")
    parser.add_argument("--output-dir", type=Path, default=_DEFAULT_OUTPUT_DIR)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    roots = args.root or [REPO_ROOT / "Akca Repo"]
    reports = [r for r in discover(roots) if r.is_lrsp]
    if args.include:
        reports = [
            r for r in reports
            if r.name in args.include
            or any(token in r.path or token in r.name for token in args.include)
        ]
    if args.max_customers is not None:
        reports = [r for r in reports if (r.num_customers or 0) <= args.max_customers]
    reports.sort(key=lambda r: (r.num_customers or 0, r.name))
    if args.limit is not None:
        reports = reports[: args.limit]
    if not reports:
        print("No matching LRSP instances found.")
        return 1

    records: list[InstancePhase2Record] = []
    for report in reports:
        print(
            f"=> {report.name}  customers={report.num_customers}  "
            f"facilities={report.num_facilities}",
            flush=True,
        )
        record = benchmark_instance(
            report=report,
            duals_mode=args.duals,
            label_limit=args.label_limit,
            skip_phase1=args.skip_phase1,
        )
        records.append(record)
        if record.failed:
            print(f"   FAILED: {record.failure_message}", flush=True)
            continue
        agg = record.aggregated()
        print(
            f"   phase1={agg['phase1_total_time']:.3f}s  "
            f"phase2_dp={agg['phase2_dp_total_time']:.3f}s  "
            f"phase2_ip={agg['phase2_ip_total_time']:.3f}s  "
            f"dp_vs_ip_speedup={agg['phase2_speedup_dp_over_ip']}",
            flush=True,
        )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    json_path = args.output_dir / "phase2_microbench.json"
    json_path.write_text(
        json.dumps([asdict(r) for r in records], indent=2, default=str),
        encoding="utf-8",
    )
    csv_path = args.output_dir / "phase2_microbench.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "instance",
            "num_customers",
            "num_facilities",
            "vehicle_time_limit",
            "duals_source",
            "facility_id",
            "phase1_route_count",
            "phase1_time_s",
            "phase2_dp_time_s",
            "phase2_ip_time_s",
            "phase2_dp_status",
            "phase2_ip_status",
            "phase2_dp_objective",
            "phase2_ip_objective",
            "phase2_dp_route_count",
            "phase2_ip_route_count",
        ])
        for record in records:
            for fac in record.facilities:
                writer.writerow([
                    record.instance_name,
                    record.num_customers,
                    record.num_facilities,
                    record.vehicle_time_limit,
                    record.duals_source,
                    fac.facility_id,
                    fac.phase1_route_count,
                    f"{fac.phase1_time:.4f}",
                    f"{fac.phase2_dp_time:.4f}",
                    f"{fac.phase2_ip_time:.4f}",
                    fac.phase2_dp_status,
                    fac.phase2_ip_status,
                    fac.phase2_dp_objective,
                    fac.phase2_ip_objective,
                    fac.phase2_dp_route_count,
                    fac.phase2_ip_route_count,
                ])

    table_path = args.output_dir / "phase2_microbench_table.txt"
    table_path.write_text(_render_table(records), encoding="utf-8")

    print()
    print(f"Wrote {len(records)} instance records to {args.output_dir}/")
    print("  - phase2_microbench.json")
    print("  - phase2_microbench.csv")
    print("  - phase2_microbench_table.txt")
    return 0


def _render_table(records: Sequence[InstancePhase2Record]) -> str:
    headers = [
        "instance",
        "n_cust",
        "n_fac",
        "duals",
        "phase1_s",
        "phase2_dp_s",
        "phase2_ip_s",
        "speedup_dp_vs_ip",
    ]
    rows: list[list[str]] = []
    for record in records:
        if record.failed:
            rows.append([
                record.instance_name,
                str(record.num_customers),
                str(record.num_facilities),
                record.duals_source,
                "FAILED",
                "FAILED",
                "FAILED",
                "-",
            ])
            continue
        agg = record.aggregated()
        speedup = agg["phase2_speedup_dp_over_ip"]
        rows.append([
            record.instance_name,
            str(record.num_customers),
            str(record.num_facilities),
            record.duals_source,
            f"{agg['phase1_total_time']:.3f}",
            f"{agg['phase2_dp_total_time']:.3f}",
            f"{agg['phase2_ip_total_time']:.3f}",
            f"{speedup:.3f}" if isinstance(speedup, (int, float)) else "-",
        ])

    widths = [
        max(len(headers[i]), max((len(row[i]) for row in rows), default=0))
        for i in range(len(headers))
    ]

    def fmt_row(values: list[str]) -> str:
        return " | ".join(value.ljust(widths[i]) for i, value in enumerate(values))

    sep = "-+-".join("-" * w for w in widths)
    body = "\n".join(fmt_row(row) for row in rows)
    return "\n".join([fmt_row(headers), sep, body, ""])


if __name__ == "__main__":
    raise SystemExit(main())
