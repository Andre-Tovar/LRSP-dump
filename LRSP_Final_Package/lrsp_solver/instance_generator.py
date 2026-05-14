"""
LRSP instance generator.

Builds a deterministic, fully-connected LRSP test instance whose three
tightness knobs control the difficulty regime. Mirrors the design of
`mespprc/instance_generator.py` but with the LRSP layer on top: location
(facilities with capacity + opening cost) AND scheduling (vehicle duty-time
limit) baked in.

The generated instance is a true LRSP instance — it always carries a finite
`vehicle_time_limit`. We never produce LRP-style instances; if you want LRP
behaviour, set `vehicle_time_factor` very large so the time limit is
effectively non-binding.

Coordinate model
================

Customers are placed uniformly at random in `[coordinate_min, coordinate_max]^2`.
Facilities are placed uniformly at random in the same square (independent
draws). All travel is straight-line Euclidean and `vehicle_operating_cost`
multiplies the geometric distance to give the actual arc cost.

Tightness knobs
===============

Three independent dials control how hard the problem is:

- `vehicle_capacity_factor` β_v : Q = β_v * max_i demand_i, clamped up to
                                   max_i demand_i. Larger = more customers
                                   fit on a single vehicle trip.
                                   β_v = 1   → only single-customer trips
                                   β_v = 2-3 → typical 2-3 customers per trip
                                   β_v ≥ 5  → capacity is rarely the bottleneck

- `facility_capacity_factor` β_f : Cap_j = β_f * (total_demand /
                                   num_facilities). Smaller = tighter facility
                                   selection (need to open more or be smart
                                   about which to open).
                                   β_f = 1   → tight; the average facility
                                              must operate near full
                                   β_f = 2-3 → comfortable
                                   β_f ≥ 4  → facility capacity barely binds

- `vehicle_time_factor` γ_t      : T = γ_t * longest_singleton_round_trip.
                                   The pairing's duty-time limit. Smaller =
                                   tighter scheduling.
                                   γ_t = 1   → only singletons fit in a duty
                                   γ_t = 2-3 → 2-3 customers per duty
                                   γ_t ≥ 4  → time barely binds

The three default to a moderate setting that produces non-trivial but
solvable instances.

Opening costs and the vehicle fixed cost
========================================

`opening_cost_factor` scales the per-facility opening cost relative to the
mean singleton round-trip cost. Default 5.0 — comparable in magnitude to a
handful of routing decisions.

`vehicle_fixed_cost_factor` scales the per-trip fixed cost the same way.
Default 0.5.

Both are deliberately reported as `*_factor` rather than absolute values so
the generator scales sensibly across instance sizes.

Feasibility validation
======================

Every generated instance is passed through `validate_feasibility(...)` which
asserts:

1. Σ demand ≤ Σ facility_capacity (else the LP is provably infeasible)
2. For every customer i there is some facility j such that
   `2 * d(j, i) * operating_cost ≤ vehicle_time_limit` and
   `demand_i ≤ vehicle_capacity` (else customer i is uncoverable)

If either fails the generator raises `LRSPGeneratorError`. The bundled
default knobs leave plenty of headroom; tightness regimes are calibrated
to stay feasible while exercising the solver's pricing layer.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from math import sqrt
from random import Random
from statistics import fmean
from typing import List, Tuple

from .instance import Customer, Facility, LRSPInstance
from .utils import euclidean_distance

Coordinate = Tuple[float, float]


class LRSPGeneratorError(RuntimeError):
    """Raised when the generator cannot produce a feasible instance."""


@dataclass(frozen=True, slots=True)
class GeneratorConfig:
    """
    Configuration for the LRSP instance generator.

    Tightness knobs:

    - `vehicle_capacity_factor`  β_v : Q = β_v * max_i demand_i
    - `facility_capacity_factor` β_f : Cap_j = β_f * (total demand / F)
    - `vehicle_time_factor`      γ_t : T = γ_t * max_i longest_singleton_round_trip

    Reproducibility:

    - `seed` fully determines customer / facility placement and demand draws.
      Two runs with the same `seed` and the same other parameters produce
      identical instances.
    """

    num_customers: int
    num_facilities: int

    # Coordinate box and placement
    coordinate_min: float = 0.0
    coordinate_max: float = 100.0

    # Customer demand distribution (integer uniform on [demand_min, demand_max])
    demand_min: int = 5
    demand_max: int = 30

    # Vehicle parameters
    vehicle_operating_cost: float = 1.0
    vehicle_capacity_factor: float = 2.5
    vehicle_time_factor: float = 2.5

    # Facility parameters
    facility_capacity_factor: float = 2.0
    opening_cost_factor: float = 5.0
    vehicle_fixed_cost_factor: float = 0.5

    # Naming + reproducibility
    name: str | None = None
    seed: int = 0

    # Numeric tolerance used by feasibility checks
    feasibility_tolerance: float = 1e-9

    def __post_init__(self) -> None:
        if self.num_customers < 1:
            raise ValueError("num_customers must be >= 1")
        if self.num_facilities < 1:
            raise ValueError("num_facilities must be >= 1")
        if self.demand_min < 1 or self.demand_max < self.demand_min:
            raise ValueError("require 1 <= demand_min <= demand_max")
        if self.coordinate_max <= self.coordinate_min:
            raise ValueError("coordinate_max must exceed coordinate_min")
        for name, value in (
            ("vehicle_operating_cost", self.vehicle_operating_cost),
            ("vehicle_capacity_factor", self.vehicle_capacity_factor),
            ("vehicle_time_factor", self.vehicle_time_factor),
            ("facility_capacity_factor", self.facility_capacity_factor),
            ("opening_cost_factor", self.opening_cost_factor),
            ("vehicle_fixed_cost_factor", self.vehicle_fixed_cost_factor),
        ):
            if value <= 0:
                raise ValueError(f"{name} must be > 0 (got {value})")


# ----------------------------------------------------------------------------
# Generator
# ----------------------------------------------------------------------------


def generate_instance(config: GeneratorConfig) -> LRSPInstance:
    """
    Build an LRSPInstance from a GeneratorConfig deterministically.

    Customer ids are 1..N, facility ids are N+1..N+F. The instance is always
    LRSP (carries a finite vehicle_time_limit) — never LRP.
    """

    rng = Random(config.seed)

    customers = _draw_customers(config, rng)
    facilities = _draw_facilities(config, rng, customers)

    total_demand = sum(c.demand for c in customers)
    max_demand = max(c.demand for c in customers)

    # Vehicle capacity — must be at least max(demand) so every customer fits.
    vehicle_capacity = max(
        float(max_demand),
        config.vehicle_capacity_factor * float(max_demand),
    )

    # Facility capacity (uniform across facilities for simplicity; the Akca
    # .txt format we round-trip through carries a single facility_capacity
    # value too). Floor it at 1.5 * max_demand so a singleton fits — and
    # at total_demand / num_facilities so a tight β_f is still feasible.
    base_facility_cap = total_demand / float(config.num_facilities)
    facility_capacity = max(
        1.5 * float(max_demand),
        config.facility_capacity_factor * base_facility_cap,
    )
    facility_capacity = float(round(facility_capacity))

    facilities = [
        Facility(
            id=f.id, x=f.x, y=f.y,
            opening_cost=f.opening_cost,
            capacity=facility_capacity,
        )
        for f in facilities
    ]

    # Per-customer cheapest singleton round-trip cost (over all facilities).
    # vehicle_time_limit is set as a multiple of the WORST such trip — tight
    # γ_t means even small instances might struggle to combine routes; loose
    # γ_t means typical pairings can string together 3+ customers.
    singleton_round_trips = [
        2.0 * config.vehicle_operating_cost * min(
            euclidean_distance(c.x, c.y, f.x, f.y) for f in facilities
        )
        for c in customers
    ]
    longest_singleton_round_trip = max(singleton_round_trips)
    mean_singleton_round_trip = fmean(singleton_round_trips)
    vehicle_time_limit = config.vehicle_time_factor * longest_singleton_round_trip

    # Opening costs: scaled by mean singleton cost so the trade-off between
    # opening a new facility and routing further is meaningful.
    opening_cost = config.opening_cost_factor * mean_singleton_round_trip
    facilities = [
        Facility(
            id=f.id, x=f.x, y=f.y,
            opening_cost=float(round(opening_cost, 6)),
            capacity=f.capacity,
        )
        for f in facilities
    ]

    vehicle_fixed_cost = config.vehicle_fixed_cost_factor * mean_singleton_round_trip
    vehicle_fixed_cost = float(round(vehicle_fixed_cost, 6))

    name = config.name or _default_name(config)

    instance = LRSPInstance(
        name=name,
        customers=list(customers),
        facilities=list(facilities),
        vehicle_capacity=vehicle_capacity,
        vehicle_fixed_cost=vehicle_fixed_cost,
        vehicle_operating_cost=config.vehicle_operating_cost,
        vehicle_time_limit=vehicle_time_limit,
    )
    validate_feasibility(instance, tolerance=config.feasibility_tolerance)
    return instance


# ----------------------------------------------------------------------------
# Feasibility
# ----------------------------------------------------------------------------


def validate_feasibility(instance: LRSPInstance, *, tolerance: float = 1e-9) -> None:
    """
    Reject instances that would force a trivial infeasibility.

    Checks:
      1. Σ demand ≤ Σ facility_capacity
      2. For every customer i, there exists some facility j such that
         a one-customer trip out-and-back from j to i fits inside both
         vehicle_capacity and vehicle_time_limit.
      3. vehicle_time_limit > 0.
    """

    if instance.vehicle_time_limit is None or instance.vehicle_time_limit <= 0.0:
        raise LRSPGeneratorError(
            f"instance '{instance.name}' has non-positive vehicle_time_limit"
        )

    total_demand = sum(c.demand for c in instance.customers)
    total_capacity = sum(f.capacity for f in instance.facilities)
    if total_demand > total_capacity + tolerance:
        raise LRSPGeneratorError(
            f"instance '{instance.name}' total demand {total_demand} exceeds "
            f"total facility capacity {total_capacity}"
        )

    op = instance.vehicle_operating_cost
    tlim = instance.vehicle_time_limit
    cap = instance.vehicle_capacity
    for c in instance.customers:
        if c.demand > cap + tolerance:
            raise LRSPGeneratorError(
                f"customer {c.id} demand {c.demand} exceeds vehicle capacity {cap}"
            )
        # Cheapest singleton round-trip from any facility.
        best = min(
            2.0 * op * euclidean_distance(c.x, c.y, f.x, f.y)
            for f in instance.facilities
        )
        if best > tlim + tolerance:
            raise LRSPGeneratorError(
                f"customer {c.id} cannot be reached: cheapest singleton "
                f"round-trip {best:.3f} exceeds vehicle_time_limit {tlim:.3f}"
            )


# ----------------------------------------------------------------------------
# Internal placement helpers
# ----------------------------------------------------------------------------


def _draw_customers(config: GeneratorConfig, rng: Random) -> List[Customer]:
    out: List[Customer] = []
    for i in range(config.num_customers):
        x = rng.uniform(config.coordinate_min, config.coordinate_max)
        y = rng.uniform(config.coordinate_min, config.coordinate_max)
        d = rng.randint(config.demand_min, config.demand_max)
        out.append(Customer(id=i + 1, x=x, y=y, demand=float(d)))
    return out


def _draw_facilities(
    config: GeneratorConfig, rng: Random, customers: List[Customer]
) -> List[Facility]:
    """Place facilities uniformly at random in the same coordinate box. We
    use a separate stream draw here to avoid coupling customer demand draws
    to facility positions (a demand sequence shouldn't depend on facility
    layout)."""

    out: List[Facility] = []
    F = config.num_facilities
    base_id = config.num_customers + 1
    for j in range(F):
        x = rng.uniform(config.coordinate_min, config.coordinate_max)
        y = rng.uniform(config.coordinate_min, config.coordinate_max)
        # opening_cost and capacity are filled in by `generate_instance` once
        # we know the dependent quantities. Use placeholders here.
        out.append(Facility(id=base_id + j, x=x, y=y, opening_cost=0.0, capacity=0.0))
    return out


def _default_name(config: GeneratorConfig) -> str:
    return (
        f"lrsp_n{config.num_customers:03d}_f{config.num_facilities:02d}"
        f"_bv{config.vehicle_capacity_factor:.1f}"
        f"_bf{config.facility_capacity_factor:.1f}"
        f"_gt{config.vehicle_time_factor:.1f}"
        f"_s{config.seed}"
    )


# ----------------------------------------------------------------------------
# Convenience: difficulty regimes
# ----------------------------------------------------------------------------


REGIMES: dict[str, dict[str, float]] = {
    # All loose — trivial pairings, capacity barely binds, lots of feasible
    # singletons. Useful as a sanity check / lower bound on solver difficulty.
    "easy": dict(
        vehicle_capacity_factor=4.0,
        facility_capacity_factor=3.0,
        vehicle_time_factor=4.0,
    ),
    # Moderate tightness — non-trivial pairings exist, capacity matters,
    # several iterations of column generation are needed.
    "moderate": dict(
        vehicle_capacity_factor=2.5,
        facility_capacity_factor=2.0,
        vehicle_time_factor=2.5,
    ),
    # Tight — most binds, multi-trip pairings are forced. Phase 2 fires
    # often. The hardest of the bundled regimes.
    "tight": dict(
        vehicle_capacity_factor=1.7,
        facility_capacity_factor=1.4,
        vehicle_time_factor=1.7,
    ),
}


def regime_config(
    regime: str,
    *,
    num_customers: int,
    num_facilities: int,
    seed: int = 0,
    **overrides: object,
) -> GeneratorConfig:
    """Build a GeneratorConfig from a named tightness regime."""
    if regime not in REGIMES:
        raise ValueError(
            f"unknown regime {regime!r}; choose from {list(REGIMES.keys())}"
        )
    base = REGIMES[regime]
    fields: dict[str, object] = {
        "num_customers": num_customers,
        "num_facilities": num_facilities,
        "seed": seed,
        **base,
    }
    fields.update(overrides)
    return GeneratorConfig(**fields)  # type: ignore[arg-type]
