from __future__ import annotations

import argparse
import csv
import sys
from dataclasses import dataclass
from pathlib import Path
from time import perf_counter
from typing import Iterable, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from lrsp_solver import LRSPSolver, build_akca_style_instance


AKCA_BENCHMARK_SPECS: tuple[tuple[str, str, int, str, str], ...] = (
    ("p01", "f", 25, "v1", "t1"),
    ("p01", "f", 25, "v2", "t1"),
    ("p01", "f", 40, "v1", "t1"),
    ("p01", "l", 40, "v1", "t1"),
    ("p07", "s", 40, "v1", "t1"),
    ("p07", "t", 40, "v2", "t1"),
)


@dataclass(slots=True, frozen=True)
class SolverRunRecord:
    instance_name: str
    pipeline_label: str
    pricing_phase2: str
    wall_clock_seconds: float
    column_generation_seconds: float
    integer_objective: float | None
    root_lp_objective: float | None
    pricing_call_count: int
    total_columns: int
    master_iterations: int
    status: str


@dataclass(slots=True, frozen=True)
class InstanceComparison:
    instance_name: str
    dp_dp: SolverRunRecord
    dp_ip: SolverRunRecord

    @property
    def objective_match(self) -> bool | None:
        if self.dp_dp.integer_objective is None or self.dp_ip.integer_objective is None:
            return None
        return abs(self.dp_dp.integer_objective - self.dp_ip.integer_objective) <= 1e-6

    @property
    def winner_label(self) -> str:
        if abs(self.dp_dp.wall_clock_seconds - self.dp_ip.wall_clock_seconds) <= 1e-9:
            return "Tie"
        if self.dp_dp.wall_clock_seconds < self.dp_ip.wall_clock_seconds:
            return self.dp_dp.pipeline_label
        return self.dp_ip.pipeline_label


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Benchmark the LRSP root solver on selected Akca instances using "
            "DP/DP and DP/IP two-phase pricing, then save a comparison chart."
        )
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=None,
        help="Benchmark only the first N bundled Akca instances from the fixed comparison list.",
    )
    parser.add_argument(
        "--max-iterations",
        type=int,
        default=20,
        help="Maximum column-generation iterations for each LRSP solve.",
    )
    parser.add_argument(
        "--max-columns-per-facility",
        type=int,
        default=1,
        help="Maximum improving pairings added per facility per iteration.",
    )
    parser.add_argument(
        "--phase1-label-limit",
        type=int,
        default=250,
        help=(
            "Heuristic cap on kept Phase 1 labels per node. "
            "This keeps the Akca benchmark runs practical. Use 0 to disable the cap."
        ),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("Test Results") / "lrsp_dp_dp_vs_dp_ip_runtime.png",
        help="Where to save the matplotlib comparison chart.",
    )
    parser.add_argument(
        "--csv-output",
        type=Path,
        default=Path("Test Results") / "lrsp_dp_dp_vs_dp_ip_runtime.csv",
        help="Where to save the raw benchmark measurements as CSV.",
    )
    return parser.parse_args()


def load_instances(limit: int | None = None):
    specs = AKCA_BENCHMARK_SPECS if limit is None else AKCA_BENCHMARK_SPECS[:limit]
    instances = []
    for reference_group, marker, customer_count, vehicle_variant, time_variant in specs:
        instances.append(
            build_akca_style_instance(
                reference_group,
                marker,
                customer_count,
                vehicle_variant,
                time_variant,
            )
        )
    return instances


def run_solver_variant(
    instance,
    *,
    pricing_phase2: str,
    max_iterations: int,
    max_columns_per_facility: int,
    phase1_label_limit: int | None,
) -> SolverRunRecord:
    pipeline_label = "DP/DP" if pricing_phase2 == "dp" else "DP/IP"
    print(
        f"Running {pipeline_label} on {instance.name} "
        f"(max_iterations={max_iterations}, max_columns_per_facility={max_columns_per_facility}, "
        f"phase1_label_limit={phase1_label_limit})...",
        flush=True,
    )
    solver = LRSPSolver(
        pricing_phase2=pricing_phase2,
        phase1_label_limit=phase1_label_limit,
        max_iterations=max_iterations,
        max_columns_per_facility=max_columns_per_facility,
    )
    started_at = perf_counter()
    result = solver.solve_root_node(instance)
    elapsed = perf_counter() - started_at
    return SolverRunRecord(
        instance_name=instance.name,
        pipeline_label=pipeline_label,
        pricing_phase2=pricing_phase2,
        wall_clock_seconds=elapsed,
        column_generation_seconds=result.total_column_generation_time,
        integer_objective=result.integer_objective,
        root_lp_objective=result.root_lp_objective,
        pricing_call_count=result.pricing_call_count,
        total_columns=result.total_columns,
        master_iterations=result.master_iterations,
        status=result.status,
    )


def compare_instances(
    *,
    max_iterations: int,
    max_columns_per_facility: int,
    phase1_label_limit: int | None,
    limit: int | None = None,
) -> list[InstanceComparison]:
    comparisons: list[InstanceComparison] = []
    instances = load_instances(limit=limit)
    if not instances:
        return comparisons
    for index, instance in enumerate(instances, start=1):
        print(f"\n[{index}/{len(instances)}] Benchmarking {instance.name}", flush=True)
        dp_dp = run_solver_variant(
            instance,
            pricing_phase2="dp",
            max_iterations=max_iterations,
            max_columns_per_facility=max_columns_per_facility,
            phase1_label_limit=phase1_label_limit,
        )
        dp_ip = run_solver_variant(
            instance,
            pricing_phase2="ip",
            max_iterations=max_iterations,
            max_columns_per_facility=max_columns_per_facility,
            phase1_label_limit=phase1_label_limit,
        )
        comparisons.append(
            InstanceComparison(
                instance_name=instance.name,
                dp_dp=dp_dp,
                dp_ip=dp_ip,
            )
        )
    return comparisons


def print_summary_table(comparisons: Sequence[InstanceComparison]) -> None:
    header = (
        f"{'Instance':<18} {'DP/DP (s)':>12} {'DP/IP (s)':>12} "
        f"{'Winner':>10} {'ObjMatch':>10}"
    )
    print(header)
    print("-" * len(header))
    for comparison in comparisons:
        objective_match = comparison.objective_match
        objective_text = "n/a" if objective_match is None else str(objective_match)
        print(
            f"{comparison.instance_name:<18} "
            f"{comparison.dp_dp.wall_clock_seconds:>12.4f} "
            f"{comparison.dp_ip.wall_clock_seconds:>12.4f} "
            f"{comparison.winner_label:>10} "
            f"{objective_text:>10}"
        )


def save_csv(comparisons: Sequence[InstanceComparison], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "instance_name",
                "pipeline_label",
                "pricing_phase2",
                "wall_clock_seconds",
                "column_generation_seconds",
                "integer_objective",
                "root_lp_objective",
                "pricing_call_count",
                "total_columns",
                "master_iterations",
                "status",
            ]
        )
        for record in flatten_records(comparisons):
            writer.writerow(
                [
                    record.instance_name,
                    record.pipeline_label,
                    record.pricing_phase2,
                    f"{record.wall_clock_seconds:.10f}",
                    f"{record.column_generation_seconds:.10f}",
                    record.integer_objective,
                    record.root_lp_objective,
                    record.pricing_call_count,
                    record.total_columns,
                    record.master_iterations,
                    record.status,
                ]
            )


def flatten_records(comparisons: Sequence[InstanceComparison]) -> Iterable[SolverRunRecord]:
    for comparison in comparisons:
        yield comparison.dp_dp
        yield comparison.dp_ip


def create_runtime_chart(comparisons: Sequence[InstanceComparison], output_path: Path) -> Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)

    instance_names = [comparison.instance_name for comparison in comparisons]
    dp_dp_times = [comparison.dp_dp.wall_clock_seconds for comparison in comparisons]
    dp_ip_times = [comparison.dp_ip.wall_clock_seconds for comparison in comparisons]

    positions = np.arange(len(instance_names))
    width = 0.36

    fig, ax = plt.subplots(figsize=(max(10, len(instance_names) * 1.6), 6))
    bars_dp = ax.bar(
        positions - width / 2,
        dp_dp_times,
        width,
        label="DP/DP",
        color="#4C72B0",
    )
    bars_ip = ax.bar(
        positions + width / 2,
        dp_ip_times,
        width,
        label="DP/IP",
        color="#DD8452",
    )

    ax.set_title("Akca LRSP Runtime Comparison by Instance")
    ax.set_ylabel("Root-node wall-clock runtime (s)")
    ax.set_xlabel("Instance")
    ax.set_xticks(positions)
    ax.set_xticklabels(instance_names, rotation=25, ha="right")
    ax.grid(axis="y", alpha=0.25)
    ax.legend()

    ymax = max(dp_dp_times + dp_ip_times) if comparisons else 0.0
    padding = max(0.05 * ymax, 0.02)
    ax.set_ylim(0.0, ymax + 4 * padding if ymax > 0 else 1.0)

    for bar in list(bars_dp) + list(bars_ip):
        height = bar.get_height()
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            height + padding * 0.2,
            f"{height:.2f}s",
            ha="center",
            va="bottom",
            fontsize=9,
            rotation=90,
        )

    for position, comparison in zip(positions, comparisons):
        ax.text(
            position,
            max(comparison.dp_dp.wall_clock_seconds, comparison.dp_ip.wall_clock_seconds) + 1.8 * padding,
            f"Winner: {comparison.winner_label}",
            ha="center",
            va="bottom",
            fontsize=9,
            fontweight="bold",
            color="#2F5D50",
        )
        if comparison.objective_match is False:
            ax.text(
                position,
                max(comparison.dp_dp.wall_clock_seconds, comparison.dp_ip.wall_clock_seconds) + 0.7 * padding,
                "Objective mismatch",
                ha="center",
                va="bottom",
                fontsize=8,
                color="#B22222",
            )

    fig.text(
        0.01,
        0.01,
        "Lower is better. Phase 1 is DP in both pipelines; only Phase 2 changes.",
        ha="left",
        va="bottom",
        fontsize=9,
    )
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(output_path, dpi=200, bbox_inches="tight")
    plt.close(fig)
    return output_path


def main() -> None:
    args = parse_args()
    phase1_label_limit = None if args.phase1_label_limit <= 0 else args.phase1_label_limit
    comparisons = compare_instances(
        max_iterations=args.max_iterations,
        max_columns_per_facility=args.max_columns_per_facility,
        phase1_label_limit=phase1_label_limit,
        limit=args.limit,
    )
    print_summary_table(comparisons)
    save_csv(comparisons, args.csv_output)
    chart_path = create_runtime_chart(comparisons, args.output)
    print(f"\nSaved chart to {chart_path}")
    print(f"Saved raw measurements to {args.csv_output}")


if __name__ == "__main__":
    main()
