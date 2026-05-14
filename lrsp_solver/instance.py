from __future__ import annotations

import importlib.util
from dataclasses import dataclass, field
from math import inf
from pathlib import Path
from typing import Dict, List, Mapping, Sequence

from .utils import euclidean_distance


@dataclass(frozen=True, slots=True)
class Customer:
    id: int
    x: float
    y: float
    demand: float


@dataclass(frozen=True, slots=True)
class Facility:
    id: int
    x: float
    y: float
    opening_cost: float
    capacity: float


@dataclass(slots=True)
class LRSPInstance:
    """
    Integrated LRSP instance.

    The instance is the data the LRSP master operates on. The per-facility pricing
    graph passed to a MESPPRC engine is built on demand from this object.

    `vehicle_time_limit` is optional. When `None`, the pricing engine treats route
    duration as unconstrained, which matches the simple Akca / Cordeau-style
    instances that only specify a vehicle capacity. When finite, it is enforced as
    a per-route local resource limit and as a per-pairing global resource limit so
    multi-route pairings remain feasible.
    """

    name: str
    customers: List[Customer]
    facilities: List[Facility]
    vehicle_capacity: float
    vehicle_fixed_cost: float = 0.0
    vehicle_operating_cost: float = 1.0
    vehicle_time_limit: float | None = None
    notes: List[str] = field(default_factory=list)

    def customer_ids(self) -> List[int]:
        return [c.id for c in self.customers]

    def facility_ids(self) -> List[int]:
        return [f.id for f in self.facilities]

    def customer_by_id(self) -> Dict[int, Customer]:
        return {c.id: c for c in self.customers}

    def facility_by_id(self) -> Dict[int, Facility]:
        return {f.id: f for f in self.facilities}

    def total_demand(self) -> float:
        return sum(c.demand for c in self.customers)

    def travel_cost(self, x1: float, y1: float, x2: float, y2: float) -> float:
        return self.vehicle_operating_cost * euclidean_distance(x1, y1, x2, y2)

    def minimum_required_open_facilities(self) -> int:
        """
        Lower bound on the number of facilities that must be open in any feasible
        solution: greedily pack the largest capacities until total demand is covered.
        """

        total = self.total_demand()
        if total <= 0:
            return 0
        sorted_caps = sorted((f.capacity for f in self.facilities), reverse=True)
        running = 0.0
        for count, cap in enumerate(sorted_caps, start=1):
            running += cap
            if running + 1e-9 >= total:
                return count
        raise ValueError(
            f"Instance '{self.name}' has total demand {total} exceeding total facility "
            "capacity. The instance is infeasible regardless of which facilities open."
        )


def _coerce_id(raw: object) -> int:
    if isinstance(raw, int):
        return raw
    return int(str(raw))


def _coerce_xy(raw: object) -> tuple[float, float]:
    if isinstance(raw, (list, tuple)) and len(raw) == 2:
        return float(raw[0]), float(raw[1])
    raise ValueError(f"Coordinate value {raw!r} is not a 2-tuple.")


def load_lrsp_instance(
    path: str | Path,
    *,
    name: str | None = None,
    vehicle_operating_cost: float = 1.0,
) -> LRSPInstance:
    """
    Load an Akca-style LRSP `.txt` instance.

    The file format used throughout `routingproblems-lrspcode*` follows the Akca
    dissertation convention:

    - line 1: `num_facilities num_customers`
    - line 2: opening costs for each facility
    - line 3: `vehicle_fixed_cost num_vehicles_per_facility`
    - line 4: `vehicle_capacity facility_capacity vehicle_time_limit`
    - next num_customers rows: `id x y service_time demand`
    - next num_facilities rows: `id x y 0 0`

    The `vehicle_time_limit` field on line 4 is the LRSP scheduling component
    (per-vehicle duty / route time limit). Files without that field belong to LRP
    or VRP families and are intentionally rejected by `load_lrsp_instance`.
    """

    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(path)

    with path.open("r", encoding="utf-8") as handle:
        raw_lines = [line.strip() for line in handle if line.strip()]

    if len(raw_lines) < 4:
        raise ValueError(
            f"{path}: file is too short to be an Akca LRSP instance "
            "(needs at least 4 header lines)."
        )

    header_counts = _parse_floats(raw_lines[0])
    if len(header_counts) != 2:
        raise ValueError(
            f"{path}: header line must contain exactly two integers "
            "(num_facilities num_customers)."
        )
    num_facilities = int(header_counts[0])
    num_customers = int(header_counts[1])

    opening_costs = _parse_floats(raw_lines[1])
    if len(opening_costs) != num_facilities:
        raise ValueError(
            f"{path}: expected {num_facilities} opening costs on line 2, "
            f"got {len(opening_costs)}."
        )

    vehicle_header = _parse_floats(raw_lines[2])
    if len(vehicle_header) < 1:
        raise ValueError(
            f"{path}: line 3 must contain at least vehicle_fixed_cost."
        )
    vehicle_fixed_cost = float(vehicle_header[0])

    capacity_header = _parse_floats(raw_lines[3])
    if len(capacity_header) < 3:
        raise ValueError(
            f"{path}: line 4 must contain vehicle_capacity facility_capacity "
            "vehicle_time_limit. This file lacks the LRSP scheduling field."
        )
    vehicle_capacity, facility_capacity, vehicle_time_limit = (
        float(capacity_header[0]),
        float(capacity_header[1]),
        float(capacity_header[2]),
    )

    body_start = 4
    body_end = body_start + num_customers + num_facilities
    if len(raw_lines) < body_end:
        raise ValueError(
            f"{path}: expected {num_customers + num_facilities} body rows after the "
            f"4-line header, found only {len(raw_lines) - body_start}."
        )

    customers: List[Customer] = []
    for line in raw_lines[body_start : body_start + num_customers]:
        values = _parse_floats(line)
        if len(values) < 5:
            raise ValueError(
                f"{path}: customer row '{line}' does not have 5 columns."
            )
        customers.append(
            Customer(
                id=int(values[0]),
                x=float(values[1]),
                y=float(values[2]),
                demand=float(values[4]),
            )
        )

    facilities: List[Facility] = []
    facility_rows = raw_lines[body_start + num_customers : body_end]
    for facility_row, opening_cost in zip(facility_rows, opening_costs):
        values = _parse_floats(facility_row)
        if len(values) < 3:
            raise ValueError(
                f"{path}: facility row '{facility_row}' does not have at least 3 columns."
            )
        facilities.append(
            Facility(
                id=int(values[0]),
                x=float(values[1]),
                y=float(values[2]),
                opening_cost=float(opening_cost),
                capacity=float(facility_capacity),
            )
        )

    return LRSPInstance(
        name=name or path.stem,
        customers=customers,
        facilities=facilities,
        vehicle_capacity=vehicle_capacity,
        vehicle_fixed_cost=vehicle_fixed_cost,
        vehicle_operating_cost=vehicle_operating_cost,
        vehicle_time_limit=vehicle_time_limit,
    )


def _parse_floats(line: str) -> list[float]:
    return [float(token) for token in line.replace("\t", " ").split() if token]


def load_instance_from_module(
    module_path: str | Path,
    *,
    name: str | None = None,
    vehicle_fixed_cost: float = 0.0,
    vehicle_operating_cost: float = 1.0,
    vehicle_time_limit: float | None = None,
) -> LRSPInstance:
    """
    Load a Cordeau / Akca-style LRSP instance from a Python module.

    The expected module variables follow the `routingproblems-lrp_dip_instances_samira`
    convention used throughout the archived repository:
    - `DEMAND`: mapping of customer id to demand
    - `NODES_LOCATION`: mapping of node id to (x, y) for both customers and facilities
    - `VEHICLE_CAPACITY`: scalar
    - `FIXED_COST`: mapping of facility id to opening cost
    - `FACILITY_CAPACITY`: mapping of facility id to capacity

    Customer ids are exactly the keys of `DEMAND`; facility ids are exactly the keys of
    `FIXED_COST`. All other entries in `NODES_LOCATION` must match one of those two
    sets, so a stray key would surface as an error rather than be silently dropped.
    """

    path = Path(module_path)
    if not path.exists():
        raise FileNotFoundError(path)

    module_name = path.stem
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not load instance module at {path}.")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    return load_instance_from_data(
        demand=getattr(module, "DEMAND"),
        nodes_location=getattr(module, "NODES_LOCATION"),
        vehicle_capacity=getattr(module, "VEHICLE_CAPACITY"),
        fixed_cost=getattr(module, "FIXED_COST"),
        facility_capacity=getattr(module, "FACILITY_CAPACITY"),
        name=name or path.stem,
        vehicle_fixed_cost=vehicle_fixed_cost,
        vehicle_operating_cost=vehicle_operating_cost,
        vehicle_time_limit=vehicle_time_limit,
    )


def load_instance_from_data(
    *,
    demand: Mapping[object, float],
    nodes_location: Mapping[object, Sequence[float]],
    vehicle_capacity: float,
    fixed_cost: Mapping[object, float],
    facility_capacity: Mapping[object, float],
    name: str = "lrsp_instance",
    vehicle_fixed_cost: float = 0.0,
    vehicle_operating_cost: float = 1.0,
    vehicle_time_limit: float | None = None,
) -> LRSPInstance:
    customer_ids = sorted(_coerce_id(k) for k in demand)
    facility_ids = sorted(_coerce_id(k) for k in fixed_cost)

    coords: Dict[int, tuple[float, float]] = {}
    for raw_id, raw_xy in nodes_location.items():
        coords[_coerce_id(raw_id)] = _coerce_xy(raw_xy)

    missing = [cid for cid in customer_ids + facility_ids if cid not in coords]
    if missing:
        raise ValueError(f"NODES_LOCATION is missing coordinates for ids: {missing}")

    customers: List[Customer] = []
    for cid in customer_ids:
        x, y = coords[cid]
        customers.append(Customer(id=cid, x=x, y=y, demand=float(demand[_lookup_key(demand, cid)])))

    facilities: List[Facility] = []
    for fid in facility_ids:
        x, y = coords[fid]
        facilities.append(
            Facility(
                id=fid,
                x=x,
                y=y,
                opening_cost=float(fixed_cost[_lookup_key(fixed_cost, fid)]),
                capacity=float(facility_capacity[_lookup_key(facility_capacity, fid)]),
            )
        )

    return LRSPInstance(
        name=name,
        customers=customers,
        facilities=facilities,
        vehicle_capacity=float(vehicle_capacity),
        vehicle_fixed_cost=float(vehicle_fixed_cost),
        vehicle_operating_cost=float(vehicle_operating_cost),
        vehicle_time_limit=None if vehicle_time_limit is None else float(vehicle_time_limit),
    )


def _lookup_key(mapping: Mapping[object, object], target_id: int) -> object:
    """Tolerate both int and string keys in the original instance modules."""
    if target_id in mapping:
        return target_id
    str_key = str(target_id)
    if str_key in mapping:
        return str_key
    raise KeyError(target_id)


def synthetic_instance(
    *,
    name: str = "synthetic",
    customer_count: int = 5,
    facility_count: int = 2,
    seed: int | None = 0,
    vehicle_capacity: float = 100.0,
    vehicle_fixed_cost: float = 0.0,
    vehicle_operating_cost: float = 1.0,
) -> LRSPInstance:
    """
    Build a small synthetic LRSP instance for tests and quick checks.

    Customers are placed on a circle, facilities on a smaller inner circle.
    """

    from math import cos, pi, sin
    from random import Random

    rng = Random(seed)

    customers: List[Customer] = []
    for i in range(customer_count):
        angle = 2.0 * pi * i / customer_count
        customers.append(
            Customer(
                id=i + 1,
                x=50.0 + 40.0 * cos(angle),
                y=50.0 + 40.0 * sin(angle),
                demand=float(rng.randint(5, 30)),
            )
        )

    facilities: List[Facility] = []
    for j in range(facility_count):
        angle = 2.0 * pi * j / facility_count
        facilities.append(
            Facility(
                id=customer_count + j + 1,
                x=50.0 + 10.0 * cos(angle),
                y=50.0 + 10.0 * sin(angle),
                opening_cost=20.0,
                capacity=vehicle_capacity * 4.0,
            )
        )

    return LRSPInstance(
        name=name,
        customers=customers,
        facilities=facilities,
        vehicle_capacity=vehicle_capacity,
        vehicle_fixed_cost=vehicle_fixed_cost,
        vehicle_operating_cost=vehicle_operating_cost,
    )
