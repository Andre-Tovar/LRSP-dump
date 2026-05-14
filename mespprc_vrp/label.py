from __future__ import annotations

from dataclasses import dataclass, field
from numbers import Integral
from typing import Iterable, List

REACHABLE = 0
TEMP_UNREACHABLE = -2
PERM_UNREACHABLE = -1


@dataclass(slots=True)
class Label:
    """
    Generic dynamic-programming label shared by both phases.

    The `resources` vector is solver-specific:
    - Phase 1: local / per-route resources
    - Phase 2: global / route-combination resources

    The `unreachable_vector` follows the richer MESPPRC semantics:
    - `value > 0`: the customer has already been visited, and the value is its visit order
    - `0`: the customer is currently reachable and not yet visited
    - `-2`: the customer is temporarily unreachable from the current partial state
    - `-1`: the customer is permanently unreachable from the current partial state

    `unreachable_count` is the count of permanently lost customers. It includes visited
    customers (`value > 0`) and permanently unreachable customers (`value == -1`), and
    it explicitly excludes temporarily unreachable customers (`value == -2`).
    """

    current_node: int
    cost: float
    resources: List[float] = field(default_factory=list)
    unreachable_vector: List[int] = field(default_factory=list)
    unreachable_count: int | None = None

    def __post_init__(self) -> None:
        self.resources = list(self.resources)
        self.unreachable_vector = [
            self.normalize_customer_state(value) for value in self.unreachable_vector
        ]

        computed_count = self.compute_unreachable_count(self.unreachable_vector)
        if self.unreachable_count is None:
            self.unreachable_count = computed_count
        elif self.unreachable_count != computed_count:
            raise ValueError(
                "Label unreachable_count does not match the unreachable_vector semantics."
            )

    def copy(self, **changes: object) -> "Label":
        data = {
            "current_node": self.current_node,
            "cost": self.cost,
            "resources": list(self.resources),
            "unreachable_vector": list(self.unreachable_vector),
            "unreachable_count": self.unreachable_count,
        }
        data.update(changes)
        return Label(**data)

    @staticmethod
    def normalize_customer_state(value: int) -> int:
        if not isinstance(value, Integral):
            raise ValueError(f"Customer state must be an integer, got {value!r}.")

        normalized = int(value)
        if normalized > 0 or normalized in (
            REACHABLE,
            TEMP_UNREACHABLE,
            PERM_UNREACHABLE,
        ):
            return normalized

        raise ValueError(
            "Invalid customer state value. Expected a positive visit order, "
            "0, -2, or -1."
        )

    @classmethod
    def validate_unreachable_vector(cls, vector: Iterable[int]) -> None:
        for value in vector:
            cls.normalize_customer_state(value)

    @staticmethod
    def is_visited(value: int) -> bool:
        return value > 0

    @staticmethod
    def is_reachable(value: int) -> bool:
        return value == REACHABLE

    @staticmethod
    def is_temp_unreachable(value: int) -> bool:
        return value == TEMP_UNREACHABLE

    @staticmethod
    def is_perm_unreachable(value: int) -> bool:
        return value == PERM_UNREACHABLE

    @classmethod
    def is_permanently_lost(cls, value: int) -> bool:
        return cls.is_visited(value) or cls.is_perm_unreachable(value)

    @classmethod
    def compute_permanent_unreachable_count(cls, vector: Iterable[int]) -> int:
        cls.validate_unreachable_vector(vector)
        return sum(1 for value in vector if cls.is_permanently_lost(value))

    @classmethod
    def compute_unreachable_count(cls, vector: Iterable[int]) -> int:
        """
        Backward-compatible alias for the permanent-unreachable count.
        """

        return cls.compute_permanent_unreachable_count(vector)

    @classmethod
    def structural_state_rank(cls, value: int) -> int:
        normalized = cls.normalize_customer_state(value)
        if cls.is_reachable(normalized):
            return 0
        if cls.is_temp_unreachable(normalized):
            return 1
        return 2
