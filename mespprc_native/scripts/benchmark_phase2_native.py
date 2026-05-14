"""
Native MESPPRC benchmark: Phase 2 DP vs Phase 2 IP, both in C.

Phase 1 is run once per replicate inside the C library and the same Phase 1
result handle is fed to both Phase 2 solvers, so the only thing that varies
between the two timings is the Phase 2 algorithm.

Usage:
    python mespprc_native/scripts/benchmark_phase2_native.py \
        --start-n 3 --max-n 14 --replicates 3 --threshold-seconds 30
"""

from __future__ import annotations

import argparse
import math
import statistics
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mespprc import GeneratorConfig, generate_instance
import mespprc_native


def fmt_ms(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value:.3f}ms"


def fmt_obj(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value:.6f}"


def safe_mean(xs: list[float]) -> float | None:
    return statistics.fmean(xs) if xs else None


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Native (C) Phase 2 DP-vs-IP timing benchmark."
    )
    ap.add_argument("--start-n", type=int, default=3)
    ap.add_argument("--max-n", type=int, default=14)
    ap.add_argument("--replicates", type=int, default=3)
    ap.add_argument(
        "--threshold-seconds",
        type=float,
        default=30.0,
        help="Stop once average DP+IP wallclock per replicate exceeds this.",
    )
    ap.add_argument("--base-seed", type=int, default=12_345)
    ap.add_argument("--objective-tolerance", type=float, default=1e-6)
    args = ap.parse_args()

    print(
        f"{'n':>3} | {'rep':>3} | {'p1_ms':>9} | {'dp_ms':>9} | {'ip_ms':>9} | "
        f"{'p1_routes':>9} | {'ip_red':>6} | dp_obj           | ip_obj           | match"
    )
    print("-" * 110)

    summary_rows: list[tuple[int, float | None, float | None, float | None, int, int, int]] = []
    stop_reason: str | None = None
    n = args.start_n
    while n <= args.max_n:
        dp_times: list[float] = []
        ip_times: list[float] = []
        p1_times: list[float] = []
        match_count = 0
        compared = 0
        feas_count = 0
        for r in range(args.replicates):
            seed = args.base_seed + 1000 * n + r
            inst = generate_instance(GeneratorConfig(num_customers=n, seed=seed))
            tim = mespprc_native.time_phase2_dp_vs_ip(inst)

            p1_times.append(tim.phase1_ms)
            dp_times.append(tim.dp_ms)
            ip_times.append(tim.ip_ms)

            if tim.dp_feasible and tim.ip_feasible:
                feas_count += 1
                if tim.dp_objective is not None and tim.ip_objective is not None:
                    compared += 1
                    if abs(tim.dp_objective - tim.ip_objective) <= args.objective_tolerance:
                        match_count += 1
                        match_str = "ok"
                    else:
                        match_str = (
                            f"MISMATCH(diff={tim.dp_objective - tim.ip_objective:+.3e})"
                        )
                else:
                    match_str = "no_objs"
            elif not tim.dp_feasible and not tim.ip_feasible:
                match_str = (
                    f"both_infeas(dp={tim.dp_infeasibility_reason},"
                    f"ip={tim.ip_infeasibility_reason})"
                )
            else:
                match_str = (
                    f"feas_mismatch(dp={tim.dp_feasible},ip={tim.ip_feasible})"
                )

            print(
                f"{n:>3} | {r:>3} | {fmt_ms(tim.phase1_ms):>9} | "
                f"{fmt_ms(tim.dp_ms):>9} | {fmt_ms(tim.ip_ms):>9} | "
                f"{tim.phase1_route_count:>9} | {tim.ip_reduced_route_count:>6} | "
                f"{fmt_obj(tim.dp_objective):>16} | {fmt_obj(tim.ip_objective):>16} | "
                f"{match_str}"
            )

        avg_p1 = safe_mean(p1_times)
        avg_dp = safe_mean(dp_times)
        avg_ip = safe_mean(ip_times)
        summary_rows.append((n, avg_p1, avg_dp, avg_ip, match_count, compared, feas_count))
        winner = (
            "DP" if (avg_dp is not None and avg_ip is not None and avg_dp < avg_ip)
            else ("IP" if avg_dp is not None and avg_ip is not None else "?")
        )
        print(
            f"  [n={n} avg]  p1={fmt_ms(avg_p1)}  dp={fmt_ms(avg_dp)}  ip={fmt_ms(avg_ip)}  "
            f"winner={winner}  obj_match={match_count}/{compared}"
        )

        # Stopping rule: average DP+IP per replicate over threshold.
        if avg_dp is not None and avg_ip is not None:
            avg_total_s = (avg_dp + avg_ip + (avg_p1 or 0.0)) / 1000.0
            if avg_total_s > args.threshold_seconds:
                stop_reason = "avg_runtime_exceeded"
                break

        n += 1

    if stop_reason is None:
        stop_reason = f"reached --max-n={args.max_n}"

    print("\nNative DP-vs-IP summary")
    print(f"  Stop reason: {stop_reason}")
    print(
        f"  {'n':>3} | {'p1_ms':>9} | {'dp_ms':>9} | {'ip_ms':>9} | "
        f"{'dp/ip':>7} | obj_match"
    )
    for n_v, p1, dp, ip, m, c, _f in summary_rows:
        ratio = (
            f"{dp/ip:.2f}x" if (dp is not None and ip is not None and ip > 0) else "n/a"
        )
        print(
            f"  {n_v:>3} | {fmt_ms(p1):>9} | {fmt_ms(dp):>9} | {fmt_ms(ip):>9} | "
            f"{ratio:>7} | {m}/{c}"
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
