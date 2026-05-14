"""
Scan the repository for valid LRSP test instances.

A "valid LRSP instance" is one that exposes the full
Location + Routing + Scheduling structure used in Asta's dissertation:

- multiple candidate facilities with opening costs (location decision),
- elementary vehicle routes from a chosen facility back to the same facility
  serving disjoint customer sets (routing decision),
- a per-vehicle duty / route time limit on line 4 of the file
  (scheduling decision: each vehicle is constrained by how long it can be on duty).

Pure LRP, VRP, ESPPRC and MESPPRC files are rejected because they lack at least
one of the three components, even when they live next to LRSP files in the same
folder.

Usage:
    python scripts/discover_lrsp_instances.py
    python scripts/discover_lrsp_instances.py --root "Akca Repo"
    python scripts/discover_lrsp_instances.py --json results/lrsp_pricing_comparison/instance_inventory.json
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Iterable, List

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from lrsp_solver.instance import _parse_floats  # type: ignore[attr-defined]


_EXCLUDED_NAME_TOKENS = (
    "vrp",       # pure VRP files
    "espprc",
    "mespprc",
    "cvrp",
)
_EXCLUDED_PATH_TOKENS = (
    "lrp_dip_instances",  # Cordeau LRP-DIP — no scheduling
    "lrpaper",
    "vrpinforms05",
    "vrplib",
)


@dataclass(slots=True)
class InstanceReport:
    name: str
    path: str
    is_lrsp: bool
    reason: str
    num_facilities: int | None = None
    num_customers: int | None = None
    num_vehicles_per_facility: int | None = None
    vehicle_capacity: float | None = None
    facility_capacity: float | None = None
    vehicle_time_limit: float | None = None
    notes: List[str] = field(default_factory=list)


def discover(roots: Iterable[Path]) -> List[InstanceReport]:
    candidates: List[Path] = []
    for root in roots:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*.txt")):
            candidates.append(path)

    seen_basenames: set[str] = set()
    reports: List[InstanceReport] = []

    for path in candidates:
        rel_path = path.relative_to(REPO_ROOT) if path.is_relative_to(REPO_ROOT) else path
        report = InstanceReport(name=path.stem, path=str(rel_path), is_lrsp=False, reason="")

        excluded_path = next(
            (token for token in _EXCLUDED_PATH_TOKENS if token in str(rel_path).lower()),
            None,
        )
        if excluded_path is not None:
            report.reason = f"excluded_path_marker:{excluded_path}"
            reports.append(report)
            continue

        excluded_name = next(
            (token for token in _EXCLUDED_NAME_TOKENS if token in path.stem.lower()),
            None,
        )
        if excluded_name is not None:
            report.reason = f"excluded_name_marker:{excluded_name}"
            reports.append(report)
            continue

        try:
            lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
        except UnicodeDecodeError:
            report.reason = "binary_or_non_utf8_file"
            reports.append(report)
            continue

        if len(lines) < 4:
            report.reason = "fewer_than_four_header_lines"
            reports.append(report)
            continue

        header = _try_parse_floats(lines[0])
        if header is None or len(header) != 2:
            report.reason = "header_line_does_not_match_lrsp_format"
            reports.append(report)
            continue

        num_facilities = int(header[0])
        num_customers = int(header[1])
        if num_facilities <= 0 or num_customers <= 0:
            report.reason = "non_positive_counts_on_header_line"
            reports.append(report)
            continue
        report.num_facilities = num_facilities
        report.num_customers = num_customers

        opening_costs = _try_parse_floats(lines[1])
        if opening_costs is None or len(opening_costs) != num_facilities:
            report.reason = "opening_costs_count_mismatch"
            reports.append(report)
            continue

        vehicle_header = _try_parse_floats(lines[2])
        if vehicle_header is None or len(vehicle_header) < 1:
            report.reason = "missing_vehicle_fixed_cost_line"
            reports.append(report)
            continue
        if len(vehicle_header) >= 2:
            report.num_vehicles_per_facility = int(vehicle_header[1])

        capacity_header = _try_parse_floats(lines[3])
        if capacity_header is None or len(capacity_header) < 3:
            report.reason = "missing_scheduling_field_on_line_4"
            reports.append(report)
            continue

        report.vehicle_capacity = float(capacity_header[0])
        report.facility_capacity = float(capacity_header[1])
        report.vehicle_time_limit = float(capacity_header[2])
        if report.vehicle_time_limit <= 0:
            report.reason = "non_positive_vehicle_time_limit"
            reports.append(report)
            continue

        body_required = num_customers + num_facilities
        if len(lines) < 4 + body_required:
            report.reason = "body_row_count_mismatch"
            reports.append(report)
            continue

        # Sanity check the first customer row to make sure the format truly fits LRSP.
        first_customer_row = _try_parse_floats(lines[4])
        if first_customer_row is None or len(first_customer_row) < 5:
            report.reason = "customer_row_does_not_have_5_columns"
            reports.append(report)
            continue

        if path.name in seen_basenames:
            report.reason = (
                f"duplicate_basename_already_indexed (kept earlier copy)"
            )
            reports.append(report)
            continue

        seen_basenames.add(path.name)
        report.is_lrsp = True
        report.reason = (
            "valid_lrsp:has_facilities_routes_and_per_vehicle_time_limit"
        )
        reports.append(report)

    return reports


def _try_parse_floats(line: str) -> list[float] | None:
    try:
        return _parse_floats(line)
    except ValueError:
        return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--root",
        action="append",
        type=Path,
        default=None,
        help="Repository subfolder(s) to scan. Default: 'Akca Repo'.",
    )
    parser.add_argument(
        "--json",
        type=Path,
        default=None,
        help="Optional JSON output path for the discovery report.",
    )
    parser.add_argument(
        "--include-rejected",
        action="store_true",
        help="Print rejected files alongside accepted LRSP instances.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    roots = args.root or [REPO_ROOT / "Akca Repo"]
    reports = discover(roots)

    accepted = [r for r in reports if r.is_lrsp]
    rejected = [r for r in reports if not r.is_lrsp]

    print(f"Scanned {len(reports)} candidate files; accepted {len(accepted)} as LRSP.")
    print()
    print("Accepted LRSP instances:")
    print("-" * 80)
    for report in sorted(accepted, key=lambda r: (r.num_customers or 0, r.name)):
        print(
            f"  {report.name:25}  "
            f"customers={report.num_customers:<3} "
            f"facilities={report.num_facilities:<2} "
            f"vehicle_cap={report.vehicle_capacity:<6} "
            f"time_limit={report.vehicle_time_limit:<6} "
            f"path={report.path}"
        )

    if args.include_rejected:
        print()
        print("Rejected files (sample):")
        print("-" * 80)
        for report in rejected[:30]:
            print(f"  {report.name:25}  reason={report.reason}  path={report.path}")
        if len(rejected) > 30:
            print(f"  ... and {len(rejected) - 30} more rejected files")

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(
            json.dumps(
                {
                    "accepted": [asdict(r) for r in accepted],
                    "rejected": [asdict(r) for r in rejected],
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        print()
        print(f"Discovery report written to {args.json}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
