"""
LRSP (Location, Routing, and Scheduling Problem) solver.

Module layout:
- `instance` - LRSP instance dataclasses and Cordeau/Akca-style loaders
- `column` - master-problem column dataclass and dual container
- `master_problem` - PuLP-based restricted master with the Akca-style row set
- `pricing_interface` - abstract `PricingSolver` and per-call problem/result types
- `pricing_graph` - per-facility reduced-cost graph builder shared by all engines
- `pricing_dp` - MESPPRC DP-based pricing engine adapter
- `pricing_ip` - MESPPRC IP-based pricing engine adapter
- `column_generation` - root-LP column-generation loop
- `solver` - top-level `LRSPSolver` with pricing-engine selection
- `experiment_runner` - DP-vs-IP comparison helpers
- `results` - result containers used across the solver

The default solver formulation matches the Akca-style integrated LRSP master
(facility-open variables, set-partitioning over pairing columns, customer
coverage, facility capacity, per-(customer, facility) linking, lower bound on
open facilities). Pricing is per-facility and uses the user's MESPPRC engines
through clean adapters so the LRSP layer stays oblivious to which engine it is
talking to.
"""

from .column import Column, MasterDuals, deduplicate
from .column_generation import ColumnGenerationConfig, ColumnGenerationSolver
from .experiment_runner import (
    PricingComparison,
    compare_pricing_engines,
    format_comparison_table,
)
from .instance import (
    Customer,
    Facility,
    LRSPInstance,
    load_instance_from_data,
    load_instance_from_module,
    load_lrsp_instance,
    synthetic_instance,
)
from .instance_database import (
    InstanceRecord,
    filter_instances,
    iter_database_instances,
    list_database_instances,
    load_database_instance,
)
from .instance_generator import (
    GeneratorConfig,
    LRSPGeneratorError,
    REGIMES,
    generate_instance,
    regime_config,
    validate_feasibility,
)
from .instance_io import (
    instance_from_dict,
    instance_to_dict,
    read_lrsp_json,
    write_akca_txt,
    write_lrsp_json,
)
from .master_problem import MasterConfig, MasterSolution, RestrictedMasterProblem
from .pricing_dp import MESPPRCDPPricingSolver
from .pricing_graph import (
    FacilityPricingGraph,
    SOURCE_NODE_ID,
    actual_route_travel_cost,
    build_facility_pricing_graph,
)
from .pricing_interface import (
    PricingConfig,
    PricingProblem,
    PricingResult,
    PricingSolver,
)
from .pricing_ip import MESPPRCIPPricingSolver
from .results import (
    ColumnGenerationResult,
    IterationSummary,
    PricingFacilitySummary,
)
from .solver import LRSPSolver, LRSPSolverConfig

__all__ = [
    "Column",
    "ColumnGenerationConfig",
    "ColumnGenerationResult",
    "ColumnGenerationSolver",
    "Customer",
    "Facility",
    "FacilityPricingGraph",
    "GeneratorConfig",
    "InstanceRecord",
    "IterationSummary",
    "LRSPGeneratorError",
    "LRSPInstance",
    "LRSPSolver",
    "LRSPSolverConfig",
    "MESPPRCDPPricingSolver",
    "MESPPRCIPPricingSolver",
    "MasterConfig",
    "MasterDuals",
    "MasterSolution",
    "PricingComparison",
    "PricingConfig",
    "PricingFacilitySummary",
    "PricingProblem",
    "PricingResult",
    "PricingSolver",
    "REGIMES",
    "RestrictedMasterProblem",
    "SOURCE_NODE_ID",
    "actual_route_travel_cost",
    "build_facility_pricing_graph",
    "compare_pricing_engines",
    "deduplicate",
    "filter_instances",
    "format_comparison_table",
    "generate_instance",
    "instance_from_dict",
    "instance_to_dict",
    "iter_database_instances",
    "list_database_instances",
    "load_database_instance",
    "load_instance_from_data",
    "load_instance_from_module",
    "load_lrsp_instance",
    "read_lrsp_json",
    "regime_config",
    "synthetic_instance",
    "validate_feasibility",
    "write_akca_txt",
    "write_lrsp_json",
]
