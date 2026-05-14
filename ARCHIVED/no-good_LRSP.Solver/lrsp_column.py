from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Tuple

from mespprc_lrsp import Route


def _rounded_resource_signature(values: Iterable[float]) -> Tuple[float, ...]:
    return tuple(round(float(value), 8) for value in values)


def _route_signature(route: Route) -> tuple[object, ...]:
    return (
        tuple(route.path),
        tuple(sorted(route.covered_customers)),
        _rounded_resource_signature(route.local_resources),
        _rounded_resource_signature(route.global_resources),
        round(float(route.cost), 8),
        route.first_customer_in_route,
        tuple(route.customer_state_signature),
    )


@dataclass(slots=True, frozen=True)
class MasterDuals:
    coverage_duals: Dict[int, float]
    facility_capacity_duals: Dict[int, float]
    facility_linking_duals: Dict[int, float]
    required_assignment_duals: Dict[tuple[int, int], float]
    forbidden_assignment_duals: Dict[tuple[int, int], float]
    facility_customer_link_duals: Dict[tuple[int, int], float] = field(default_factory=dict)
    minimum_open_facilities_dual: float = 0.0


@dataclass(slots=True)
class LRSPPairingColumn:
    """
    Pairing column used in the restricted master problem.

    A column corresponds to one feasible vehicle pairing for one facility. It is the
    master-level object produced by the facility-specific pricing engine.
    """

    column_id: int
    facility_id: int
    covered_customers: Tuple[int, ...]
    pairing_cost: float
    reduced_cost: float
    total_demand: float
    total_duty_time: float
    total_route_time: float
    route_count: int
    constituent_routes: Tuple[Route, ...]
    pricing_engine: str
    source_iteration: int | None = None
    metadata: Dict[str, float | int | str | bool] = field(default_factory=dict)

    def signature(self) -> tuple[object, ...]:
        canonical_route_signatures = tuple(
            sorted(_route_signature(route) for route in self.constituent_routes)
        )
        return (
            self.facility_id,
            self.covered_customers,
            canonical_route_signatures,
        )

    def is_improving(self, tolerance: float = 1e-6) -> bool:
        return self.reduced_cost < -tolerance

    @property
    def covered_customer_set(self) -> set[int]:
        return set(self.covered_customers)

    @property
    def best_pairing(self) -> List[Route]:
        return list(self.constituent_routes)
