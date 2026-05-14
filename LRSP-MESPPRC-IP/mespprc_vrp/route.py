from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Sequence, Set, Tuple

from .label import Label

OPTIMAL_STATUS = "optimal"
NO_IMPROVING_PAIRING_STATUS = "no_improving_pairing"
NO_NEGATIVE_ROUTES_FROM_PHASE1 = "no_negative_routes_from_phase1"
NO_FEASIBLE_NEGATIVE_PAIRING = "no_feasible_negative_pairing"
GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING = "global_resource_limits_prevent_feasible_pairing"


@dataclass(slots=True)
class Route:
    """
    Shared Phase 2 route object.

    In `mespprc_vrp`, `Route.cost` is interpreted as reduced cost. Phase 2 uses these
    routes as candidates in the pricing pairing problem and preserves the richer Phase 1
    metadata for diagnostics and downstream integration.
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


def pricing_cover_key(route: Route) -> Tuple[int, ...]:
    return tuple(sorted(route.covered_customers))


def pricing_route_reduction_key(route: Route) -> tuple[object, ...]:
    return (
        pricing_cover_key(route),
        route.cost,
        tuple(route.global_resources),
        route.route_id,
    )


def pricing_route_no_worse_global_resources(a: Route, b: Route) -> bool:
    return len(a.global_resources) == len(b.global_resources) and all(
        av <= bv for av, bv in zip(a.global_resources, b.global_resources)
    )


def pricing_route_dominates(a: Route, b: Route) -> bool:
    return (
        pricing_cover_key(a) == pricing_cover_key(b)
        and a.cost <= b.cost
        and pricing_route_no_worse_global_resources(a, b)
        and (
            a.cost < b.cost
            or (
                len(a.global_resources) == len(b.global_resources)
                and any(av < bv for av, bv in zip(a.global_resources, b.global_resources))
            )
        )
    )


def reduce_pricing_route_pool(
    routes: Sequence[Route],
) -> tuple[List[Route], List[RouteReductionRecord]]:
    reductions: List[RouteReductionRecord] = []
    deduplicated: List[Route] = []
    duplicate_map: dict[tuple[Tuple[int, ...], float, Tuple[float, ...]], Route] = {}

    for route in routes:
        duplicate_key = (
            pricing_cover_key(route),
            route.cost,
            tuple(route.global_resources),
        )
        kept_duplicate = duplicate_map.get(duplicate_key)
        if kept_duplicate is not None:
            reductions.append(
                RouteReductionRecord(
                    kept_route_id=kept_duplicate.route_id,
                    removed_route_id=route.route_id,
                    reason="duplicate route under covered-customer set, cost, and global resources",
                )
            )
            continue

        duplicate_map[duplicate_key] = route
        deduplicated.append(route)

    grouped: dict[Tuple[int, ...], List[Route]] = {}
    for route in deduplicated:
        grouped.setdefault(pricing_cover_key(route), []).append(route)

    reduced_routes: List[Route] = []
    for group_routes in grouped.values():
        survivors: List[Route] = []
        for route in sorted(group_routes, key=pricing_route_reduction_key):
            dominated = False
            next_survivors: List[Route] = []
            for survivor in survivors:
                if pricing_route_dominates(survivor, route):
                    reductions.append(
                        RouteReductionRecord(
                            kept_route_id=survivor.route_id,
                            removed_route_id=route.route_id,
                            reason=(
                                "same covered set, no worse reduced cost, and no worse "
                                "global resources"
                            ),
                        )
                    )
                    dominated = True
                    break

                if pricing_route_dominates(route, survivor):
                    reductions.append(
                        RouteReductionRecord(
                            kept_route_id=route.route_id,
                            removed_route_id=survivor.route_id,
                            reason=(
                                "same covered set, no worse reduced cost, and no worse "
                                "global resources"
                            ),
                        )
                    )
                    continue

                next_survivors.append(survivor)

            if not dominated:
                next_survivors.append(route)
                survivors = next_survivors

        reduced_routes.extend(survivors)

    reduced_routes.sort(key=pricing_route_reduction_key)
    return reduced_routes, reductions
