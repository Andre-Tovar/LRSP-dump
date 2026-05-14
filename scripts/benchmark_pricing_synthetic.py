"""
Phase 2 DP vs IP timing benchmark on synthetic LRSP instances.

The real Akca LRSP `.txt` instances stress the user's MESPPRC Phase 1 labeling
hard, which dominates wall-clock. To produce a clean DP-vs-IP comparison
without that confound, this script:

- builds a tightly-scoped synthetic LRSP instance at a configurable size,
- builds the per-facility reduced-cost pricing graph with zero duals,
- runs Phase 1 with a strict label limit so the route pool is small,
- times Phase 2 DP and Phase 2 IP head-to-head on the same route pool,
- repeats for several sizes and writes JSON / CSV / table.

Usage:
    python scripts/benchmark_pricing_synthetic.py
    python scripts/benchmark_pricing_synthetic.py --sizes 4,6,8,10 --replicates 2
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from time import perf_counter
from typing import List

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mespprc import Phase1Solver, Phase2DPSolver, Phase2IPSolver

from lrsp_solver import synthetic_instance
from lrsp_solver.column import MasterDuals
from lrsp_solver.pricing_graph import build_facility_pricing_graph


_DEFAULT_OUTPUT_DIR = REPO_ROOT / "results" / "lrsp_pricing_comparison" / "synthetic"


@dataclass(slots=True)
class FacilityRecord:
    facility_id: int
    phase1_route_count: int
    phase1_time: float
    phase2_dp_time: float
    phase2_ip_time: float
    phase2_dp_objective: float | None
    phase2_ip_objective: float | None


@dataclass(slots=True)
class SizeRecord:
    instance_name: str
    customer_count: int
    facility_count: int
    seed: int
    facilities: List[FacilityRecord] = field(default_factory=list)

    def aggregated(self) -> dict[str, object]:
        if not self.facilities:
            return {}
        n = len(self.facilities)
        total_p1 = sum(f.phase1_time for f in self.facilities)
        total_dp = sum(f.phase2_dp_time for f in self.facilities)
        total_ip = sum(f.phase2_ip_time for f in self.facilities)
        return {
            "customer_count": self.customer_count,
            "facility_count": self.facility_count,
            "seed": self.seed,
            "phase1_total_time": total_p1,
            "phase2_dp_total_time": total_dp,
            "phase2_ip_total_time": total_ip,
            "phase1_avg_time_per_facility": total_p1 / n,
            "phase2_dp_avg_time_per_facility": total_dp / n,
            "phase2_ip_avg_time_per_facility": total_ip / n,
            "ip_minus_dp_total": total_ip - total_dp,
            "speedup_dp_over_ip": (total_ip / total_dp) if total_dp > 0 else None,
        }


def _zero_duals(instance) -> MasterDuals:
    return MasterDuals(
        coverage={c.id: 0.0 for c in instance.customers},
        facility_capacity={f.id: 0.0 for f in instance.facilities},
        facility_customer_link={
            (c.id, f.id): 0.0 for f in instance.facilities for c in instance.customers
        },
        min_open_facilities=0.0,
    )


def benchmark_size(
    *,
    customer_count: int,
    facility_count: int,
    seed: int,
    label_limit: int,
) -> SizeRecord:
    instance = synthetic_instance(
        name=f"synth_n{customer_count}_f{facility_count}_s{seed}",
        customer_count=customer_count,
        facility_count=facility_count,
        seed=seed,
    )
    duals = _zero_duals(instance)
    record = SizeRecord(
        instance_name=instance.name,
        customer_count=customer_count,
        facility_count=facility_count,
        seed=seed,
    )

    for facility in instance.facilities:
        graph = build_facility_pricing_graph(instance, facility, duals)
        phase1 = Phase1Solver(graph.pricing_instance, label_limit=label_limit)
        t0 = perf_counter()
        phase1_result = phase1.solve()
        phase1_time = perf_counter() - t0
        routes = list(phase1_result.feasible_routes)

        dp_solver = Phase2DPSolver(graph.pricing_instance)
        t0 = perf_counter()
        dp_result = dp_solver.solve(routes)
        dp_time = perf_counter() - t0

        ip_solver = Phase2IPSolver(graph.pricing_instance)
        t0 = perf_counter()
        ip_result = ip_solver.solve(routes, collect_diagnostics=False)
        ip_time = perf_counter() - t0

        record.facilities.append(
            FacilityRecord(
                facility_id=facility.id,
                phase1_route_count=len(routes),
                phase1_time=phase1_time,
                phase2_dp_time=dp_time,
                phase2_ip_time=ip_time,
                phase2_dp_objective=(
                    float(dp_result.total_cost) if dp_result.total_cost is not None else None
                ),
                phase2_ip_objective=(
                    float(ip_result.objective_value)
                    if ip_result.objective_value is not None
                    else None
                ),
            )
        )

    return record


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--sizes", default="4,6,8,10",
                        help="Comma-separated customer counts.")
    parser.add_argument("--facility-count", type=int, default=2)
    parser.add_argument("--replicates", type=int, default=1,
                        help="Number of seed replicates per size.")
    parser.add_argument("--label-limit", type=int, default=200)
    parser.add_argument("--output-dir", type=Path, default=_DEFAULT_OUTPUT_DIR)
    return parser.parse_args()


def _render_table(records: list[SizeRecord]) -> str:
    headers = [
        "instance",
        "n_cust",
        "n_fac",
        "seed",
        "phase1_s",
        "phase2_dp_s",
        "phase2_ip_s",
        "ip_minus_dp_s",
        "ip/dp ratio",
    ]
    rows: list[list[str]] = []
    for record in records:
        agg = record.aggregated()
        rows.append([
            record.instance_name,
            str(record.customer_count),
            str(record.facility_count),
            str(record.seed),
            f"{agg['phase1_total_time']:.4f}",
            f"{agg['phase2_dp_total_time']:.4f}",
            f"{agg['phase2_ip_total_time']:.4f}",
            f"{agg['ip_minus_dp_total']:+.4f}",
            (f"{agg['speedup_dp_over_ip']:.2f}"
             if isinstance(agg['speedup_dp_over_ip'], (int, float))
             else "-"),
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


def main() -> int:
    args = parse_args()
    sizes = [int(token) for token in args.sizes.split(",") if token.strip()]
    records: list[SizeRecord] = []
    for n in sizes:
        for replicate in range(args.replicates):
            seed = 1000 + replicate
            print(
                f"=> n={n} f={args.facility_count} seed={seed}",
                flush=True,
            )
            record = benchmark_size(
                customer_count=n,
                facility_count=args.facility_count,
                seed=seed,
                label_limit=args.label_limit,
            )
            records.append(record)
            agg = record.aggregated()
            print(
                f"   phase1={agg['phase1_total_time']:.3f}s  "
                f"phase2_dp={agg['phase2_dp_total_time']:.3f}s  "
                f"phase2_ip={agg['phase2_ip_total_time']:.3f}s  "
                f"ip-dp={agg['ip_minus_dp_total']:+.3f}s  "
                f"ratio={agg['speedup_dp_over_ip']}",
                flush=True,
            )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    json_path = args.output_dir / "synthetic_phase2_microbench.json"
    json_path.write_text(
        json.dumps([asdict(r) for r in records], indent=2),
        encoding="utf-8",
    )
    csv_path = args.output_dir / "synthetic_phase2_microbench.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "instance",
            "customer_count",
            "facility_count",
            "seed",
            "facility_id",
            "phase1_route_count",
            "phase1_time_s",
            "phase2_dp_time_s",
            "phase2_ip_time_s",
            "phase2_dp_objective",
            "phase2_ip_objective",
        ])
        for record in records:
            for fac in record.facilities:
                writer.writerow([
                    record.instance_name,
                    record.customer_count,
                    record.facility_count,
                    record.seed,
                    fac.facility_id,
                    fac.phase1_route_count,
                    f"{fac.phase1_time:.4f}",
                    f"{fac.phase2_dp_time:.4f}",
                    f"{fac.phase2_ip_time:.4f}",
                    fac.phase2_dp_objective,
                    fac.phase2_ip_objective,
                ])
    table_path = args.output_dir / "synthetic_phase2_microbench_table.txt"
    table_path.write_text(_render_table(records), encoding="utf-8")
    print()
    print(f"Wrote {len(records)} records to {args.output_dir}/")
    print("  - synthetic_phase2_microbench.json")
    print("  - synthetic_phase2_microbench.csv")
    print("  - synthetic_phase2_microbench_table.txt")
    print()
    print(_render_table(records))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
