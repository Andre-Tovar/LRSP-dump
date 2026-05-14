"""
CLI driver: solve the same LRSP instance with both pricing engines and print a
side-by-side comparison table.

Usage:
    python scripts/compare_pricing_engines.py path/to/instance.py
    python scripts/compare_pricing_engines.py path/to/instance.py --max-iterations 30
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from lrsp_solver import (
    LRSPSolverConfig,
    compare_pricing_engines,
    format_comparison_table,
    load_instance_from_module,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare DP- vs IP-based MESPPRC pricing on an LRSP instance.")
    parser.add_argument("instance", help="Path to a Python LRSP instance module.")
    parser.add_argument("--max-iterations", type=int, default=50)
    parser.add_argument("--max-columns-per-facility", type=int, default=5)
    parser.add_argument("--phase1-label-limit", type=int, default=None)
    parser.add_argument("--vehicle-fixed-cost", type=float, default=0.0)
    parser.add_argument("--vehicle-operating-cost", type=float, default=1.0)
    parser.add_argument("--vehicle-time-limit", type=float, default=None)
    parser.add_argument("--no-integer", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    instance = load_instance_from_module(
        args.instance,
        vehicle_fixed_cost=args.vehicle_fixed_cost,
        vehicle_operating_cost=args.vehicle_operating_cost,
        vehicle_time_limit=args.vehicle_time_limit,
    )
    config = LRSPSolverConfig(
        max_iterations=args.max_iterations,
        max_columns_per_facility=args.max_columns_per_facility,
        phase1_label_limit=args.phase1_label_limit,
        solve_integer_master=not args.no_integer,
    )
    comparison = compare_pricing_engines(instance, base_config=config)
    print(format_comparison_table([comparison]))
    if (
        comparison.dp_result.root_lp_objective is not None
        and comparison.ip_result.root_lp_objective is not None
    ):
        print()
        print(f"root LP match    : {comparison.root_lp_match}")
        print(f"integer match    : {comparison.integer_match}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
