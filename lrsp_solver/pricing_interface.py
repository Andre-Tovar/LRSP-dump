from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import List

from .column import Column, MasterDuals
from .instance import Facility, LRSPInstance


@dataclass(slots=True)
class PricingConfig:
    """
    Per-call pricing configuration.

    `improvement_tolerance` is the threshold below which a candidate column is treated
    as improving (i.e. reduced cost < -tolerance).

    `max_columns_per_facility` caps how many improving columns each facility may
    contribute per pricing pass. Setting this above 1 lets the master use cheap
    secondary candidates produced for free by the same labeling pass.

    `phase1_label_limit` is forwarded to the MESPPRC Phase 1 solver. `None` means no
    cap.
    """

    improvement_tolerance: float = 1e-6
    max_columns_per_facility: int = 5
    phase1_label_limit: int | None = None


@dataclass(slots=True)
class PricingProblem:
    """
    A single facility's pricing problem at one column-generation iteration.
    """

    instance: LRSPInstance
    facility: Facility
    duals: MasterDuals
    iteration: int
    next_column_id_start: int
    config: PricingConfig


@dataclass(slots=True)
class PricingResult:
    """
    Output of one pricing call for one facility.

    The returned `columns` list may be empty. `best_reduced_cost` is the most
    negative reduced cost actually observed by the engine, or None if the engine
    did not encounter any candidate column.
    """

    facility_id: int
    columns: List[Column]
    pricing_time: float
    best_reduced_cost: float | None
    status: str
    diagnostics: dict = field(default_factory=dict)

    @property
    def has_improving_column(self) -> bool:
        return len(self.columns) > 0


class PricingSolver(ABC):
    """
    Abstract pricing engine for the LRSP column-generation loop.

    Implementations are responsible for:
    - building a per-facility reduced-cost graph from the master duals,
    - running their preferred MESPPRC oracle,
    - converting any improving routes / pairings into LRSP `Column` objects,
    - reporting timing and a status string.

    The LRSP master has no awareness of which engine is in use, so a clean
    implementation can be swapped in without touching the column-generation loop.
    """

    name: str = "abstract"

    @abstractmethod
    def solve(self, problem: PricingProblem) -> PricingResult:
        """Solve the pricing subproblem for one facility."""
