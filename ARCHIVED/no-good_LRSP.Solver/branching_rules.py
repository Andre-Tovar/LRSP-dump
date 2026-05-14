from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Set


@dataclass(slots=True, frozen=True)
class CustomerFacilityAssignment:
    customer_id: int
    facility_id: int


@dataclass(slots=True)
class NodeConstraints:
    """
    Branch-and-price node constraints for the LRSP.

    The primary branching object is customer-to-facility assignment, which is natural
    for Akca's integrated location-routing-scheduling structure because the master
    already couples customer coverage, facility opening, and pairing selection.
    """

    fixed_openings: Dict[int, int] = field(default_factory=dict)
    required_customer_facilities: Dict[int, int] = field(default_factory=dict)
    forbidden_customer_facilities: Dict[int, Set[int]] = field(default_factory=dict)

    def clone(self) -> "NodeConstraints":
        return NodeConstraints(
            fixed_openings=dict(self.fixed_openings),
            required_customer_facilities=dict(self.required_customer_facilities),
            forbidden_customer_facilities={
                customer_id: set(facility_ids)
                for customer_id, facility_ids in self.forbidden_customer_facilities.items()
            },
        )

    def fix_facility(self, facility_id: int, value: int) -> None:
        self.fixed_openings[facility_id] = int(value)

    def require_customer_at_facility(self, customer_id: int, facility_id: int) -> None:
        fixed_value = self.fixed_openings.get(facility_id)
        if fixed_value == 0:
            raise ValueError(
                f"Facility {facility_id} is already fixed closed and cannot receive customer {customer_id}."
            )
        self.required_customer_facilities[customer_id] = facility_id
        forbidden = self.forbidden_customer_facilities.setdefault(customer_id, set())
        forbidden.clear()
        self.fixed_openings[facility_id] = 1

    def forbid_customer_at_facility(self, customer_id: int, facility_id: int) -> None:
        required = self.required_customer_facilities.get(customer_id)
        if required == facility_id:
            raise ValueError(
                f"Customer {customer_id} is already required at facility {facility_id}."
            )
        self.forbidden_customer_facilities.setdefault(customer_id, set()).add(facility_id)

    def is_customer_allowed_at_facility(self, customer_id: int, facility_id: int) -> bool:
        required = self.required_customer_facilities.get(customer_id)
        if required is not None:
            return required == facility_id
        return facility_id not in self.forbidden_customer_facilities.get(customer_id, set())

    def forbidden_facilities_for_customer(self, customer_id: int) -> Set[int]:
        return set(self.forbidden_customer_facilities.get(customer_id, set()))

    def active_assignments(self) -> List[CustomerFacilityAssignment]:
        return [
            CustomerFacilityAssignment(customer_id, facility_id)
            for customer_id, facility_id in sorted(self.required_customer_facilities.items())
        ]

    @classmethod
    def root(cls) -> "NodeConstraints":
        return cls()
