from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, List

from .instance import LRSPInstance
from .results import ColumnGenerationResult
from .solver import LRSPSolver, LRSPSolverConfig


@dataclass(slots=True)
class PricingComparison:
    instance_name: str
    dp_result: ColumnGenerationResult
    ip_result: ColumnGenerationResult

    @property
    def root_lp_match(self) -> bool:
        if self.dp_result.root_lp_objective is None or self.ip_result.root_lp_objective is None:
            return False
        return abs(self.dp_result.root_lp_objective - self.ip_result.root_lp_objective) <= 1e-4

    @property
    def integer_match(self) -> bool:
        if self.dp_result.integer_objective is None or self.ip_result.integer_objective is None:
            return False
        return abs(self.dp_result.integer_objective - self.ip_result.integer_objective) <= 1e-4


def compare_pricing_engines(
    instance: LRSPInstance,
    *,
    base_config: LRSPSolverConfig | None = None,
) -> PricingComparison:
    """
    Solve the same LRSP instance once with each built-in pricing engine.

    `base_config` is copied for both runs and only the `pricing` field is overridden,
    so any other config tweaks (label limit, max iterations, etc.) apply to both.
    """

    base = base_config or LRSPSolverConfig()
    dp_config = _replace_pricing(base, "dp")
    ip_config = _replace_pricing(base, "ip")

    dp_result = LRSPSolver(dp_config).solve(instance)
    ip_result = LRSPSolver(ip_config).solve(instance)

    return PricingComparison(
        instance_name=instance.name,
        dp_result=dp_result,
        ip_result=ip_result,
    )


def format_comparison_table(comparisons: Iterable[PricingComparison]) -> str:
    """Render a printable comparison table for one or more `PricingComparison`s."""

    rows: List[dict] = []
    for comp in comparisons:
        rows.append(comp.dp_result.comparison_row())
        rows.append(comp.ip_result.comparison_row())

    if not rows:
        return "(no comparisons to report)"

    headers = [
        "instance",
        "pricing",
        "status",
        "root_lp",
        "integer",
        "iterations",
        "columns",
        "pricing_calls",
        "master_time",
        "pricing_time",
        "total_time",
    ]
    widths = {h: len(h) for h in headers}
    string_rows: list[list[str]] = []
    for row in rows:
        string_row = []
        for h in headers:
            value = row.get(h)
            if isinstance(value, float):
                cell = f"{value:.4f}"
            elif value is None:
                cell = "-"
            else:
                cell = str(value)
            string_row.append(cell)
            widths[h] = max(widths[h], len(cell))
        string_rows.append(string_row)

    def fmt_row(values: list[str]) -> str:
        return " | ".join(value.ljust(widths[h]) for value, h in zip(values, headers))

    header_line = fmt_row(headers)
    separator = "-+-".join("-" * widths[h] for h in headers)
    body_lines = [fmt_row(r) for r in string_rows]
    return "\n".join([header_line, separator, *body_lines])


def _replace_pricing(base: LRSPSolverConfig, pricing: str) -> LRSPSolverConfig:
    return LRSPSolverConfig(
        pricing=pricing,  # type: ignore[arg-type]
        max_iterations=base.max_iterations,
        max_columns_per_facility=base.max_columns_per_facility,
        phase1_label_limit=base.phase1_label_limit,
        improvement_tolerance=base.improvement_tolerance,
        solve_integer_master=base.solve_integer_master,
        use_facility_customer_linking=base.use_facility_customer_linking,
        use_min_open_facilities_bound=base.use_min_open_facilities_bound,
        time_limit_seconds=base.time_limit_seconds,
        cbc_msg=base.cbc_msg,
    )
