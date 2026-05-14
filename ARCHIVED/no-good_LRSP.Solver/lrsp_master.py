from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Sequence

from pulp import (
    LpBinary,
    LpContinuous,
    LpMinimize,
    LpProblem,
    LpStatus,
    LpVariable,
    PULP_CBC_CMD,
    lpSum,
    value,
)

from .akca_instance_generator import AkcaLRSPInstance
from .branching_rules import NodeConstraints
from .lrsp_column import LRSPPairingColumn, MasterDuals


@dataclass(slots=True, frozen=True)
class MasterModelConfig:
    use_customer_facility_linking: bool = True
    use_minimum_open_facilities_bound: bool = True


@dataclass(slots=True)
class LRSPMasterSolution:
    status: str
    is_feasible: bool
    is_optimal: bool
    objective_value: float | None
    facility_open_values: Dict[int, float]
    column_values: Dict[int, float]
    selected_columns: List[LRSPPairingColumn]
    duals: MasterDuals | None
    customer_facility_assignment_values: Dict[tuple[int, int], float]
    minimum_required_open_facilities: int


class RestrictedMasterProblem:
    """
    Restricted master problem for the integrated LRSP.

    The default formulation follows the stronger Akca-style set-partitioning master:
    - facility-opening variables
    - pairing-selection column variables
    - exact customer coverage
    - facility capacity
    - per-(facility, customer) opening/assignment linking
    - lower bound on the number of open facilities
    """

    def __init__(
        self,
        instance: AkcaLRSPInstance,
        config: MasterModelConfig | None = None,
    ) -> None:
        self.instance = instance
        self.config = config or MasterModelConfig()
        self._columns: List[LRSPPairingColumn] = []
        self._column_signatures: set[tuple[object, ...]] = set()

    @property
    def columns(self) -> List[LRSPPairingColumn]:
        return list(self._columns)

    def add_columns(
        self,
        columns: Sequence[LRSPPairingColumn],
    ) -> List[LRSPPairingColumn]:
        added: List[LRSPPairingColumn] = []
        for column in columns:
            signature = column.signature()
            if signature in self._column_signatures:
                continue
            self._column_signatures.add(signature)
            self._columns.append(column)
            added.append(column)
        self._columns.sort(key=lambda column: column.column_id)
        return added

    def solve(
        self,
        *,
        relax: bool,
        node_constraints: NodeConstraints | None = None,
    ) -> LRSPMasterSolution:
        node_constraints = node_constraints or NodeConstraints.root()
        model = LpProblem(
            "akca_lrsp_restricted_master",
            LpMinimize,
        )

        category = LpContinuous if relax else LpBinary
        y_vars = {
            facility.facility_id: LpVariable(
                f"y_{facility.facility_id}",
                lowBound=0.0,
                upBound=1.0,
                cat=category,
            )
            for facility in self.instance.facilities
        }
        z_vars = {
            column.column_id: LpVariable(
                f"z_{column.column_id}",
                lowBound=0.0,
                upBound=1.0,
                cat=category,
            )
            for column in self._columns
        }

        model += (
            lpSum(
                facility.opening_cost * y_vars[facility.facility_id]
                for facility in self.instance.facilities
            )
            + lpSum(column.pairing_cost * z_vars[column.column_id] for column in self._columns)
        )

        coverage_constraints: Dict[int, object] = {}
        for customer in self.instance.customers:
            coverage_constraints[customer.customer_id] = model.addConstraint(
                lpSum(
                    z_vars[column.column_id]
                    for column in self._columns
                    if customer.customer_id in column.covered_customer_set
                )
                == 1,
                name=f"cover_{customer.customer_id}",
            )

        capacity_constraints: Dict[int, object] = {}
        for facility in self.instance.facilities:
            facility_columns = [
                column for column in self._columns if column.facility_id == facility.facility_id
            ]
            capacity_constraints[facility.facility_id] = model.addConstraint(
                lpSum(
                    column.total_demand * z_vars[column.column_id]
                    for column in facility_columns
                )
                <= facility.capacity * y_vars[facility.facility_id],
                name=f"capacity_{facility.facility_id}",
            )

        facility_customer_link_constraints: Dict[tuple[int, int], object] = {}
        if self.config.use_customer_facility_linking:
            for facility in self.instance.facilities:
                for customer in self.instance.customers:
                    facility_customer_link_constraints[
                        (customer.customer_id, facility.facility_id)
                    ] = model.addConstraint(
                        lpSum(
                            z_vars[column.column_id]
                            for column in self._columns
                            if column.facility_id == facility.facility_id
                            and customer.customer_id in column.covered_customer_set
                        )
                        <= y_vars[facility.facility_id],
                        name=f"facility_customer_link_{customer.customer_id}_{facility.facility_id}",
                    )

        minimum_required_open_facilities = self._minimum_required_open_facilities()
        minimum_open_facilities_constraint: object | None = None
        if self.config.use_minimum_open_facilities_bound:
            minimum_open_facilities_constraint = model.addConstraint(
                lpSum(y_vars.values()) >= minimum_required_open_facilities,
                name="min_open_facilities",
            )

        required_assignment_constraints: Dict[tuple[int, int], object] = {}
        forbidden_assignment_constraints: Dict[tuple[int, int], object] = {}
        for customer_id, facility_id in sorted(
            node_constraints.required_customer_facilities.items()
        ):
            required_assignment_constraints[(customer_id, facility_id)] = model.addConstraint(
                lpSum(
                    z_vars[column.column_id]
                    for column in self._columns
                    if column.facility_id == facility_id
                    and customer_id in column.covered_customer_set
                )
                == 1,
                name=f"assign_req_{customer_id}_{facility_id}",
            )
        for customer_id, forbidden_facilities in sorted(
            node_constraints.forbidden_customer_facilities.items()
        ):
            for facility_id in sorted(forbidden_facilities):
                forbidden_assignment_constraints[(customer_id, facility_id)] = model.addConstraint(
                    lpSum(
                        z_vars[column.column_id]
                        for column in self._columns
                        if column.facility_id == facility_id
                        and customer_id in column.covered_customer_set
                    )
                    == 0,
                    name=f"assign_forbid_{customer_id}_{facility_id}",
                )

        opening_fix_constraints: Dict[int, object] = {}
        for facility_id, fixed_value in sorted(node_constraints.fixed_openings.items()):
            opening_fix_constraints[facility_id] = model.addConstraint(
                y_vars[facility_id] == fixed_value,
                name=f"open_fix_{facility_id}",
            )

        model.solve(PULP_CBC_CMD(msg=False))
        status = LpStatus[model.status]
        is_optimal = status == "Optimal"
        is_feasible = status in {"Optimal", "Not Solved", "Undefined"} or status == "Optimal"
        objective_value = value(model.objective)

        facility_open_values = {
            facility_id: float(value(var) or 0.0)
            for facility_id, var in y_vars.items()
        }
        column_values = {
            column_id: float(value(var) or 0.0)
            for column_id, var in z_vars.items()
        }
        selected_columns = [
            column for column in self._columns if column_values.get(column.column_id, 0.0) > 1e-8
        ]
        customer_facility_assignment_values = {
            (customer.customer_id, facility.facility_id): sum(
                column_values[column.column_id]
                for column in self._columns
                if column.facility_id == facility.facility_id
                and customer.customer_id in column.covered_customer_set
            )
            for customer in self.instance.customers
            for facility in self.instance.facilities
        }

        duals = None
        if relax and is_optimal:
            duals = MasterDuals(
                coverage_duals={
                    customer.customer_id: float(
                        model.constraints[f"cover_{customer.customer_id}"].pi or 0.0
                    )
                    for customer in self.instance.customers
                },
                facility_capacity_duals={
                    facility.facility_id: float(
                        model.constraints[f"capacity_{facility.facility_id}"].pi or 0.0
                    )
                    for facility in self.instance.facilities
                },
                facility_linking_duals={
                    facility.facility_id: 0.0 for facility in self.instance.facilities
                },
                facility_customer_link_duals={
                    (customer.customer_id, facility.facility_id): float(
                        model.constraints[
                            f"facility_customer_link_{customer.customer_id}_{facility.facility_id}"
                        ].pi
                        or 0.0
                    )
                    for facility in self.instance.facilities
                    for customer in self.instance.customers
                    if self.config.use_customer_facility_linking
                },
                minimum_open_facilities_dual=(
                    float(model.constraints["min_open_facilities"].pi or 0.0)
                    if minimum_open_facilities_constraint is not None
                    else 0.0
                ),
                required_assignment_duals={
                    (customer_id, facility_id): float(
                        model.constraints[f"assign_req_{customer_id}_{facility_id}"].pi or 0.0
                    )
                    for customer_id, facility_id in required_assignment_constraints
                },
                forbidden_assignment_duals={
                    (customer_id, facility_id): float(
                        model.constraints[f"assign_forbid_{customer_id}_{facility_id}"].pi
                        or 0.0
                    )
                    for customer_id, facility_id in forbidden_assignment_constraints
                },
            )

        return LRSPMasterSolution(
            status=status,
            is_feasible=is_feasible,
            is_optimal=is_optimal,
            objective_value=float(objective_value) if objective_value is not None else None,
            facility_open_values=facility_open_values,
            column_values=column_values,
            selected_columns=selected_columns,
            duals=duals,
            customer_facility_assignment_values=customer_facility_assignment_values,
            minimum_required_open_facilities=minimum_required_open_facilities,
        )

    def _minimum_required_open_facilities(self) -> int:
        total_demand = self.instance.total_demand()
        if total_demand <= 0:
            return 0

        sorted_capacities = sorted(
            (facility.capacity for facility in self.instance.facilities),
            reverse=True,
        )
        covered = 0.0
        count = 0
        for capacity in sorted_capacities:
            covered += capacity
            count += 1
            if covered + 1e-9 >= total_demand:
                return count
        raise ValueError(
            "LRSP instance total demand exceeds the total capacity of all candidate facilities."
        )
