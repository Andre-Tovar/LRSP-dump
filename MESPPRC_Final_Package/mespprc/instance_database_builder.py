from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from dataclasses import asdict, dataclass
from pathlib import Path
from time import perf_counter
from typing import Any, Iterable, Mapping

from .instance import MESPPRCInstance
from .instance_database import (
    DATABASE_SCHEMA,
    DATABASE_SCHEMA_VERSION,
    DATABASE_ROOT,
    validate_database,
)
from .instance_generator import GeneratorConfig, generate_instance
from .instance_io import (
    canonical_instance_json,
    instance_from_dict,
    instance_to_dict,
    write_instance_json,
)
from .phase1 import Phase1Solver
from .phase2_dp import Phase2DPSolver
from .phase2_ip import Phase2IPSolver

OBJECTIVE_TOLERANCE = 1e-6
DP_COMPARE_MAX_CUSTOMERS = 8


@dataclass(frozen=True, slots=True)
class GeneratorProfile:
    name: str
    beta_route: float
    rho_capacity: float
    gamma_duty: float
    service_time_slope_per_customer: float = 0.1


@dataclass(frozen=True, slots=True)
class AcceptedInstance:
    instance_id: str
    instance: MESPPRCInstance
    snapshot: Mapping[str, Any]
    checksum_sha256: str
    manifest_entry: dict[str, Any]


DEFAULT_PROFILES: tuple[GeneratorProfile, ...] = (
    GeneratorProfile(
        name="easy",
        beta_route=1.15,
        rho_capacity=2.0,
        gamma_duty=1.0,
    ),
    GeneratorProfile(
        name="moderate",
        beta_route=1.65,
        rho_capacity=3.5,
        gamma_duty=0.92,
    ),
    GeneratorProfile(
        name="tight",
        beta_route=2.05,
        rho_capacity=4.5,
        gamma_duty=0.82,
    ),
)


def build_database(
    *,
    output_root: str | Path = DATABASE_ROOT,
    target_count: int = 72,
    min_customers: int = 4,
    max_customers: int = 15,
    base_seed: int = 12_345,
    max_seed_offset: int = 64,
    max_route_count: int = 75_000,
    profiles: Iterable[GeneratorProfile] = DEFAULT_PROFILES,
    clean: bool = True,
) -> list[dict[str, Any]]:
    """
    Build a deterministic database of validated MESPPRC benchmark instances.

    The builder keeps only solver-ready instances matching the acceptance policy
    in this module. It may evaluate more candidates than it writes because tight
    generated instances can be infeasible.
    """

    output_root = Path(output_root)
    instances_dir = output_root / "instances"
    if clean and output_root.exists():
        shutil.rmtree(output_root)
    instances_dir.mkdir(parents=True, exist_ok=True)

    profile_list = tuple(profiles)
    accepted_entries: list[dict[str, Any]] = []
    seen_ids: set[str] = set()

    for n_customers in range(min_customers, max_customers + 1):
        for profile in profile_list:
            accepted_for_bucket = 0
            for seed_offset in range(max_seed_offset):
                if len(accepted_entries) >= target_count:
                    break
                seed = _seed_for(base_seed, n_customers, profile.name, seed_offset)
                config = _config_for(n_customers, profile, seed)
                candidate = evaluate_candidate(
                    config=config,
                    difficulty=profile.name,
                    max_route_count=max_route_count,
                )
                if candidate is None:
                    continue
                if candidate.instance_id in seen_ids:
                    continue

                path = instances_dir / f"{candidate.instance_id}.json"
                write_instance_json(path, candidate.instance)
                entry = dict(candidate.manifest_entry)
                entry["path"] = f"instances/{path.name}"
                accepted_entries.append(entry)
                seen_ids.add(candidate.instance_id)
                accepted_for_bucket += 1

                if accepted_for_bucket >= _target_per_bucket(target_count, profile_list):
                    break
            if len(accepted_entries) >= target_count:
                break
        if len(accepted_entries) >= target_count:
            break

    manifest = {
        "schema": DATABASE_SCHEMA,
        "schema_version": DATABASE_SCHEMA_VERSION,
        "description": (
            "Validated MESPPRC benchmark instances stored as full graph JSON "
            "snapshots. Generator config is recorded for provenance only."
        ),
        "instance_count": len(accepted_entries),
        "acceptance_policy": {
            "validate_generated_instance": True,
            "validate_json_round_trip": True,
            "require_phase1_route_pool": True,
            "require_phase2_ip_feasible": True,
            "dp_compare_max_customers": DP_COMPARE_MAX_CUSTOMERS,
            "objective_tolerance": OBJECTIVE_TOLERANCE,
            "max_route_count": max_route_count,
        },
        "instances": sorted(
            accepted_entries,
            key=lambda entry: (
                entry["num_customers"],
                entry["difficulty"],
                entry["id"],
            ),
        ),
    }
    (output_root / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return manifest["instances"]


def evaluate_candidate(
    *,
    config: GeneratorConfig,
    difficulty: str,
    max_route_count: int,
) -> AcceptedInstance | None:
    """Return accepted instance metadata, or `None` if the candidate fails policy."""

    generation_start = perf_counter()
    try:
        instance = generate_instance(config)
        instance.validate()
    except Exception:
        return None
    generation_seconds = perf_counter() - generation_start

    phase1_start = perf_counter()
    try:
        phase1 = Phase1Solver(instance).solve()
    except Exception:
        return None
    phase1_seconds = perf_counter() - phase1_start

    required_customers = set(instance.required_customers())
    generated_coverage = (
        set().union(*(route.covered_customers for route in phase1.feasible_routes))
        if phase1.feasible_routes
        else set()
    )
    if not required_customers.issubset(generated_coverage):
        return None
    if len(phase1.feasible_routes) > max_route_count:
        return None

    ip_start = perf_counter()
    try:
        ip_result = Phase2IPSolver(instance).solve(
            phase1.feasible_routes,
            collect_diagnostics=False,
        )
    except Exception:
        return None
    ip_seconds = perf_counter() - ip_start
    if not (ip_result.is_feasible and ip_result.coverage_complete):
        return None

    ip_objective = ip_result.total_cost
    if ip_objective is None:
        ip_objective = ip_result.objective_value
    if ip_objective is None:
        return None

    dp_seconds: float | None = None
    dp_status: str | None = None
    dp_objective: float | None = None
    dp_objective_match: bool | None = None
    if config.num_customers <= DP_COMPARE_MAX_CUSTOMERS:
        dp_start = perf_counter()
        try:
            dp_result = Phase2DPSolver(instance).solve(phase1.feasible_routes)
        except Exception:
            return None
        dp_seconds = perf_counter() - dp_start
        dp_status = dp_result.status
        dp_objective = dp_result.total_cost
        if not (dp_result.is_feasible and dp_result.coverage_complete):
            return None
        if dp_objective is None:
            return None
        dp_objective_match = abs(dp_objective - ip_objective) <= OBJECTIVE_TOLERANCE
        if not dp_objective_match:
            return None

    snapshot = instance_to_dict(instance)
    try:
        instance_from_dict(snapshot).validate()
    except Exception:
        return None
    checksum = _checksum_data(snapshot)

    instance_id = _instance_id(config, difficulty)
    route_count = len(phase1.feasible_routes)
    metrics = {
        "generation_seconds": generation_seconds,
        "phase1_seconds": phase1_seconds,
        "phase2_ip_seconds": ip_seconds,
        "phase2_dp_seconds": dp_seconds,
        "phase1_route_count": route_count,
        "phase2_ip_status": ip_result.status,
        "phase2_ip_objective": ip_objective,
        "phase2_dp_status": dp_status,
        "phase2_dp_objective": dp_objective,
        "dp_ip_objective_match": dp_objective_match,
    }
    entry = {
        "id": instance_id,
        "path": "",
        "checksum_sha256": checksum,
        "num_customers": config.num_customers,
        "num_arcs": len(instance.arcs),
        "route_count": route_count,
        "difficulty": difficulty,
        "tags": _tags_for(config, difficulty, route_count),
        "generator_config": asdict(config),
        "metrics": metrics,
    }
    return AcceptedInstance(
        instance_id=instance_id,
        instance=instance,
        snapshot=snapshot,
        checksum_sha256=checksum,
        manifest_entry=entry,
    )


def self_check_database(
    *,
    manifest_path: str | Path | None = None,
) -> int:
    """Validate the bundled database and return the number of stored instances."""

    return len(validate_database(manifest_path=manifest_path))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build or validate the bundled MESPPRC instance database.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser("build", help="Generate validated JSON instances.")
    build.add_argument("--output-root", type=Path, default=DATABASE_ROOT)
    build.add_argument("--target-count", type=int, default=72)
    build.add_argument("--min-customers", type=int, default=4)
    build.add_argument("--max-customers", type=int, default=15)
    build.add_argument("--base-seed", type=int, default=12_345)
    build.add_argument("--max-seed-offset", type=int, default=64)
    build.add_argument("--max-route-count", type=int, default=75_000)
    build.add_argument("--no-clean", action="store_true")

    check = subparsers.add_parser("check", help="Validate existing database files.")
    check.add_argument("--manifest-path", type=Path, default=None)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.command == "build":
        entries = build_database(
            output_root=args.output_root,
            target_count=args.target_count,
            min_customers=args.min_customers,
            max_customers=args.max_customers,
            base_seed=args.base_seed,
            max_seed_offset=args.max_seed_offset,
            max_route_count=args.max_route_count,
            clean=not args.no_clean,
        )
        print(f"Wrote {len(entries)} MESPPRC database instances to {args.output_root}")
        return

    count = self_check_database(manifest_path=args.manifest_path)
    print(f"Validated {count} MESPPRC database instances.")


def _config_for(
    n_customers: int,
    profile: GeneratorProfile,
    seed: int,
) -> GeneratorConfig:
    return GeneratorConfig(
        num_customers=n_customers,
        seed=seed,
        beta_route=profile.beta_route,
        rho_capacity=profile.rho_capacity,
        gamma_duty=profile.gamma_duty,
        service_time_slope_per_customer=profile.service_time_slope_per_customer,
    )


def _seed_for(
    base_seed: int,
    n_customers: int,
    profile_name: str,
    seed_offset: int,
) -> int:
    profile_value = sum((idx + 1) * ord(char) for idx, char in enumerate(profile_name))
    return base_seed + 10_000 * n_customers + 101 * profile_value + seed_offset


def _instance_id(config: GeneratorConfig, difficulty: str) -> str:
    seed = 0 if config.seed is None else int(config.seed)
    return f"mespprc_n{config.num_customers:03d}_{difficulty}_s{seed}"


def _tags_for(
    config: GeneratorConfig,
    difficulty: str,
    route_count: int,
) -> list[str]:
    tags = [difficulty, f"n{config.num_customers:03d}"]
    if config.num_customers <= DP_COMPARE_MAX_CUSTOMERS:
        tags.append("dp_checked")
    if route_count < 100:
        tags.append("small_route_pool")
    elif route_count < 5_000:
        tags.append("medium_route_pool")
    else:
        tags.append("large_route_pool")
    return tags


def _target_per_bucket(
    target_count: int,
    profiles: tuple[GeneratorProfile, ...],
) -> int:
    # Keeps the saved suite broad across size/profile buckets before filling all
    # the way to target_count.
    return max(1, target_count // max(1, len(profiles) * 12))


def _checksum_data(data: Mapping[str, Any]) -> str:
    payload = canonical_instance_json(data).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


if __name__ == "__main__":
    main()
