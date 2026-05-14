from __future__ import annotations

import argparse
import math
import traceback
from dataclasses import dataclass
from statistics import fmean
from time import perf_counter
from typing import Iterable, List, Sequence

from mespprc import (
    GeneratorConfig,
    Phase1Solver,
    Phase2DPSolver,
    Phase2IPSolver,
    generate_instance,
)


@dataclass(slots=True)
class BenchmarkConfig:
    """
    Benchmark configuration for the benchmark harness.

    Timing boundaries:
    - instance generation is untimed
    - timing starts immediately before Phase 1
    - timing stops immediately after Phase 2

    In `compare` mode:
    - DP and IP use the same generated instance per replicate
    - Phase 1 is run once because it is deterministic for the same instance
    - the same exported Phase 1 route pool is then passed to both Phase 2 solvers
    - fresh solver objects are still created for both Phase 2 methods

    In `ip_only` mode:
    - Phase 1 is still run once per replicate
    - only the IP Phase 2 pipeline is timed and reported

    Default benchmark policy:
    - DP is run only through `dp_max_n`
    - IP continues until its average full-pipeline runtime exceeds the threshold
    """

    start_n: int = 1
    replicates_per_n: int = 5
    stop_average_total_seconds: float = 20.0
    solver_mode: str = "compare"
    base_seed: int = 12_345
    objective_tolerance: float = 1e-6
    dp_max_n: int = 8
    max_n: int | None = None
    show_plots: bool = True
    include_phase2_plot: bool = True
    save_plot_prefix: str | None = None

    def seed_for(self, n_customers: int, replicate_index: int) -> int:
        return self.base_seed + 1000 * n_customers + replicate_index


@dataclass(slots=True)
class PipelineRunResult:
    phase1_time: float | None
    phase2_time: float | None
    total_time: float | None
    route_count: int | None
    status: str
    feasible: bool
    objective: float | None
    error: str | None


@dataclass(slots=True)
class ReplicateBenchmarkResult:
    n_customers: int
    replicate_index: int
    seed: int
    phase1_time_dp: float | None
    phase2_time_dp: float | None
    total_time_dp: float | None
    phase1_time_ip: float | None
    phase2_time_ip: float | None
    total_time_ip: float | None
    number_of_routes_generated_dp: int | None
    number_of_routes_generated_ip: int | None
    dp_status: str
    ip_status: str
    dp_objective: float | None
    ip_objective: float | None
    objective_match: bool | None
    dp_feasible: bool
    ip_feasible: bool
    dp_error: str | None
    ip_error: str | None


@dataclass(slots=True)
class AveragedBenchmarkResult:
    n_customers: int
    avg_phase1_time_dp: float | None
    avg_phase2_time_dp: float | None
    avg_total_time_dp: float | None
    avg_phase1_time_ip: float | None
    avg_phase2_time_ip: float | None
    avg_total_time_ip: float | None
    avg_route_count_dp: float | None
    avg_route_count_ip: float | None
    dp_successful_replicates: int
    ip_successful_replicates: int
    dp_feasible_replicates: int
    ip_feasible_replicates: int
    comparable_objective_count: int
    matching_objective_count: int
    mismatch_count: int


def run_shared_phase1(instance) -> tuple[PipelineRunResult, object] | tuple[PipelineRunResult, None]:
    phase1_start = perf_counter()
    try:
        phase1_result = Phase1Solver(instance).solve()
    except Exception as exc:  # pragma: no cover - defensive benchmark harness
        phase1_time = perf_counter() - phase1_start
        return PipelineRunResult(
            phase1_time=phase1_time,
            phase2_time=None,
            total_time=phase1_time,
            route_count=None,
            status="phase1_error",
            feasible=False,
            objective=None,
            error=f"{type(exc).__name__}: {exc}",
        )
    phase1_time = perf_counter() - phase1_start

    return PipelineRunResult(
        phase1_time=phase1_time,
        phase2_time=None,
        total_time=phase1_time,
        route_count=len(phase1_result.feasible_routes),
        status="phase1_complete",
        feasible=bool(phase1_result.feasible_routes),
        objective=None,
        error=None,
    ), phase1_result


def run_phase2_dp_pipeline(
    instance,
    *,
    phase1_time: float,
    feasible_routes,
    route_count: int,
) -> PipelineRunResult:
    phase2_start = perf_counter()
    try:
        phase2_result = Phase2DPSolver(instance).solve(feasible_routes)
    except Exception as exc:  # pragma: no cover - defensive benchmark harness
        phase2_time = perf_counter() - phase2_start
        return PipelineRunResult(
            phase1_time=phase1_time,
            phase2_time=phase2_time,
            total_time=phase1_time + phase2_time,
            route_count=route_count,
            status="phase2_error",
            feasible=False,
            objective=None,
            error=f"{type(exc).__name__}: {exc}",
        )
    phase2_time = perf_counter() - phase2_start

    return PipelineRunResult(
        phase1_time=phase1_time,
        phase2_time=phase2_time,
        total_time=phase1_time + phase2_time,
        route_count=route_count,
        status=phase2_result.status,
        feasible=bool(phase2_result.is_feasible and phase2_result.coverage_complete),
        objective=phase2_result.total_cost,
        error=None,
    )


def run_phase2_ip_pipeline(
    instance,
    *,
    phase1_time: float,
    feasible_routes,
    route_count: int,
) -> PipelineRunResult:
    phase2_start = perf_counter()
    try:
        phase2_result = Phase2IPSolver(instance).solve(
            feasible_routes,
            collect_diagnostics=False,
        )
    except Exception as exc:  # pragma: no cover - defensive benchmark harness
        phase2_time = perf_counter() - phase2_start
        return PipelineRunResult(
            phase1_time=phase1_time,
            phase2_time=phase2_time,
            total_time=phase1_time + phase2_time,
            route_count=route_count,
            status="phase2_error",
            feasible=False,
            objective=None,
            error=f"{type(exc).__name__}: {exc}",
        )
    phase2_time = perf_counter() - phase2_start

    objective = (
        phase2_result.total_cost
        if phase2_result.total_cost is not None
        else phase2_result.objective_value
    )
    return PipelineRunResult(
        phase1_time=phase1_time,
        phase2_time=phase2_time,
        total_time=phase1_time + phase2_time,
        route_count=route_count,
        status=phase2_result.status,
        feasible=bool(phase2_result.is_feasible and phase2_result.coverage_complete),
        objective=objective,
        error=None,
    )


def run_single_replicate(
    *,
    benchmark_config: BenchmarkConfig,
    n_customers: int,
    replicate_index: int,
) -> ReplicateBenchmarkResult:
    seed = benchmark_config.seed_for(n_customers, replicate_index)
    instance = generate_instance(
        GeneratorConfig(num_customers=n_customers, seed=seed),
    )

    shared_phase1_run, phase1_result = run_shared_phase1(instance)
    if shared_phase1_run.error is not None:
        return ReplicateBenchmarkResult(
            n_customers=n_customers,
            replicate_index=replicate_index,
            seed=seed,
            phase1_time_dp=shared_phase1_run.phase1_time,
            phase2_time_dp=None,
            total_time_dp=shared_phase1_run.total_time,
            phase1_time_ip=shared_phase1_run.phase1_time,
            phase2_time_ip=None,
            total_time_ip=shared_phase1_run.total_time,
            number_of_routes_generated_dp=shared_phase1_run.route_count,
            number_of_routes_generated_ip=shared_phase1_run.route_count,
            dp_status=shared_phase1_run.status,
            ip_status=shared_phase1_run.status,
            dp_objective=None,
            ip_objective=None,
            objective_match=None,
            dp_feasible=False,
            ip_feasible=False,
            dp_error=shared_phase1_run.error,
            ip_error=shared_phase1_run.error,
        )

    ip_result = run_phase2_ip_pipeline(
        instance,
        phase1_time=shared_phase1_run.phase1_time or 0.0,
        feasible_routes=phase1_result.feasible_routes,
        route_count=shared_phase1_run.route_count or 0,
    )

    dp_enabled = (
        benchmark_config.solver_mode == "compare"
        and n_customers <= benchmark_config.dp_max_n
    )

    if dp_enabled:
        dp_result = run_phase2_dp_pipeline(
            instance,
            phase1_time=shared_phase1_run.phase1_time or 0.0,
            feasible_routes=phase1_result.feasible_routes,
            route_count=shared_phase1_run.route_count or 0,
        )
        objective_match = compare_objectives(
            dp_result.objective,
            ip_result.objective,
            dp_result.feasible,
            ip_result.feasible,
            benchmark_config.objective_tolerance,
        )
    else:
        dp_result = PipelineRunResult(
            phase1_time=None,
            phase2_time=None,
            total_time=None,
            route_count=None,
            status="skipped",
            feasible=False,
            objective=None,
            error=None,
        )
        objective_match = None

    return ReplicateBenchmarkResult(
        n_customers=n_customers,
        replicate_index=replicate_index,
        seed=seed,
        phase1_time_dp=dp_result.phase1_time,
        phase2_time_dp=dp_result.phase2_time,
        total_time_dp=dp_result.total_time,
        phase1_time_ip=ip_result.phase1_time,
        phase2_time_ip=ip_result.phase2_time,
        total_time_ip=ip_result.total_time,
        number_of_routes_generated_dp=dp_result.route_count,
        number_of_routes_generated_ip=ip_result.route_count,
        dp_status=dp_result.status,
        ip_status=ip_result.status,
        dp_objective=dp_result.objective,
        ip_objective=ip_result.objective,
        objective_match=objective_match,
        dp_feasible=dp_result.feasible,
        ip_feasible=ip_result.feasible,
        dp_error=dp_result.error,
        ip_error=ip_result.error,
    )


def summarize_results_for_n(
    n_customers: int,
    replicate_results: Sequence[ReplicateBenchmarkResult],
) -> AveragedBenchmarkResult:
    return AveragedBenchmarkResult(
        n_customers=n_customers,
        avg_phase1_time_dp=average_or_none(result.phase1_time_dp for result in replicate_results),
        avg_phase2_time_dp=average_or_none(result.phase2_time_dp for result in replicate_results),
        avg_total_time_dp=average_or_none(result.total_time_dp for result in replicate_results),
        avg_phase1_time_ip=average_or_none(result.phase1_time_ip for result in replicate_results),
        avg_phase2_time_ip=average_or_none(result.phase2_time_ip for result in replicate_results),
        avg_total_time_ip=average_or_none(result.total_time_ip for result in replicate_results),
        avg_route_count_dp=average_or_none(
            float(result.number_of_routes_generated_dp)
            if result.number_of_routes_generated_dp is not None
            else None
            for result in replicate_results
        ),
        avg_route_count_ip=average_or_none(
            float(result.number_of_routes_generated_ip)
            if result.number_of_routes_generated_ip is not None
            else None
            for result in replicate_results
        ),
        dp_successful_replicates=sum(result.total_time_dp is not None for result in replicate_results),
        ip_successful_replicates=sum(result.total_time_ip is not None for result in replicate_results),
        dp_feasible_replicates=sum(result.dp_feasible for result in replicate_results),
        ip_feasible_replicates=sum(result.ip_feasible for result in replicate_results),
        comparable_objective_count=sum(result.objective_match is not None for result in replicate_results),
        matching_objective_count=sum(result.objective_match is True for result in replicate_results),
        mismatch_count=sum(result.objective_match is False for result in replicate_results),
    )


def average_or_none(values: Iterable[float | None]) -> float | None:
    realized = [value for value in values if value is not None]
    if not realized:
        return None
    return fmean(realized)


def compare_objectives(
    dp_objective: float | None,
    ip_objective: float | None,
    dp_feasible: bool,
    ip_feasible: bool,
    tolerance: float,
) -> bool | None:
    if not (dp_feasible and ip_feasible):
        return None
    if dp_objective is None or ip_objective is None:
        return False
    return math.isclose(dp_objective, ip_objective, rel_tol=tolerance, abs_tol=tolerance)


def print_replicate_progress(result: ReplicateBenchmarkResult, total_replicates: int) -> None:
    prefix = (
        f"[n={result.n_customers:>3} rep={result.replicate_index + 1}/{total_replicates} "
        f"seed={result.seed}]"
    )
    segments: List[str] = []
    if result.dp_status != "skipped":
        segments.append(
            f"DP total={format_seconds(result.total_time_dp)} "
            f"(p1={format_seconds(result.phase1_time_dp)}, p2={format_seconds(result.phase2_time_dp)}, "
            f"routes={format_count(result.number_of_routes_generated_dp)}, status={result.dp_status})"
        )
    segments.append(
        f"IP total={format_seconds(result.total_time_ip)} "
        f"(p1={format_seconds(result.phase1_time_ip)}, p2={format_seconds(result.phase2_time_ip)}, "
        f"routes={format_count(result.number_of_routes_generated_ip)}, status={result.ip_status})"
    )
    if result.dp_status != "skipped":
        segments.append(
            f"dp_feasible={result.dp_feasible} ip_feasible={result.ip_feasible} "
            f"objective_match={format_objective_match(result.objective_match)}"
        )
    else:
        segments.append(f"ip_feasible={result.ip_feasible}")
    print(f"{prefix} " + " | ".join(segments))
    if result.dp_error:
        print(f"  DP error: {result.dp_error}")
    if result.ip_error:
        print(f"  IP error: {result.ip_error}")
    if result.dp_status != "skipped" and result.dp_feasible != result.ip_feasible:
        print("  Feasibility mismatch detected between DP and IP.")
    elif result.dp_status != "skipped" and result.objective_match is False:
        print(
            "  Objective mismatch detected: "
            f"dp_objective={format_objective(result.dp_objective)} "
            f"ip_objective={format_objective(result.ip_objective)}"
        )


def print_n_summary(
    summary: AveragedBenchmarkResult,
    threshold_seconds: float,
    solver_mode: str,
    dp_max_n: int,
) -> None:
    if solver_mode == "compare":
        print(
            f"[n={summary.n_customers:>3}] "
            f"avg_total_dp={format_seconds(summary.avg_total_time_dp)} "
            f"avg_total_ip={format_seconds(summary.avg_total_time_ip)} "
            f"avg_routes_dp={format_float(summary.avg_route_count_dp)} "
            f"avg_routes_ip={format_float(summary.avg_route_count_ip)} "
            f"objective_matches={summary.matching_objective_count}/{summary.comparable_objective_count}"
        )
        print(
            f"  DP successful={summary.dp_successful_replicates}, feasible={summary.dp_feasible_replicates}; "
            f"IP successful={summary.ip_successful_replicates}, feasible={summary.ip_feasible_replicates}; "
            f"DP capped at n={dp_max_n}; IP avg threshold={threshold_seconds:.2f}s"
        )
        return

    print(
        f"[n={summary.n_customers:>3}] "
        f"avg_total_ip={format_seconds(summary.avg_total_time_ip)} "
        f"avg_phase2_ip={format_seconds(summary.avg_phase2_time_ip)} "
        f"avg_routes_ip={format_float(summary.avg_route_count_ip)}"
    )
    print(
        f"  IP successful={summary.ip_successful_replicates}, feasible={summary.ip_feasible_replicates}; "
        f"single-trial threshold={threshold_seconds:.2f}s"
    )


def stopping_reason(
    summary: AveragedBenchmarkResult,
    threshold_seconds: float,
    solver_mode: str,
    dp_max_n: int,
) -> str | None:
    ip_exceeded = (
        summary.avg_total_time_ip is not None
        and summary.avg_total_time_ip > threshold_seconds
    )
    if ip_exceeded:
        return "ip_avg_runtime_exceeded"
    if summary.ip_successful_replicates == 0:
        return "ip_no_successful_runs"
    if (
        solver_mode == "compare"
        and summary.n_customers <= dp_max_n
        and summary.dp_successful_replicates == 0
    ):
        return "dp_no_successful_runs"
    return None


def plot_benchmark_results(
    *,
    summaries: Sequence[AveragedBenchmarkResult],
    benchmark_config: BenchmarkConfig,
) -> None:
    try:
        import matplotlib.pyplot as plt
    except ModuleNotFoundError as exc:  # pragma: no cover - environment dependent
        print(
            "matplotlib is not installed, so plots were skipped. "
            "Install project requirements to enable plotting."
        )
        print(f"Plotting import error: {type(exc).__name__}: {exc}")
        return

    n_values = [summary.n_customers for summary in summaries]
    avg_total_ip = [nan_if_none(summary.avg_total_time_ip) for summary in summaries]
    avg_phase2_ip = [nan_if_none(summary.avg_phase2_time_ip) for summary in summaries]
    avg_total_dp = [nan_if_none(summary.avg_total_time_dp) for summary in summaries]
    avg_phase2_dp = [nan_if_none(summary.avg_phase2_time_dp) for summary in summaries]

    figure_count = 2 if benchmark_config.include_phase2_plot else 1
    fig, axes = plt.subplots(figure_count, 1, figsize=(10, 4.5 * figure_count), sharex=True)
    if figure_count == 1:
        axes = [axes]

    if benchmark_config.solver_mode == "compare":
        axes[0].plot(n_values, avg_total_dp, marker="o", label="DP total runtime")
    axes[0].plot(n_values, avg_total_ip, marker="s", label="IP total runtime")
    axes[0].set_ylabel("Average total runtime (s)")
    axes[0].set_title(
        "Phase 2 Benchmark: Total Runtime"
        if benchmark_config.solver_mode == "compare"
        else "Phase 2 Benchmark: IP Total Runtime"
    )
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()

    if benchmark_config.include_phase2_plot:
        if benchmark_config.solver_mode == "compare":
            axes[1].plot(n_values, avg_phase2_dp, marker="o", label="DP Phase 2 runtime")
        axes[1].plot(n_values, avg_phase2_ip, marker="s", label="IP Phase 2 runtime")
        axes[1].set_ylabel("Average Phase 2 runtime (s)")
        axes[1].set_title(
            "Phase 2-Only Runtime"
            if benchmark_config.solver_mode == "compare"
            else "IP Phase 2 Runtime"
        )
        axes[1].grid(True, alpha=0.3)
        axes[1].legend()
        axes[1].set_xlabel("Number of customers")
    else:
        axes[0].set_xlabel("Number of customers")

    fig.tight_layout()

    if benchmark_config.save_plot_prefix:
        output_path = f"{benchmark_config.save_plot_prefix}_benchmark.png"
        fig.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Saved benchmark plot to {output_path}")

    if benchmark_config.show_plots:
        plt.show()
    else:
        plt.close(fig)


def nan_if_none(value: float | None) -> float:
    return math.nan if value is None else value


def format_seconds(value: float | None) -> str:
    return "n/a" if value is None else f"{value:.4f}s"


def format_count(value: int | None) -> str:
    return "n/a" if value is None else str(value)


def format_float(value: float | None) -> str:
    return "n/a" if value is None else f"{value:.2f}"


def format_objective(value: float | None) -> str:
    return "n/a" if value is None else f"{value:.6f}"


def format_objective_match(value: bool | None) -> str:
    if value is None:
        return "n/a"
    return str(value)


def run_benchmark(benchmark_config: BenchmarkConfig) -> tuple[List[ReplicateBenchmarkResult], List[AveragedBenchmarkResult], str | None]:
    replicate_results: List[ReplicateBenchmarkResult] = []
    averaged_results: List[AveragedBenchmarkResult] = []
    n_customers = benchmark_config.start_n
    stop_reason: str | None = None

    while benchmark_config.max_n is None or n_customers <= benchmark_config.max_n:
        current_n_results: List[ReplicateBenchmarkResult] = []
        for replicate_index in range(benchmark_config.replicates_per_n):
            try:
                result = run_single_replicate(
                    benchmark_config=benchmark_config,
                    n_customers=n_customers,
                    replicate_index=replicate_index,
                )
            except Exception as exc:  # pragma: no cover - defensive benchmark harness
                seed = benchmark_config.seed_for(n_customers, replicate_index)
                error_message = f"{type(exc).__name__}: {exc}"
                print(
                    f"[n={n_customers:>3} rep={replicate_index + 1}/{benchmark_config.replicates_per_n} seed={seed}] "
                    f"benchmark harness error: {error_message}"
                )
                traceback.print_exc()
                result = ReplicateBenchmarkResult(
                    n_customers=n_customers,
                    replicate_index=replicate_index,
                    seed=seed,
                    phase1_time_dp=None,
                    phase2_time_dp=None,
                    total_time_dp=None,
                    phase1_time_ip=None,
                    phase2_time_ip=None,
                    total_time_ip=None,
                    number_of_routes_generated_dp=None,
                    number_of_routes_generated_ip=None,
                    dp_status="benchmark_error",
                    ip_status="benchmark_error",
                    dp_objective=None,
                    ip_objective=None,
                    objective_match=None,
                    dp_feasible=False,
                    ip_feasible=False,
                    dp_error=error_message,
                    ip_error=error_message,
                )

            current_n_results.append(result)
            replicate_results.append(result)
            print_replicate_progress(result, benchmark_config.replicates_per_n)

        summary = summarize_results_for_n(n_customers, current_n_results)
        averaged_results.append(summary)
        print_n_summary(
            summary,
            benchmark_config.stop_average_total_seconds,
            benchmark_config.solver_mode,
            benchmark_config.dp_max_n,
        )

        stop_reason = stopping_reason(
            summary,
            benchmark_config.stop_average_total_seconds,
            benchmark_config.solver_mode,
            benchmark_config.dp_max_n,
        )
        if stop_reason is not None:
            break

        n_customers += 1

    return replicate_results, averaged_results, stop_reason


def print_final_summary(
    *,
    replicate_results: Sequence[ReplicateBenchmarkResult],
    averaged_results: Sequence[AveragedBenchmarkResult],
    stop_reason_text: str | None,
    benchmark_config: BenchmarkConfig,
) -> None:
    if not averaged_results:
        print("No benchmark rounds were completed.")
        return

    largest_completed_n = averaged_results[-1].n_customers
    comparable_count = sum(result.objective_match is not None for result in replicate_results)
    match_count = sum(result.objective_match is True for result in replicate_results)
    mismatch_count = sum(
        result.objective_match is False for result in replicate_results
    )
    feasibility_mismatch_count = sum(
        result.dp_status != "skipped" and result.dp_feasible != result.ip_feasible
        for result in replicate_results
    )

    if stop_reason_text is None:
        if benchmark_config.max_n is not None:
            stop_reason_text = f"reached configured max_n={benchmark_config.max_n}"
        else:
            stop_reason_text = "benchmark completed without crossing the threshold"

    print("\nBenchmark summary")
    print(f"  Largest completed n: {largest_completed_n}")
    print(f"  Stop reason: {stop_reason_text}")
    if benchmark_config.solver_mode == "ip_only":
        ip_successes = sum(result.total_time_ip is not None for result in replicate_results)
        ip_feasible = sum(result.ip_feasible for result in replicate_results)
        print(f"  IP successful runs: {ip_successes}")
        print(f"  IP feasible runs: {ip_feasible}")
        return

    print(f"  DP was run through n={benchmark_config.dp_max_n}")
    print(
        f"  Objective matches: {match_count}/{comparable_count} comparable feasible replicate pairs"
    )
    print(f"  Objective mismatches: {mismatch_count}")
    print(f"  Feasibility mismatches: {feasibility_mismatch_count}")


def parse_args() -> BenchmarkConfig:
    parser = argparse.ArgumentParser(
        description="Benchmark Phase 2 DP vs IP on generated instances.",
    )
    parser.add_argument("--start-n", type=int, default=1)
    parser.add_argument("--replicates", type=int, default=5)
    parser.add_argument("--threshold-seconds", type=float, default=20.0)
    parser.add_argument("--mode", choices=("ip_only", "compare"), default="compare")
    parser.add_argument("--base-seed", type=int, default=12_345)
    parser.add_argument("--objective-tolerance", type=float, default=1e-6)
    parser.add_argument("--dp-max-n", type=int, default=8)
    parser.add_argument("--max-n", type=int, default=None)
    parser.add_argument("--skip-plots", action="store_true")
    parser.add_argument("--skip-phase2-plot", action="store_true")
    parser.add_argument("--save-plot-prefix", type=str, default=None)
    args = parser.parse_args()

    return BenchmarkConfig(
        start_n=args.start_n,
        replicates_per_n=args.replicates,
        stop_average_total_seconds=args.threshold_seconds,
        solver_mode=args.mode,
        base_seed=args.base_seed,
        objective_tolerance=args.objective_tolerance,
        dp_max_n=args.dp_max_n,
        max_n=args.max_n,
        show_plots=not args.skip_plots,
        include_phase2_plot=not args.skip_phase2_plot,
        save_plot_prefix=args.save_plot_prefix,
    )


def main() -> None:
    benchmark_config = parse_args()
    replicate_results, averaged_results, stop_reason_text = run_benchmark(benchmark_config)
    print_final_summary(
        replicate_results=replicate_results,
        averaged_results=averaged_results,
        stop_reason_text=stop_reason_text,
        benchmark_config=benchmark_config,
    )
    if averaged_results and (
        benchmark_config.show_plots or benchmark_config.save_plot_prefix is not None
    ):
        plot_benchmark_results(
            summaries=averaged_results,
            benchmark_config=benchmark_config,
        )


if __name__ == "__main__":
    main()
