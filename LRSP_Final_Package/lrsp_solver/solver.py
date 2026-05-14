from __future__ import annotations

from dataclasses import dataclass
from typing import Literal

from .column_generation import ColumnGenerationConfig, ColumnGenerationSolver
from .instance import LRSPInstance
from .master_problem import MasterConfig
from .pricing_dp import MESPPRCDPPricingSolver
from .pricing_interface import PricingConfig, PricingSolver
from .pricing_ip import MESPPRCIPPricingSolver
from .results import ColumnGenerationResult


PricingChoice = Literal["ip", "dp"]


@dataclass(slots=True)
class LRSPSolverConfig:
    pricing: PricingChoice = "dp"
    max_iterations: int = 50
    max_columns_per_facility: int = 5
    phase1_label_limit: int | None = None
    improvement_tolerance: float = 1e-6
    solve_integer_master: bool = True
    use_facility_customer_linking: bool = True
    use_min_open_facilities_bound: bool = True
    time_limit_seconds: float | None = None
    cbc_msg: bool = False


class LRSPSolver:
    """
    Top-level LRSP solver.

    The pricing engine is selected by `LRSPSolverConfig.pricing`:
    - "dp" -> `MESPPRCDPPricingSolver`
    - "ip" -> `MESPPRCIPPricingSolver`

    Custom pricing engines can be passed directly via `pricing_solver` to bypass
    the built-in selector. This is what users should reach for if they want to
    benchmark a third MESPPRC variant.
    """

    def __init__(
        self,
        config: LRSPSolverConfig | None = None,
        *,
        pricing_solver: PricingSolver | None = None,
    ) -> None:
        self.config = config or LRSPSolverConfig()
        self.pricing_solver = pricing_solver or _build_pricing_solver(self.config.pricing)

    def solve(self, instance: LRSPInstance) -> ColumnGenerationResult:
        cg_config = ColumnGenerationConfig(
            max_iterations=self.config.max_iterations,
            improvement_tolerance=self.config.improvement_tolerance,
            solve_integer_master=self.config.solve_integer_master,
            time_limit_seconds=self.config.time_limit_seconds,
            master_config=MasterConfig(
                use_facility_customer_linking=self.config.use_facility_customer_linking,
                use_min_open_facilities_bound=self.config.use_min_open_facilities_bound,
                cbc_msg=self.config.cbc_msg,
            ),
            pricing_config=PricingConfig(
                improvement_tolerance=self.config.improvement_tolerance,
                max_columns_per_facility=self.config.max_columns_per_facility,
                phase1_label_limit=self.config.phase1_label_limit,
            ),
        )
        cg_solver = ColumnGenerationSolver(self.pricing_solver, cg_config)
        return cg_solver.solve(instance)


def _build_pricing_solver(pricing: PricingChoice) -> PricingSolver:
    if pricing == "dp":
        return MESPPRCDPPricingSolver()
    if pricing == "ip":
        return MESPPRCIPPricingSolver()
    raise ValueError(f"Unknown pricing choice: {pricing!r}. Expected 'dp' or 'ip'.")
