from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, List, Sequence

from .akca_instance_generator import AkcaLRSPInstance, build_akca_style_instance, iter_akca_instance_specs
from .lrsp_solver import LRSPSolver


@dataclass(slots=True)
class LRSPBenchmarkRecord:
    instance_name: str
    pricing_phase2: str
    root_lp_objective: float | None
    integer_objective: float | None
    master_iterations: int
    pricing_call_count: int
    total_columns: int
    phase1_time_total: float
    phase2_time_total: float
    total_column_generation_time: float


def benchmark_dp_vs_ip(
    instances: Sequence[AkcaLRSPInstance],
) -> List[LRSPBenchmarkRecord]:
    records: List[LRSPBenchmarkRecord] = []
    for instance in instances:
        for pricing_phase2 in ("dp", "ip"):
            result = LRSPSolver(pricing_phase2=pricing_phase2).solve_root_node(instance)
            records.append(
                LRSPBenchmarkRecord(
                    instance_name=instance.name,
                    pricing_phase2=pricing_phase2,
                    root_lp_objective=result.root_lp_objective,
                    integer_objective=result.integer_objective,
                    master_iterations=result.master_iterations,
                    pricing_call_count=result.pricing_call_count,
                    total_columns=result.total_columns,
                    phase1_time_total=result.phase1_time_total,
                    phase2_time_total=result.phase2_time_total,
                    total_column_generation_time=result.total_column_generation_time,
                )
            )
    return records


def benchmark_default_akca_instances(
    *,
    customer_counts: Sequence[int] = (25,),
    limit: int | None = None,
) -> List[LRSPBenchmarkRecord]:
    specs = list(iter_akca_instance_specs(customer_counts=customer_counts))
    if limit is not None:
        specs = specs[:limit]
    instances = [
        build_akca_style_instance(
            spec.reference_group,
            spec.marker,
            spec.customer_count,
            spec.vehicle_capacity_variant,
            spec.time_limit_variant,
        )
        for spec in specs
    ]
    return benchmark_dp_vs_ip(instances)
