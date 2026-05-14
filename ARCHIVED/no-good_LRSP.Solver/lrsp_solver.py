from __future__ import annotations

from dataclasses import dataclass

from .akca_instance_generator import AkcaLRSPInstance
from .branch_and_price import BranchAndPriceResult, LRSPBranchAndPriceSkeleton
from .lrsp_column_generation import (
    ColumnGenerationConfig,
    ColumnGenerationResult,
    LRSPColumnGenerationSolver,
)


@dataclass(slots=True)
class PricingComparisonResult:
    dp_result: ColumnGenerationResult
    ip_result: ColumnGenerationResult

    @property
    def objectives_match(self) -> bool:
        if self.dp_result.integer_objective is None or self.ip_result.integer_objective is None:
            return False
        return abs(self.dp_result.integer_objective - self.ip_result.integer_objective) <= 1e-6


class LRSPSolver:
    """
    Full LRSP solver built around an Akca-style master and a pluggable pricing backend.

    The outer shell owns the branch-and-price structure. The inner pricing problem is
    backend-driven, so different MESPPRC variants can be substituted without changing
    the LRSP master or branching code.
    """

    def __init__(
        self,
        *,
        pricing_phase2: str = "dp",
        phase1_label_limit: int | None = None,
        max_iterations: int = 20,
        max_columns_per_facility: int = 1,
        solve_integer_master_after_cg: bool = True,
        use_customer_facility_linking: bool = True,
        use_minimum_open_facilities_bound: bool = True,
        pricing_backend: object | None = None,
    ) -> None:
        self.config = ColumnGenerationConfig(
            pricing_phase2=pricing_phase2,
            phase1_label_limit=phase1_label_limit,
            max_iterations=max_iterations,
            max_columns_per_facility=max_columns_per_facility,
            solve_integer_master_after_cg=solve_integer_master_after_cg,
            use_customer_facility_linking=use_customer_facility_linking,
            use_minimum_open_facilities_bound=use_minimum_open_facilities_bound,
            pricing_backend=pricing_backend,
        )

    def solve_root_node(self, instance: AkcaLRSPInstance) -> ColumnGenerationResult:
        return LRSPColumnGenerationSolver(self.config).solve(instance)

    def solve_branch_and_price(self, instance: AkcaLRSPInstance) -> BranchAndPriceResult:
        return LRSPBranchAndPriceSkeleton(self.config).solve(instance)

    def compare_dp_vs_ip(self, instance: AkcaLRSPInstance) -> PricingComparisonResult:
        if self.config.pricing_backend is not None:
            raise ValueError(
                "compare_dp_vs_ip is only available when the solver is using the default "
                "two-phase pricing backend selection."
            )
        dp_result = LRSPSolver(
            pricing_phase2="dp",
            phase1_label_limit=self.config.phase1_label_limit,
            max_iterations=self.config.max_iterations,
            max_columns_per_facility=self.config.max_columns_per_facility,
            solve_integer_master_after_cg=self.config.solve_integer_master_after_cg,
            use_customer_facility_linking=self.config.use_customer_facility_linking,
            use_minimum_open_facilities_bound=self.config.use_minimum_open_facilities_bound,
        ).solve_root_node(instance)
        ip_result = LRSPSolver(
            pricing_phase2="ip",
            phase1_label_limit=self.config.phase1_label_limit,
            max_iterations=self.config.max_iterations,
            max_columns_per_facility=self.config.max_columns_per_facility,
            solve_integer_master_after_cg=self.config.solve_integer_master_after_cg,
            use_customer_facility_linking=self.config.use_customer_facility_linking,
            use_minimum_open_facilities_bound=self.config.use_minimum_open_facilities_bound,
        ).solve_root_node(instance)
        return PricingComparisonResult(dp_result=dp_result, ip_result=ip_result)
