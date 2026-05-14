# Hybrid pricing-engine — validation

Compares the in-solver `LRSP_PRICING_HYBRID` mode (threshold rule with T=7.29) against the DP-only and IP-only data from `cells.csv`. Each cell uses the same instance file and the same 30-second budget.

## Completion

- DP completed: 53 / 234 (22.6%)
- IP completed: 140 / 234 (59.8%)
- **Hybrid completed: 141 / 234 (60.3%)**

## Runtime vs the per-cell oracle

Per cell, define "oracle" = min(DP runtime, IP runtime) over engines that completed. The closer hybrid is to the oracle, the better the selector is doing.

- Cells with both hybrid AND at least one of DP/IP completed: 138
- Hybrid / oracle ratio: median 0.94×, mean 1.26×, max 53.26×.
- Hybrid better than oracle (impossible if selector is deterministic): 73
- Hybrid within 10% of oracle: 52
- Hybrid more than 10% slower than oracle: 13
