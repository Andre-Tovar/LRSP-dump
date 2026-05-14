"""
Full LRSP solver layer built around an Akca-style master and a pluggable pricing shell.

This package implements a branch-and-price architecture in the spirit of Akca's
dissertation:
- Akca-style LRSP benchmark instance generation from Cordeau MDVRP references
- a stronger restricted master problem with facility-customer linking rows
- column generation driven by interchangeable pricing backends
- default two-phase MESPPRC pricing with interchangeable Phase 2 (`dp` or `ip`)

Typical usage:

```python
from lrsp_solver import (
    LRSPSolver,
    build_akca_style_instance,
)

instance = build_akca_style_instance("p01", "f", 25, "v1", "t1")
solver = LRSPSolver(pricing_phase2="dp")
result = solver.solve_root_node(instance)
print(result.root_lp_objective, result.integer_objective)
```
"""

from .akca_instance_generator import (
    AkcaLRSPInstance,
    AkcaLRSPInstanceSpec,
    LRSPCustomer,
    LRSPFacility,
    build_akca_style_instance,
    generate_25_customer_instances,
    generate_40_customer_instances,
    iter_akca_instance_specs,
    load_cordeau_reference_data,
    validate_akca_style_instance,
)
from .branch_and_price import (
    BranchAndPriceResult,
    BranchDecision,
    BranchNode,
    LRSPBranchAndPriceSkeleton,
)
from .branching_rules import CustomerFacilityAssignment, NodeConstraints
from .benchmark_lrsp_dp_vs_ip import (
    LRSPBenchmarkRecord,
    benchmark_default_akca_instances,
    benchmark_dp_vs_ip,
)
from .lrsp_column import LRSPPairingColumn, MasterDuals
from .lrsp_column_generation import (
    ColumnGenerationConfig,
    ColumnGenerationIteration,
    ColumnGenerationResult,
    FacilityPricingSummary,
    LRSPColumnGenerationSolver,
)
from .lrsp_master import LRSPMasterSolution, MasterModelConfig, RestrictedMasterProblem
from .lrsp_solver import LRSPSolver, PricingComparisonResult
from .pricing_adapter import (
    BackendPricingResult,
    FacilityPricingAdapter,
    FacilityPricingProblem,
    FacilityPricingResult,
    LRSPPricingBackend,
    MESPPRCTwoPhasePricingBackend,
    PricingColumnCandidate,
    PricingEngineConfig,
    build_facility_pricing_instance,
)

__all__ = [
    "AkcaLRSPInstance",
    "AkcaLRSPInstanceSpec",
    "BackendPricingResult",
    "BranchAndPriceResult",
    "BranchDecision",
    "BranchNode",
    "ColumnGenerationConfig",
    "ColumnGenerationIteration",
    "ColumnGenerationResult",
    "CustomerFacilityAssignment",
    "FacilityPricingAdapter",
    "FacilityPricingProblem",
    "FacilityPricingResult",
    "FacilityPricingSummary",
    "LRSPBenchmarkRecord",
    "LRSPBranchAndPriceSkeleton",
    "LRSPColumnGenerationSolver",
    "LRSPCustomer",
    "LRSPFacility",
    "LRSPMasterSolution",
    "LRSPPairingColumn",
    "LRSPPricingBackend",
    "LRSPSolver",
    "MESPPRCTwoPhasePricingBackend",
    "MasterDuals",
    "MasterModelConfig",
    "NodeConstraints",
    "PricingColumnCandidate",
    "PricingComparisonResult",
    "PricingEngineConfig",
    "RestrictedMasterProblem",
    "benchmark_default_akca_instances",
    "benchmark_dp_vs_ip",
    "build_akca_style_instance",
    "build_facility_pricing_instance",
    "generate_25_customer_instances",
    "generate_40_customer_instances",
    "iter_akca_instance_specs",
    "load_cordeau_reference_data",
    "validate_akca_style_instance",
]
