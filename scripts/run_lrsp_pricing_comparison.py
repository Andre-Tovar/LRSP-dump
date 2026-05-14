"""
Run the LRSP column-generation solver on every discovered LRSP instance, once
with the IP pricing engine and once with the DP pricing engine, and write a
matched-pair performance report to disk.

The two runs for one instance share:
- the same loaded LRSPInstance object,
- the same column-generation configuration (max iterations, time limit,
  improvement tolerance, max columns per facility, label limit, etc.),
- the same singleton-seed initial column set,
- the same restricted master formulation (Akca-style cover/capacity/linking/min-open).

The only thing that differs between the two runs is which pricing solver is
used. That keeps the comparison apples-to-apples.

Outputs (default folder: results/lrsp_pricing_comparison/):
- comparison_results.json : full per-run metrics + per-instance pair
- comparison_results.csv  : flat one-row-per-run table
- comparison_table.txt    : human-readable side-by-side table

Usage:
    python scripts/run_lrsp_pricing_comparison.py
    python scripts/run_lrsp_pricing_comparison.py --time-limit 120 --max-customers 25
    python scripts/run_lrsp_pricing_comparison.py --include p01-l40-v1t1
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

from lrsp_solver import (
    LRSPSolver,
    LRSPSolverConfig,
    load_lrsp_instance,
)
from lrsp_solver.results import ColumnGenerationResult

from scripts.discover_lrsp_instances import InstanceReport, discover  # type: ignore[import]


_DEFAULT_OUTPUT_DIR = REPO_ROOT / "results" / "lrsp_pricing_comparison"


@dataclass(slots=True)
class RunRecord:
    instance_name: str
    instance_path: str
    pricing_method: str
    num_customers: int
    num_facilities: int
    vehicle_time_limit: float
    status: str
    objective_value: float | None
    root_lp_objective: float | None
    integer_objective: float | None
    iterations: int
    pricing_calls: int
    total_columns: int
    master_runtime: float
    pricing_runtime: float
    total_runtime: float
    avg_pricing_time_per_iteration: float | None
    max_pricing_time_in_iteration: float | None
    reached_optimality: bool
    no_improving_column_found: bool
    hit_time_limit: bool
    failed: bool
    failure_message: str | None


@dataclass(slots=True)
class PairRecord:
    instance_name: str
    instance_path: str
    num_customers: int
    num_facilities: int
    vehicle_time_limit: float
    dp_run: RunRecord
    ip_run: RunRecord
    objective_match: bool
    objective_diff: float | None


def select_instances(
    *,
    roots: Sequence[Path],
    include: Sequence[str] | None,
    max_customers: int | None,
    limit: int | None,
) -> List[InstanceReport]:
    reports = [r for r in discover(roots) if r.is_lrsp]
    if include:
        wanted = {token for token in include}
        reports = [
            r
            for r in reports
            if r.name in wanted or r.path.endswith(tuple(wanted)) or any(token in r.path for token in wanted)
        ]
    if max_customers is not None:
        reports = [r for r in reports if (r.num_customers or 0) <= max_customers]
    reports.sort(key=lambda r: (r.num_customers or 0, r.name))
    if limit is not None:
        reports = reports[:limit]
    return reports


def run_one(
    *,
    report: InstanceReport,
    pricing: str,
    base_config: LRSPSolverConfig,
) -> RunRecord:
    instance_path = REPO_ROOT / report.path if not Path(report.path).is_absolute() else Path(report.path)
    failure_message: str | None = None
    try:
        instance = load_lrsp_instance(instance_path)
        config = LRSPSolverConfig(
            pricing=pricing,  # type: ignore[arg-type]
            max_iterations=base_config.max_iterations,
            max_columns_per_facility=base_config.max_columns_per_facility,
            phase1_label_limit=base_config.phase1_label_limit,
            improvement_tolerance=base_config.improvement_tolerance,
            solve_integer_master=base_config.solve_integer_master,
            use_facility_customer_linking=base_config.use_facility_customer_linking,
            use_min_open_facilities_bound=base_config.use_min_open_facilities_bound,
            time_limit_seconds=base_config.time_limit_seconds,
            cbc_msg=base_config.cbc_msg,
        )
        run_start = perf_counter()
        result = LRSPSolver(config).solve(instance)
        elapsed = perf_counter() - run_start
    except Exception as exc:
        failure_message = f"{type(exc).__name__}: {exc}\n{traceback.format_exc(limit=5)}"
        return RunRecord(
            instance_name=report.name,
            instance_path=report.path,
            pricing_method=pricing,
            num_customers=report.num_customers or 0,
            num_facilities=report.num_facilities or 0,
            vehicle_time_limit=report.vehicle_time_limit or 0.0,
            status="failed",
            objective_value=None,
            root_lp_objective=None,
            integer_objective=None,
            iterations=0,
            pricing_calls=0,
            total_columns=0,
            master_runtime=0.0,
            pricing_runtime=0.0,
            total_runtime=0.0,
            avg_pricing_time_per_iteration=None,
            max_pricing_time_in_iteration=None,
            reached_optimality=False,
            no_improving_column_found=False,
            hit_time_limit=False,
            failed=True,
            failure_message=failure_message,
        )

    objective_value = (
        result.integer_objective
        if result.integer_objective is not None
        else result.root_lp_objective
    )
    no_improving_column = result.reached_optimality and any(
        it.new_column_count == 0 for it in result.iterations
    )
    return RunRecord(
        instance_name=report.name,
        instance_path=report.path,
        pricing_method=pricing,
        num_customers=report.num_customers or 0,
        num_facilities=report.num_facilities or 0,
        vehicle_time_limit=report.vehicle_time_limit or 0.0,
        status=result.status,
        objective_value=objective_value,
        root_lp_objective=result.root_lp_objective,
        integer_objective=result.integer_objective,
        iterations=result.iteration_count,
        pricing_calls=result.pricing_call_count,
        total_columns=result.total_columns,
        master_runtime=result.master_runtime,
        pricing_runtime=result.pricing_runtime,
        total_runtime=result.total_runtime if result.total_runtime > 0 else elapsed,
        avg_pricing_time_per_iteration=result.avg_pricing_time_per_iteration,
        max_pricing_time_in_iteration=result.max_pricing_time_in_iteration,
        reached_optimality=result.reached_optimality,
        no_improving_column_found=no_improving_column,
        hit_time_limit=result.status == "time_limit",
        failed=False,
        failure_message=result.failure_message,
    )


def write_outputs(
    *,
    pairs: Sequence[PairRecord],
    output_dir: Path,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)

    json_path = output_dir / "comparison_results.json"
    json_path.write_text(
        json.dumps(
            {"pairs": [asdict(p) for p in pairs]},
            indent=2,
            default=_json_default,
        ),
        encoding="utf-8",
    )

    csv_path = output_dir / "comparison_results.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        fieldnames = list(asdict(pairs[0].dp_run).keys()) if pairs else []
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for pair in pairs:
            writer.writerow(asdict(pair.dp_run))
            writer.writerow(asdict(pair.ip_run))

    table_path = output_dir / "comparison_table.txt"
    table_path.write_text(_render_table(pairs), encoding="utf-8")


def _json_default(value: object) -> object:
    if isinstance(value, Path):
        return str(value)
    raise TypeError(f"Cannot serialize {type(value).__name__}")


def _render_table(pairs: Sequence[PairRecord]) -> str:
    if not pairs:
        return "(no pairs)\n"

    headers = [
        "instance",
        "pricing",
        "n_cust",
        "n_fac",
        "time_lim",
        "status",
        "objective",
        "root_lp",
        "iter",
        "cols",
        "pcall",
        "master_s",
        "pricing_s",
        "total_s",
        "avg_pricing_s",
        "max_pricing_s",
        "reached_opt",
    ]

    rows: list[list[str]] = []
    for pair in pairs:
        for run in (pair.dp_run, pair.ip_run):
            rows.append(
                [
                    run.instance_name,
                    run.pricing_method,
                    str(run.num_customers),
                    str(run.num_facilities),
                    f"{run.vehicle_time_limit:g}",
                    run.status,
                    _fmt(run.objective_value),
                    _fmt(run.root_lp_objective),
                    str(run.iterations),
                    str(run.total_columns),
                    str(run.pricing_calls),
                    f"{run.master_runtime:.3f}",
                    f"{run.pricing_runtime:.3f}",
                    f"{run.total_runtime:.3f}",
                    _fmt(run.avg_pricing_time_per_iteration, 4),
                    _fmt(run.max_pricing_time_in_iteration, 4),
                    "yes" if run.reached_optimality else "no",
                ]
            )

    widths = [
        max(len(headers[i]), max((len(row[i]) for row in rows), default=0))
        for i in range(len(headers))
    ]

    def fmt_row(values: list[str]) -> str:
        return " | ".join(value.ljust(widths[i]) for i, value in enumerate(values))

    sep = "-+-".join("-" * w for w in widths)
    body = "\n".join(fmt_row(row) for row in rows)
    return "\n".join([fmt_row(headers), sep, body, ""])


def _fmt(value: float | None, digits: int = 4) -> str:
    if value is None:
        return "-"
    return f"{value:.{digits}f}"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--root", action="append", type=Path, default=None,
                        help="Repository folder(s) to scan; default: 'Akca Repo'.")
    parser.add_argument("--include", action="append", default=None,
                        help="Optional substring filter; instance kept if its name or path matches.")
    parser.add_argument("--max-customers", type=int, default=None,
                        help="Skip instances with more customers than this.")
    parser.add_argument("--limit", type=int, default=None,
                        help="Cap the number of instances after sorting by size.")
    parser.add_argument("--max-iterations", type=int, default=50)
    parser.add_argument("--max-columns-per-facility", type=int, default=5)
    parser.add_argument("--phase1-label-limit", type=int, default=None)
    parser.add_argument("--time-limit", type=float, default=None,
                        help="Wall-clock limit per pricing run (seconds).")
    parser.add_argument("--no-integer", action="store_true",
                        help="Skip the integer master step on both runs.")
    parser.add_argument("--output-dir", type=Path, default=_DEFAULT_OUTPUT_DIR)
    parser.add_argument("--print-table", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    roots = args.root or [REPO_ROOT / "Akca Repo"]
    selected = select_instances(
        roots=roots,
        include=args.include,
        max_customers=args.max_customers,
        limit=args.limit,
    )
    if not selected:
        print("No matching LRSP instances found.")
        return 1

    base_config = LRSPSolverConfig(
        max_iterations=args.max_iterations,
        max_columns_per_facility=args.max_columns_per_facility,
        phase1_label_limit=args.phase1_label_limit,
        solve_integer_master=not args.no_integer,
        time_limit_seconds=args.time_limit,
    )

    pairs: list[PairRecord] = []
    for report in selected:
        print(
            f"=> {report.name}  customers={report.num_customers}  "
            f"facilities={report.num_facilities}  time_limit={report.vehicle_time_limit}"
        )
        dp_record = run_one(report=report, pricing="dp", base_config=base_config)
        print(
            f"   dp: status={dp_record.status} obj={_fmt(dp_record.objective_value)} "
            f"iter={dp_record.iterations} cols={dp_record.total_columns} "
            f"total={dp_record.total_runtime:.3f}s"
        )
        ip_record = run_one(report=report, pricing="ip", base_config=base_config)
        print(
            f"   ip: status={ip_record.status} obj={_fmt(ip_record.objective_value)} "
            f"iter={ip_record.iterations} cols={ip_record.total_columns} "
            f"total={ip_record.total_runtime:.3f}s"
        )

        objective_diff: float | None = None
        objective_match = False
        if dp_record.objective_value is not None and ip_record.objective_value is not None:
            objective_diff = dp_record.objective_value - ip_record.objective_value
            objective_match = abs(objective_diff) <= 1e-3
        pairs.append(
            PairRecord(
                instance_name=report.name,
                instance_path=report.path,
                num_customers=report.num_customers or 0,
                num_facilities=report.num_facilities or 0,
                vehicle_time_limit=report.vehicle_time_limit or 0.0,
                dp_run=dp_record,
                ip_run=ip_record,
                objective_match=objective_match,
                objective_diff=objective_diff,
            )
        )

    write_outputs(pairs=pairs, output_dir=args.output_dir)
    print()
    print(f"Wrote {len(pairs)} instance pairs to {args.output_dir}/")
    print("  - comparison_results.json")
    print("  - comparison_results.csv")
    print("  - comparison_table.txt")
    if args.print_table:
        print()
        print(_render_table(pairs))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
