from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List

from .column import Column
from .master_problem import MasterSolution


@dataclass(slots=True)
class PricingFacilitySummary:
    facility_id: int
    pricing_time: float
    columns_added: int
    best_reduced_cost: float | None
    status: str


@dataclass(slots=True)
class IterationSummary:
    iteration: int
    master_objective: float | None
    master_time: float
    pricing_time: float
    new_column_count: int
    pricing_summaries: List[PricingFacilitySummary]


@dataclass(slots=True)
class ColumnGenerationResult:
    """
    Aggregate result of one full column-generation run on the LRSP root LP.

    Fields are designed so a benchmark runner can produce side-by-side comparison
    tables without touching solver internals.
    """

    pricing_engine: str
    instance_name: str
    status: str
    iterations: List[IterationSummary] = field(default_factory=list)
    root_lp_objective: float | None = None
    integer_objective: float | None = None
    final_master: MasterSolution | None = None
    integer_master: MasterSolution | None = None
    column_pool: List[Column] = field(default_factory=list)
    total_runtime: float = 0.0
    master_runtime: float = 0.0
    pricing_runtime: float = 0.0
    pricing_call_count: int = 0
    reached_optimality: bool = False
    failure_message: str | None = None

    @property
    def iteration_count(self) -> int:
        return len(self.iterations)

    @property
    def total_columns(self) -> int:
        return len(self.column_pool)

    @property
    def avg_pricing_time_per_iteration(self) -> float | None:
        if not self.iterations:
            return None
        return self.pricing_runtime / len(self.iterations)

    @property
    def max_pricing_time_in_iteration(self) -> float | None:
        if not self.iterations:
            return None
        return max(it.pricing_time for it in self.iterations)

    def comparison_row(self) -> Dict[str, object]:
        """One-row dict suitable for comparison tables."""

        return {
            "instance": self.instance_name,
            "pricing": self.pricing_engine,
            "status": self.status,
            "root_lp": self.root_lp_objective,
            "integer": self.integer_objective,
            "iterations": self.iteration_count,
            "columns": self.total_columns,
            "pricing_calls": self.pricing_call_count,
            "master_time": round(self.master_runtime, 4),
            "pricing_time": round(self.pricing_runtime, 4),
            "total_time": round(self.total_runtime, 4),
            "reached_optimality": self.reached_optimality,
        }
