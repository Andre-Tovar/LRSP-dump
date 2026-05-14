"""
Pricing-focused two-phase solver package.

Typical usage:

```python
from mespprc_vrp import Phase1Solver, Phase2DPPricingSolver

phase1 = Phase1Solver(instance).solve()
phase2 = Phase2DPPricingSolver(instance).solve(phase1)

if phase2.has_improving_pairing:
    print(phase2.total_cost, phase2.selected_route_ids)
```
"""

from .instance import Arc, MESPPRCInstance, Node, NodeType
from .label import Label, PERM_UNREACHABLE, REACHABLE, TEMP_UNREACHABLE
from .phase1 import GeneratedRoute, Phase1Result, Phase1Solver
from .phase2_dp import (
    Phase2Diagnostics,
    Phase2DPResult,
    Phase2DPPricingSolver,
    Phase2DPSolver,
    RouteNetwork,
    RouteNetworkArc,
    RouteNetworkNode,
)
from .phase2_ip import Phase2IPDiagnostics, Phase2IPResult, Phase2IPPricingSolver, Phase2IPSolver
from .route import (
    GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING,
    NO_FEASIBLE_NEGATIVE_PAIRING,
    NO_IMPROVING_PAIRING_STATUS,
    NO_NEGATIVE_ROUTES_FROM_PHASE1,
    OPTIMAL_STATUS,
    Route,
    RouteReductionRecord,
)

__all__ = [
    "Arc",
    "GeneratedRoute",
    "Label",
    "MESPPRCInstance",
    "Node",
    "NodeType",
    "PERM_UNREACHABLE",
    "Phase1Result",
    "Phase1Solver",
    "Phase2Diagnostics",
    "Phase2DPResult",
    "Phase2DPPricingSolver",
    "Phase2DPSolver",
    "Phase2IPDiagnostics",
    "Phase2IPResult",
    "Phase2IPPricingSolver",
    "Phase2IPSolver",
    "GLOBAL_LIMITS_PREVENT_FEASIBLE_PAIRING",
    "NO_FEASIBLE_NEGATIVE_PAIRING",
    "NO_IMPROVING_PAIRING_STATUS",
    "NO_NEGATIVE_ROUTES_FROM_PHASE1",
    "OPTIMAL_STATUS",
    "REACHABLE",
    "Route",
    "RouteNetwork",
    "RouteNetworkArc",
    "RouteNetworkNode",
    "RouteReductionRecord",
    "TEMP_UNREACHABLE",
]
