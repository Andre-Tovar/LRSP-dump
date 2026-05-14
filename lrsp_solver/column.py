from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Iterable, Sequence, Tuple


@dataclass(frozen=True, slots=True)
class MasterDuals:
    """
    Dual values returned by the LP relaxation of the restricted master.

    Sign convention is the one PuLP returns directly:
    - `coverage[c]` is the dual of the equality customer-coverage row
    - `facility_capacity[f]` is the dual of the `<=` capacity row (non-positive when binding)
    - `facility_customer_link[(c, f)]` is the dual of the `<=` linking row (non-positive when binding)
    - `min_open_facilities` is the dual of the `>=` lower-bound row (non-negative when binding)

    The pricing module subtracts these duals from arc and column costs in the same way
    a textbook reduced-cost computation would. Because the master rows are written in
    `==`, `<=`, `>=` form rather than collapsed to a single canonical sign, the pricing
    formula uses the duals exactly as they come back from the solver.
    """

    coverage: Dict[int, float]
    facility_capacity: Dict[int, float]
    facility_customer_link: Dict[Tuple[int, int], float] = field(default_factory=dict)
    min_open_facilities: float = 0.0


@dataclass(slots=True)
class Column:
    """
    One column added to the restricted master.

    A column corresponds to one feasible vehicle pairing routed out of a single
    facility. For instances with no duty-time limit the pairing degenerates to a
    single elementary route, but the data model is the same in either case.
    """

    column_id: int
    facility_id: int
    covered_customers: Tuple[int, ...]
    pairing_cost: float
    reduced_cost: float
    total_demand: float
    total_travel_cost: float
    routes: Tuple[Sequence[int], ...]
    pricing_engine: str
    iteration: int | None = None
    metadata: Dict[str, object] = field(default_factory=dict)

    def covered_set(self) -> set[int]:
        return set(self.covered_customers)

    def signature(self) -> Tuple[object, ...]:
        return (
            self.facility_id,
            self.covered_customers,
            tuple(tuple(route) for route in self.routes),
        )


def deduplicate(columns: Iterable[Column]) -> list[Column]:
    """Drop duplicate columns by signature, preserving insertion order."""

    seen: set[Tuple[object, ...]] = set()
    out: list[Column] = []
    for column in columns:
        sig = column.signature()
        if sig in seen:
            continue
        seen.add(sig)
        out.append(column)
    return out
