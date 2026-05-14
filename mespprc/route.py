from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Sequence, Set

from .label import Label

OPTIMAL_STATUS = "optimal"
INFEASIBLE_STATUS = "infeasible"
ROUTE_SET_INFEASIBLE = "generated_routes_cannot_cover_required_customers"
GLOBAL_LIMITS_INFEASIBLE = "global_resource_limits_prevent_full_coverage"


@dataclass(slots=True)
class Route:
    """
    Shared Phase 2 route object.

    Routes come from Phase 1 and remain valid candidates for either the DP or IP
    Phase 2 solver. The structural metadata is preserved even when a solver uses it
    only for diagnostics or safe reduction.
    """

    route_id: int
    cost: float
    local_resources: Sequence[float]
    global_resources: Sequence[float]
    covered_customers: Set[int]
    path: List[int] = field(default_factory=list)
    first_customer_in_route: int | None = None
    customer_state_signature: Sequence[int] = field(default_factory=list)

    def __post_init__(self) -> None:
        self.local_resources = list(self.local_resources)
        self.global_resources = list(self.global_resources)
        self.covered_customers = set(self.covered_customers)
        self.path = list(self.path)
        self.customer_state_signature = list(self.customer_state_signature)

        if self.customer_state_signature:
            Label.validate_unreachable_vector(self.customer_state_signature)

        if self.first_customer_in_route is None:
            self.first_customer_in_route = self._infer_first_customer_from_path()

        if (
            self.first_customer_in_route is not None
            and self.first_customer_in_route not in self.covered_customers
        ):
            raise ValueError(
                "Route first_customer_in_route must be one of the covered customers."
            )

    def _infer_first_customer_from_path(self) -> int | None:
        for node_id in self.path:
            if node_id in self.covered_customers:
                return node_id
        return None

    @classmethod
    def from_route_like(cls, route: object, fallback_id: int) -> "Route":
        required = (
            "route_id",
            "cost",
            "local_resources",
            "global_resources",
            "covered_customers",
        )
        missing = [name for name in required if not hasattr(route, name)]
        if missing:
            missing_str = ", ".join(missing)
            raise TypeError(f"Route object is missing required attributes: {missing_str}")

        route_id = getattr(route, "route_id", fallback_id)
        if route_id is None:
            route_id = fallback_id

        return cls(
            route_id=int(route_id),
            cost=float(getattr(route, "cost")),
            local_resources=list(getattr(route, "local_resources")),
            global_resources=list(getattr(route, "global_resources")),
            covered_customers=set(getattr(route, "covered_customers")),
            path=list(getattr(route, "path", [])),
            first_customer_in_route=getattr(route, "first_customer_in_route", None),
            customer_state_signature=list(
                getattr(route, "customer_state_signature", [])
            ),
        )


@dataclass(frozen=True, slots=True)
class RouteReductionRecord:
    kept_route_id: int
    removed_route_id: int
    reason: str
