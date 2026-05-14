from __future__ import annotations

from dataclasses import dataclass, field
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

from .column import Column, MasterDuals
from .instance import LRSPInstance


@dataclass(slots=True, frozen=True)
class MasterConfig:
    """
    Switches that control which Akca-style master rows are present.

    The default keeps the full structured master from the dissertation. The flags
    are exposed because some research questions only need the simpler model and
    because they make the formulation easy to reproduce on a smaller instance.
    """

    use_facility_customer_linking: bool = True
    use_min_open_facilities_bound: bool = True
    cbc_msg: bool = False


@dataclass(slots=True)
class MasterSolution:
    status: str
    is_optimal: bool
    objective: float | None
    facility_open_values: Dict[int, float]
    column_values: Dict[int, float]
    selected_columns: List[Column]
    duals: MasterDuals | None
    minimum_required_open_facilities: int
    integer: bool
    column_count: int


class RestrictedMasterProblem:
    """
    LRSP restricted master problem.

    Variables:
    - `y_j` for each facility (continuous in LP, binary in IP)
    - `λ_p` for each generated pairing column (continuous in LP, binary in IP)

    Rows (Akca-style set-partitioning master):
    - customer coverage `Σ_p a_ip λ_p == 1`
    - facility capacity `Σ_p d_p λ_p − Cap_j y_j <= 0`
    - per-(customer, facility) linking `Σ_p a_ip λ_p − y_j <= 0`
    - lower bound on the number of open facilities `Σ_j y_j >= K`
    """

    def __init__(self, instance: LRSPInstance, config: MasterConfig | None = None) -> None:
        self.instance = instance
        self.config = config or MasterConfig()
        self._columns: List[Column] = []
        self._signatures: set[tuple[object, ...]] = set()
        self._next_column_id = 1

    @property
    def columns(self) -> List[Column]:
        return list(self._columns)

    @property
    def column_count(self) -> int:
        return len(self._columns)

    def next_column_id(self) -> int:
        cid = self._next_column_id
        self._next_column_id += 1
        return cid

    def add_columns(self, columns: Sequence[Column]) -> List[Column]:
        added: List[Column] = []
        for column in columns:
            sig = column.signature()
            if sig in self._signatures:
                continue
            if column.column_id is None:
                column.column_id = self.next_column_id()
            else:
                self._next_column_id = max(self._next_column_id, column.column_id + 1)
            self._signatures.add(sig)
            self._columns.append(column)
            added.append(column)
        return added

    def solve(self, *, relax: bool) -> MasterSolution:
        category = LpContinuous if relax else LpBinary
        model = LpProblem("lrsp_restricted_master", LpMinimize)

        y_vars = {
            facility.id: LpVariable(
                f"y_{facility.id}", lowBound=0.0, upBound=1.0, cat=category
            )
            for facility in self.instance.facilities
        }
        z_vars = {
            column.column_id: LpVariable(
                f"z_{column.column_id}", lowBound=0.0, upBound=1.0, cat=category
            )
            for column in self._columns
        }

        model += (
            lpSum(f.opening_cost * y_vars[f.id] for f in self.instance.facilities)
            + lpSum(c.pairing_cost * z_vars[c.column_id] for c in self._columns)
        )

        coverage_names: Dict[int, str] = {}
        for customer in self.instance.customers:
            name = f"cover_{customer.id}"
            coverage_names[customer.id] = name
            model.addConstraint(
                lpSum(
                    z_vars[c.column_id]
                    for c in self._columns
                    if customer.id in c.covered_customers
                )
                == 1,
                name=name,
            )

        capacity_names: Dict[int, str] = {}
        for facility in self.instance.facilities:
            name = f"capacity_{facility.id}"
            capacity_names[facility.id] = name
            facility_columns = [
                c for c in self._columns if c.facility_id == facility.id
            ]
            model.addConstraint(
                lpSum(c.total_demand * z_vars[c.column_id] for c in facility_columns)
                - facility.capacity * y_vars[facility.id]
                <= 0,
                name=name,
            )

        link_names: Dict[tuple[int, int], str] = {}
        if self.config.use_facility_customer_linking:
            for facility in self.instance.facilities:
                for customer in self.instance.customers:
                    name = f"link_{customer.id}_{facility.id}"
                    link_names[(customer.id, facility.id)] = name
                    model.addConstraint(
                        lpSum(
                            z_vars[c.column_id]
                            for c in self._columns
                            if c.facility_id == facility.id
                            and customer.id in c.covered_customers
                        )
                        - y_vars[facility.id]
                        <= 0,
                        name=name,
                    )

        min_open = self.instance.minimum_required_open_facilities()
        min_open_name: str | None = None
        if self.config.use_min_open_facilities_bound and min_open > 0:
            min_open_name = "min_open_facilities"
            model.addConstraint(
                lpSum(y_vars.values()) >= min_open,
                name=min_open_name,
            )

        model.solve(PULP_CBC_CMD(msg=self.config.cbc_msg))
        status = LpStatus[model.status]
        is_optimal = status == "Optimal"
        objective = value(model.objective)

        facility_open_values = {
            f.id: float(value(y_vars[f.id]) or 0.0)
            for f in self.instance.facilities
        }
        column_values = {
            c.column_id: float(value(z_vars[c.column_id]) or 0.0)
            for c in self._columns
        }
        selected_columns = [
            c for c in self._columns if column_values.get(c.column_id, 0.0) > 1e-6
        ]

        duals: MasterDuals | None = None
        if relax and is_optimal:
            duals = MasterDuals(
                coverage={
                    customer.id: _dual(model, coverage_names[customer.id])
                    for customer in self.instance.customers
                },
                facility_capacity={
                    facility.id: _dual(model, capacity_names[facility.id])
                    for facility in self.instance.facilities
                },
                facility_customer_link={
                    key: _dual(model, link_names[key]) for key in link_names
                },
                min_open_facilities=(
                    _dual(model, min_open_name) if min_open_name is not None else 0.0
                ),
            )

        return MasterSolution(
            status=status,
            is_optimal=is_optimal,
            objective=float(objective) if objective is not None else None,
            facility_open_values=facility_open_values,
            column_values=column_values,
            selected_columns=selected_columns,
            duals=duals,
            minimum_required_open_facilities=min_open,
            integer=not relax,
            column_count=len(self._columns),
        )


def _dual(model: LpProblem, name: str) -> float:
    """Return the dual value of a constraint by name, defaulting to 0.0."""

    constraint = model.constraints.get(name)
    if constraint is None:
        return 0.0
    pi = constraint.pi
    if pi is None:
        return 0.0
    return float(pi)
