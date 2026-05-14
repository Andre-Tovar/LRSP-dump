"""
CLI driver: solve one LRSP instance with the chosen pricing engine.

Usage:
    python scripts/run_lrsp.py path/to/instance.py --pricing dp
    python scripts/run_lrsp.py path/to/instance.py --pricing ip --max-iterations 30
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from lrsp_solver import (
    LRSPSolver,
    LRSPSolverConfig,
    load_instance_from_module,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Solve an LRSP instance.")
    parser.add_argument("instance", help="Path to a Python LRSP instance module.")
    parser.add_argument("--pricing", choices=("dp", "ip"), default="dp")
    parser.add_argument("--max-iterations", type=int, default=50)
    parser.add_argument("--max-columns-per-facility", type=int, default=5)
    parser.add_argument("--phase1-label-limit", type=int, default=None)
    parser.add_argument("--vehicle-fixed-cost", type=float, default=0.0)
    parser.add_argument("--vehicle-operating-cost", type=float, default=1.0)
    parser.add_argument("--vehicle-time-limit", type=float, default=None)
    parser.add_argument(
        "--no-integer", action="store_true", help="Skip the integer master step."
    )
    parser.add_argument("--cbc-msg", action="store_true")
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
        pricing=args.pricing,
        max_iterations=args.max_iterations,
        max_columns_per_facility=args.max_columns_per_facility,
        phase1_label_limit=args.phase1_label_limit,
        solve_integer_master=not args.no_integer,
        cbc_msg=args.cbc_msg,
    )
    solver = LRSPSolver(config)
    result = solver.solve(instance)

    print(f"instance         : {result.instance_name}")
    print(f"pricing engine   : {result.pricing_engine}")
    print(f"status           : {result.status}")
    print(f"reached optimality: {result.reached_optimality}")
    print(f"iterations       : {result.iteration_count}")
    print(f"pricing calls    : {result.pricing_call_count}")
    print(f"total columns    : {result.total_columns}")
    print(f"root LP objective: {result.root_lp_objective}")
    print(f"integer objective: {result.integer_objective}")
    print(f"master runtime   : {result.master_runtime:.4f}s")
    print(f"pricing runtime  : {result.pricing_runtime:.4f}s")
    print(f"total runtime    : {result.total_runtime:.4f}s")
    if result.failure_message:
        print(f"failure message  : {result.failure_message}")

    if result.integer_master is not None and result.integer_master.is_optimal:
        print()
        print("Selected columns:")
        for column in result.integer_master.selected_columns:
            print(
                f"  facility={column.facility_id} "
                f"covers={column.covered_customers} "
                f"cost={column.pairing_cost:.4f}"
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
