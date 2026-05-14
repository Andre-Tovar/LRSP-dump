from __future__ import annotations

from dataclasses import dataclass, field
from math import inf
from typing import Dict, List, Tuple

from .akca_instance_generator import AkcaLRSPInstance
from .branching_rules import NodeConstraints
from .lrsp_column_generation import (
    ColumnGenerationConfig,
    ColumnGenerationResult,
    LRSPColumnGenerationSolver,
)


@dataclass(slots=True)
class BranchDecision:
    description: str
    facility_id: int | None = None
    customer_id: int | None = None
    value: int | None = None


@dataclass(slots=True)
class BranchNode:
    node_id: int
    depth: int
    constraints: NodeConstraints = field(default_factory=NodeConstraints.root)
    decisions: List[BranchDecision] = field(default_factory=list)
    parent_node_id: int | None = None


@dataclass(slots=True)
class BranchAndPriceResult:
    status: str
    best_objective: float | None
    best_node_id: int | None
    processed_node_count: int
    explored_nodes: List[BranchNode]
    incumbent_result: ColumnGenerationResult | None
    root_result: ColumnGenerationResult | None


class LRSPBranchAndPriceSkeleton:
    """
    Problem-aware branch-and-price driver for the LRSP outer layer.

    The branching strategy follows the integrated LRSP structure from Akca's work:
    branch first on customer-to-facility assignment, and if the assignment is already
    integral, branch on fractional facility-opening values.
    """

    def __init__(
        self,
        config: ColumnGenerationConfig | None = None,
        *,
        integrality_tolerance: float = 1e-6,
        max_nodes: int = 25,
    ) -> None:
        self.config = config or ColumnGenerationConfig()
        self.integrality_tolerance = integrality_tolerance
        self.max_nodes = max_nodes

    def solve_root(self, instance: AkcaLRSPInstance) -> ColumnGenerationResult:
        return LRSPColumnGenerationSolver(self.config).solve_with_constraints(
            instance,
            node_constraints=NodeConstraints.root(),
        )

    def solve(self, instance: AkcaLRSPInstance) -> BranchAndPriceResult:
        root = BranchNode(node_id=0, depth=0)
        pending: List[BranchNode] = [root]
        explored: List[BranchNode] = []
        processed_node_count = 0
        incumbent_result: ColumnGenerationResult | None = None
        incumbent_objective = inf
        best_node_id: int | None = None
        root_result: ColumnGenerationResult | None = None

        while pending and processed_node_count < self.max_nodes:
            node = min(pending, key=lambda candidate: candidate.depth)
            pending.remove(node)

            result = LRSPColumnGenerationSolver(self.config).solve_with_constraints(
                instance,
                node_constraints=node.constraints,
            )
            if root_result is None:
                root_result = result
            explored.append(node)
            processed_node_count += 1

            integer_objective = result.integer_objective
            if integer_objective is not None and integer_objective < incumbent_objective:
                incumbent_result = result
                incumbent_objective = integer_objective
                best_node_id = node.node_id

            root_solution = result.root_master_solution
            if root_solution is None or not root_solution.is_optimal:
                continue

            node_lower_bound = result.root_lp_objective
            if (
                node_lower_bound is not None
                and incumbent_result is not None
                and node_lower_bound >= incumbent_objective - self.integrality_tolerance
            ):
                continue

            if self._is_integral(root_solution):
                continue

            branch_choice = self._choose_branch(root_solution)
            if branch_choice is None:
                continue

            branch_kind, subject_id, value = branch_choice
            if branch_kind == "assignment":
                customer_id, facility_id = subject_id
                left_constraints = node.constraints.clone()
                left_constraints.require_customer_at_facility(customer_id, facility_id)

                right_constraints = node.constraints.clone()
                right_constraints.forbid_customer_at_facility(customer_id, facility_id)

                pending.append(
                    BranchNode(
                        node_id=node.node_id * 2 + 1,
                        depth=node.depth + 1,
                        constraints=left_constraints,
                        decisions=list(node.decisions)
                        + [
                            BranchDecision(
                                description="require customer at facility",
                                customer_id=customer_id,
                                facility_id=facility_id,
                                value=1,
                            )
                        ],
                        parent_node_id=node.node_id,
                    )
                )
                pending.append(
                    BranchNode(
                        node_id=node.node_id * 2 + 2,
                        depth=node.depth + 1,
                        constraints=right_constraints,
                        decisions=list(node.decisions)
                        + [
                            BranchDecision(
                                description="forbid customer at facility",
                                customer_id=customer_id,
                                facility_id=facility_id,
                                value=0,
                            )
                        ],
                        parent_node_id=node.node_id,
                    )
                )
            elif branch_kind == "facility":
                facility_id = int(subject_id)
                del value
                left_constraints = node.constraints.clone()
                left_constraints.fix_facility(facility_id, 1)

                right_constraints = node.constraints.clone()
                right_constraints.fix_facility(facility_id, 0)

                pending.append(
                    BranchNode(
                        node_id=node.node_id * 2 + 1,
                        depth=node.depth + 1,
                        constraints=left_constraints,
                        decisions=list(node.decisions)
                        + [
                            BranchDecision(
                                description="force facility open",
                                facility_id=facility_id,
                                value=1,
                            )
                        ],
                        parent_node_id=node.node_id,
                    )
                )
                pending.append(
                    BranchNode(
                        node_id=node.node_id * 2 + 2,
                        depth=node.depth + 1,
                        constraints=right_constraints,
                        decisions=list(node.decisions)
                        + [
                            BranchDecision(
                                description="force facility closed",
                                facility_id=facility_id,
                                value=0,
                            )
                        ],
                        parent_node_id=node.node_id,
                    )
                )

        if pending:
            status = "node_limit"
        elif incumbent_result is not None:
            status = "optimal"
        else:
            status = "no_integer_solution"

        return BranchAndPriceResult(
            status=status,
            best_objective=(
                incumbent_result.integer_objective if incumbent_result is not None else None
            ),
            best_node_id=best_node_id,
            processed_node_count=processed_node_count,
            explored_nodes=explored,
            incumbent_result=incumbent_result,
            root_result=root_result,
        )

    def _is_integral(self, solution) -> bool:
        return all(
            abs(value - round(value)) <= self.integrality_tolerance
            for value in list(solution.facility_open_values.values())
            + list(solution.column_values.values())
        )

    def _choose_branch(
        self,
        solution,
    ) -> Tuple[str, object, float] | None:
        assignment_choice = self._choose_branch_assignment(solution)
        if assignment_choice is not None:
            customer_id, facility_id, value = assignment_choice
            return ("assignment", (customer_id, facility_id), value)

        facility_choice = self._choose_branch_facility(solution)
        if facility_choice is not None:
            facility_id, value = facility_choice
            return ("facility", facility_id, value)
        return None

    def _choose_branch_assignment(
        self,
        solution,
    ) -> Tuple[int, int, float] | None:
        fractional_assignments = [
            (customer_id, facility_id, value)
            for (customer_id, facility_id), value in solution.customer_facility_assignment_values.items()
            if self.integrality_tolerance < value < 1.0 - self.integrality_tolerance
        ]
        if not fractional_assignments:
            return None
        return min(
            fractional_assignments,
            key=lambda item: abs(item[2] - 0.5),
        )

    def _choose_branch_facility(
        self,
        solution,
    ) -> Tuple[int, float] | None:
        fractional_openings = [
            (facility_id, value)
            for facility_id, value in solution.facility_open_values.items()
            if self.integrality_tolerance < value < 1.0 - self.integrality_tolerance
        ]
        if not fractional_openings:
            return None
        return min(
            fractional_openings,
            key=lambda item: abs(item[1] - 0.5),
        )
