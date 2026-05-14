from importlib.machinery import EXTENSION_SUFFIXES

from ...mespprc_c import instance as _instance_module
from .instance import Arc, MESPPRCInstance, Node, NodeType
from .instance_generator import (
    CalibrationConfig,
    CalibrationIteration,
    CalibrationReport,
    GeneratedBenchmarkInstance,
    GeneratorConfig,
    calibrate_instance,
    generate_benchmark_instance,
    generate_instance,
)
from .label import (
    Label,
    PERM_UNREACHABLE,
    REACHABLE,
    TEMP_UNREACHABLE,
)
from .phase1 import GeneratedRoute, Phase1Result, Phase1Solver
from .route import (
    GLOBAL_LIMITS_INFEASIBLE,
    INFEASIBLE_STATUS,
    OPTIMAL_STATUS,
    ROUTE_SET_INFEASIBLE,
    Route,
    RouteReductionRecord,
)
from .phase2_dp import (
    Phase2Diagnostics,
    Phase2DPResult,
    Phase2CoveringDPSolver,
    Phase2DPSolver,
    RouteNetwork,
    RouteNetworkArc,
    RouteNetworkNode,
)
from .phase2_ip import (
    Phase2IPDiagnostics,
    Phase2IPRoutePoolDiagnostics,
    Phase2IPResult,
    Phase2IPSolver,
)

_INSTANCE_MODULE_PATH = getattr(_instance_module, "__file__", "") or ""
C_BINDINGS_AVAILABLE = any(
    _INSTANCE_MODULE_PATH.endswith(suffix) for suffix in EXTENSION_SUFFIXES
)
C_BINDINGS_STATUS = (
    "compiled_extension_modules_loaded"
    if C_BINDINGS_AVAILABLE
    else "python_source_fallback_loaded"
)

__all__ = [
    "Arc",
    "CalibrationConfig",
    "CalibrationIteration",
    "CalibrationReport",
    "C_BINDINGS_AVAILABLE",
    "C_BINDINGS_STATUS",
    "GeneratedBenchmarkInstance",
    "GeneratedRoute",
    "GeneratorConfig",
    "Label",
    "MESPPRCInstance",
    "Node",
    "NodeType",
    "PERM_UNREACHABLE",
    "Phase1Result",
    "Phase1Solver",
    "Phase2CoveringDPSolver",
    "Phase2Diagnostics",
    "Phase2DPResult",
    "Phase2DPSolver",
    "Phase2IPDiagnostics",
    "Phase2IPRoutePoolDiagnostics",
    "Phase2IPResult",
    "Phase2IPSolver",
    "GLOBAL_LIMITS_INFEASIBLE",
    "INFEASIBLE_STATUS",
    "OPTIMAL_STATUS",
    "REACHABLE",
    "ROUTE_SET_INFEASIBLE",
    "Route",
    "RouteReductionRecord",
    "RouteNetwork",
    "RouteNetworkArc",
    "RouteNetworkNode",
    "TEMP_UNREACHABLE",
    "calibrate_instance",
    "generate_benchmark_instance",
    "generate_instance",
]
