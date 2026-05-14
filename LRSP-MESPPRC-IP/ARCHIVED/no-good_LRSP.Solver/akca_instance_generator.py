from __future__ import annotations

from dataclasses import dataclass, field
from math import hypot
from pathlib import Path
from typing import Dict, Iterable, Iterator, List, Sequence


AKCA_MARKER_CUSTOMER_INDICES: Dict[tuple[str, str, int], tuple[int, ...]] = {
    ("p01", "f", 25): tuple(range(1, 26)),
    ("p01", "l", 25): tuple(range(26, 51)),
    ("p03", "f", 25): tuple(range(1, 26)),
    ("p03", "m", 25): tuple(range(26, 51)),
    ("p03", "l", 25): tuple(range(51, 76)),
    ("p07", "f", 25): tuple(range(1, 26)),
    ("p07", "s", 25): tuple(range(26, 51)),
    ("p07", "t", 25): tuple(range(51, 76)),
    ("p01", "f", 40): tuple(range(1, 41)),
    ("p01", "l", 40): tuple(range(11, 51)),
    ("p03", "f", 40): tuple(range(1, 41)),
    ("p03", "m", 40): tuple(range(16, 36)) + tuple(range(41, 61)),
    ("p03", "l", 40): tuple(range(36, 76)),
    ("p07", "f", 40): tuple(range(1, 41)),
    ("p07", "s", 40): tuple(range(31, 71)),
    ("p07", "t", 40): tuple(range(1, 21)) + tuple(range(51, 71)),
}

AKCA_FACILITY_COORDINATES: Dict[tuple[str, int], tuple[tuple[float, float], ...]] = {
    ("p01", 25): ((20, 20), (30, 40), (15, 40), (45, 40), (55, 50)),
    ("p03", 25): ((40, 40), (50, 22), (25, 45), (20, 20), (55, 55)),
    ("p07", 25): ((15, 35), (55, 35), (35, 20), (35, 50), (25, 45)),
    ("p01", 40): ((20, 20), (30, 40), (15, 40), (50, 30), (55, 55)),
    ("p03", 40): ((40, 40), (50, 22), (25, 45), (20, 20), (55, 55)),
    ("p07", 40): ((15, 35), (55, 35), (35, 20), (35, 50), (25, 45)),
}

AKCA_VEHICLE_CAPACITY: Dict[tuple[str, str], float] = {
    ("p01", "v1"): 50.0,
    ("p01", "v2"): 70.0,
    ("p03", "v1"): 60.0,
    ("p03", "v2"): 80.0,
    ("p07", "v1"): 50.0,
    ("p07", "v2"): 70.0,
}

AKCA_TIME_LIMITS: Dict[str, float] = {
    "t1": 140.0,
    "t2": 160.0,
}

AKCA_FACILITY_CAPACITY = 250.0
AKCA_FACILITY_OPENING_COSTS = (1615.0, 1845.0, 1781.0, 1975.0, 1753.0)
AKCA_VEHICLE_FIXED_COST = 225.0
AKCA_VEHICLE_OPERATING_COST = 1.0


@dataclass(slots=True, frozen=True)
class LRSPCustomer:
    customer_id: int
    x: float
    y: float
    demand: float


@dataclass(slots=True, frozen=True)
class LRSPFacility:
    facility_id: int
    x: float
    y: float
    opening_cost: float
    capacity: float


@dataclass(slots=True, frozen=True)
class AkcaLRSPInstanceSpec:
    """
    Specification for one Akca-style LRSP instance family member.

    Exact from the dissertation:
    - customer subsets by marker/reference/count from Table 5.1
    - facility coordinates from Table 5.2
    - facility capacity, vehicle-capacity variants, and time-limit variants from Table 5.3

    Reconstructed in code:
    - loading customer coordinates/demands from local Cordeau MDVRP reference files
    - packaging these ingredients into a facility-specific pricing workflow
    """

    reference_group: str
    marker: str
    customer_count: int
    vehicle_capacity_variant: str
    time_limit_variant: str

    @property
    def name(self) -> str:
        return (
            f"{self.reference_group}-{self.marker}{self.customer_count}-"
            f"{self.vehicle_capacity_variant}{self.time_limit_variant}"
        )


@dataclass(slots=True)
class AkcaLRSPInstance:
    """
    Full LRSP instance used by the outer branch-and-price layer.

    This object represents the integrated LRSP data:
    - customer locations and demands
    - candidate facility locations and opening costs
    - facility capacities
    - vehicle capacity and vehicle working-time (distance) limit
    - travel-cost parameters used by the pricing engine

    It is intentionally separate from `mespprc_lrsp.MESPPRCInstance`, which is the
    single-facility pricing graph built on demand for one candidate facility at a time.
    """

    name: str
    reference_group: str
    marker: str
    customer_count: int
    vehicle_capacity_variant: str
    time_limit_variant: str
    customers: List[LRSPCustomer]
    facilities: List[LRSPFacility]
    vehicle_capacity: float
    vehicle_time_limit: float
    vehicle_fixed_cost: float
    vehicle_operating_cost: float
    reconstruction_notes: List[str] = field(default_factory=list)

    def customer_ids(self) -> List[int]:
        return [customer.customer_id for customer in self.customers]

    def facility_ids(self) -> List[int]:
        return [facility.facility_id for facility in self.facilities]

    def customer_by_id(self) -> Dict[int, LRSPCustomer]:
        return {customer.customer_id: customer for customer in self.customers}

    def facility_by_id(self) -> Dict[int, LRSPFacility]:
        return {facility.facility_id: facility for facility in self.facilities}

    def total_demand(self) -> float:
        return sum(customer.demand for customer in self.customers)

    def validate_against_akca_family(self) -> None:
        if len(self.facilities) != 5:
            raise ValueError("Akca-style LRSP instances must expose exactly 5 candidate facilities.")
        if self.customer_count not in {25, 40}:
            raise ValueError("Akca-style LRSP instances are defined here only for 25 and 40 customers.")
        if self.time_limit_variant not in AKCA_TIME_LIMITS:
            raise ValueError(f"Unknown Akca time-limit variant: {self.time_limit_variant}.")
        if self.reference_group in {"p01", "p03", "p07"}:
            expected = AKCA_FACILITY_COORDINATES[(self.reference_group, self.customer_count)]
            observed = tuple((facility.x, facility.y) for facility in self.facilities)
            if observed != expected:
                raise ValueError(
                    f"Facility coordinates do not match the Akca reconstruction for {self.reference_group}."
                )


@dataclass(slots=True, frozen=True)
class _CordeauReferenceData:
    reference_group: str
    depot_count: int
    customers: Dict[int, LRSPCustomer]


def _default_mdvrp_data_root() -> Path:
    return (
        Path(__file__).resolve().parents[1]
        / "VRPSolverEasy-master"
        / "VRPSolverEasy"
        / "demos"
        / "data"
        / "MDVRP"
    )


def _default_akca_repo_root() -> Path:
    return (
        Path(__file__).resolve().parents[1]
        / "Akca Repo"
        / "routingproblems-lrspcodenew-a2985b2bf3ec"
    )


def _find_bundled_akca_instance_path(spec: AkcaLRSPInstanceSpec) -> Path | None:
    filename = (
        f"{spec.reference_group}-{spec.marker}{spec.customer_count}-"
        f"{spec.vehicle_capacity_variant}{spec.time_limit_variant}.txt"
    )
    repo_root = _default_akca_repo_root()
    candidate_dirs = (
        repo_root,
        repo_root / "exact-bp",
        repo_root / "exact-1stp-bp",
        repo_root / "heur-bp",
    )
    for candidate_dir in candidate_dirs:
        candidate = candidate_dir / filename
        if candidate.exists():
            return candidate
    return None


def _build_akca_instance_from_bundled_file(spec: AkcaLRSPInstanceSpec) -> AkcaLRSPInstance | None:
    path = _find_bundled_akca_instance_path(spec)
    if path is None:
        return None

    lines = [line.strip() for line in path.read_text().splitlines() if line.strip()]
    header = [int(token) for token in lines[0].split()]
    if len(header) < 2:
        raise ValueError(f"Unexpected Akca instance header in {path}.")

    facility_count = header[0]
    customer_count = header[1]
    if facility_count != 5:
        raise ValueError(f"Expected 5 facilities in {path}, found {facility_count}.")

    opening_costs = [float(token) for token in lines[1].split()]
    if len(opening_costs) != facility_count:
        raise ValueError(f"Unexpected facility-opening-cost row in {path}.")

    vehicle_cost_tokens = [float(token) for token in lines[2].split()]
    if not vehicle_cost_tokens:
        raise ValueError(f"Unexpected vehicle-cost row in {path}.")
    vehicle_fixed_cost = vehicle_cost_tokens[0]

    capacity_tokens = [float(token) for token in lines[3].split()]
    if len(capacity_tokens) < 3:
        raise ValueError(f"Unexpected capacity/time row in {path}.")
    vehicle_capacity = capacity_tokens[0]
    facility_capacity = capacity_tokens[1]
    vehicle_time_limit = capacity_tokens[2]

    customer_lines = lines[4 : 4 + customer_count]
    facility_lines = lines[4 + customer_count : 4 + customer_count + facility_count]

    customers = []
    for raw in customer_lines:
        tokens = raw.split()
        customers.append(
            LRSPCustomer(
                customer_id=int(tokens[0]),
                x=float(tokens[1]),
                y=float(tokens[2]),
                demand=float(tokens[4]),
            )
        )

    parsed_facility_coords = [
        (float(raw.split()[1]), float(raw.split()[2]))
        for raw in facility_lines
    ]
    expected_coords = AKCA_FACILITY_COORDINATES.get((spec.reference_group, spec.customer_count))
    ordered_facility_coords = parsed_facility_coords
    if expected_coords is not None and set(parsed_facility_coords) == set(expected_coords):
        ordered_facility_coords = list(expected_coords)

    facilities = [
        LRSPFacility(
            facility_id=index + 1,
            x=float(x),
            y=float(y),
            opening_cost=opening_costs[index],
            capacity=facility_capacity,
        )
        for index, (x, y) in enumerate(ordered_facility_coords)
    ]

    notes = [
        "Customer coordinates, customer demands, and facility coordinates were parsed "
        "directly from the bundled Akca LRSP instance file.",
        f"Bundled Akca instance source: {path}",
        "Vehicle operating cost is reconstructed as 1.0 to match the dissertation and "
        "the existing Python solver conventions.",
    ]

    return AkcaLRSPInstance(
        name=spec.name,
        reference_group=spec.reference_group,
        marker=spec.marker,
        customer_count=spec.customer_count,
        vehicle_capacity_variant=spec.vehicle_capacity_variant,
        time_limit_variant=spec.time_limit_variant,
        customers=customers,
        facilities=facilities,
        vehicle_capacity=vehicle_capacity,
        vehicle_time_limit=vehicle_time_limit,
        vehicle_fixed_cost=vehicle_fixed_cost,
        vehicle_operating_cost=AKCA_VEHICLE_OPERATING_COST,
        reconstruction_notes=notes,
    )


def load_cordeau_reference_data(
    reference_group: str,
    *,
    data_root: Path | None = None,
) -> _CordeauReferenceData:
    data_root = data_root or _default_mdvrp_data_root()
    path = data_root / reference_group
    lines = [line.strip() for line in path.read_text().splitlines() if line.strip()]
    header = [int(token) for token in lines[0].split()]
    if len(header) < 4:
        raise ValueError(f"Unexpected header in {path}.")

    customer_count = header[2]
    depot_count = header[3]
    depot_header_count = depot_count
    customer_lines = lines[1 + depot_header_count : 1 + depot_header_count + customer_count]

    customers: Dict[int, LRSPCustomer] = {}
    for raw in customer_lines:
        tokens = raw.split()
        customer_id = int(tokens[0])
        x = float(tokens[1])
        y = float(tokens[2])
        demand = float(tokens[4])
        customers[customer_id] = LRSPCustomer(
            customer_id=customer_id,
            x=x,
            y=y,
            demand=demand,
        )

    return _CordeauReferenceData(
        reference_group=reference_group,
        depot_count=depot_count,
        customers=customers,
    )


def build_akca_style_instance(
    reference_group: str,
    marker: str,
    customer_count: int,
    vehicle_capacity_variant: str = "v1",
    time_limit_variant: str = "t1",
    *,
    data_root: Path | None = None,
) -> AkcaLRSPInstance:
    spec = AkcaLRSPInstanceSpec(
        reference_group=reference_group,
        marker=marker,
        customer_count=customer_count,
        vehicle_capacity_variant=vehicle_capacity_variant,
        time_limit_variant=time_limit_variant,
    )
    try:
        reference_data = load_cordeau_reference_data(
            reference_group,
            data_root=data_root,
        )
    except FileNotFoundError:
        bundled_instance = _build_akca_instance_from_bundled_file(spec)
        if bundled_instance is None:
            raise
        bundled_instance.validate_against_akca_family()
        return bundled_instance
    subset_key = (reference_group, marker, customer_count)
    if subset_key not in AKCA_MARKER_CUSTOMER_INDICES:
        raise ValueError(f"Unsupported Akca-style subset: {subset_key}.")
    facility_key = (reference_group, customer_count)
    if facility_key not in AKCA_FACILITY_COORDINATES:
        raise ValueError(f"Unsupported facility coordinate set: {facility_key}.")

    customer_ids = AKCA_MARKER_CUSTOMER_INDICES[subset_key]
    customers = [reference_data.customers[customer_id] for customer_id in customer_ids]
    facilities = [
        LRSPFacility(
            facility_id=index + 1,
            x=float(x),
            y=float(y),
            opening_cost=AKCA_FACILITY_OPENING_COSTS[index],
            capacity=AKCA_FACILITY_CAPACITY,
        )
        for index, (x, y) in enumerate(AKCA_FACILITY_COORDINATES[facility_key])
    ]

    vehicle_capacity = AKCA_VEHICLE_CAPACITY[(reference_group, vehicle_capacity_variant)]
    vehicle_time_limit = AKCA_TIME_LIMITS[time_limit_variant]

    notes = [
        "Customer subsets, facility coordinates, time limits, and capacity variants are "
        "taken from Akca's dissertation Tables 5.1-5.3.",
        "Customer coordinates and demands are loaded from local Cordeau et al. MDVRP "
        "reference files bundled in the repository.",
        "The outer LRSP solver calls mespprc_lrsp as a single-facility pricing engine "
        "once per candidate facility.",
    ]

    return AkcaLRSPInstance(
        name=spec.name,
        reference_group=reference_group,
        marker=marker,
        customer_count=customer_count,
        vehicle_capacity_variant=vehicle_capacity_variant,
        time_limit_variant=time_limit_variant,
        customers=customers,
        facilities=facilities,
        vehicle_capacity=vehicle_capacity,
        vehicle_time_limit=vehicle_time_limit,
        vehicle_fixed_cost=AKCA_VEHICLE_FIXED_COST,
        vehicle_operating_cost=AKCA_VEHICLE_OPERATING_COST,
        reconstruction_notes=notes,
    )


def iter_akca_instance_specs(
    *,
    customer_counts: Sequence[int] = (25, 40),
    vehicle_capacity_variants: Sequence[str] = ("v1", "v2"),
    time_limit_variants: Sequence[str] = ("t1", "t2"),
) -> Iterator[AkcaLRSPInstanceSpec]:
    seen: set[tuple[str, str, int]] = set()
    for reference_group, marker, count in AKCA_MARKER_CUSTOMER_INDICES:
        if count not in customer_counts:
            continue
        key = (reference_group, marker, count)
        if key in seen:
            continue
        seen.add(key)
        for vehicle_capacity_variant in vehicle_capacity_variants:
            for time_limit_variant in time_limit_variants:
                yield AkcaLRSPInstanceSpec(
                    reference_group=reference_group,
                    marker=marker,
                    customer_count=count,
                    vehicle_capacity_variant=vehicle_capacity_variant,
                    time_limit_variant=time_limit_variant,
                )


def generate_25_customer_instances(
    *,
    data_root: Path | None = None,
) -> List[AkcaLRSPInstance]:
    return [
        build_akca_style_instance(
            spec.reference_group,
            spec.marker,
            spec.customer_count,
            spec.vehicle_capacity_variant,
            spec.time_limit_variant,
            data_root=data_root,
        )
        for spec in iter_akca_instance_specs(customer_counts=(25,))
    ]


def generate_40_customer_instances(
    *,
    data_root: Path | None = None,
) -> List[AkcaLRSPInstance]:
    return [
        build_akca_style_instance(
            spec.reference_group,
            spec.marker,
            spec.customer_count,
            spec.vehicle_capacity_variant,
            spec.time_limit_variant,
            data_root=data_root,
        )
        for spec in iter_akca_instance_specs(customer_counts=(40,))
    ]


def validate_akca_style_instance(instance: AkcaLRSPInstance) -> None:
    instance.validate_against_akca_family()


def rounded_euclidean_distance(
    x1: float,
    y1: float,
    x2: float,
    y2: float,
) -> float:
    return float(int(round(hypot(x1 - x2, y1 - y2))))


def max_facility_customer_distance(instance: AkcaLRSPInstance) -> float:
    max_distance = 0.0
    for facility in instance.facilities:
        for customer in instance.customers:
            max_distance = max(
                max_distance,
                rounded_euclidean_distance(facility.x, facility.y, customer.x, customer.y),
            )
    return max_distance
