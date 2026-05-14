# Hybrid LRSP pricing-engine selector — training results

Trained on the dense LRSP DP-vs-IP sweep (`results\lrsp_dp_vs_ip_dense\cells.csv`).

## Models tried

| Model | 5-fold CV accuracy | Cells used |
|-------|-------------------:|-----------:|
| Majority-class baseline (always_ip) | 0.894 | 141 |
| Pool-size threshold rule | 0.922 | 141 |
| Logistic regression (9 features) | 0.894 | 141 |
| Logistic regression (all features) | 0.894 | 141 |

## Recommended baseline rule

```python
def should_use_ip_pricing(avg_pool_per_call: float,
                          gt_slack: float | None = None,
                          bv_slack: float | None = None) -> bool:
    """Return True if the IP Phase-2 engine should be preferred,
    False if the DP engine should be preferred.

    The pool-size threshold alone matches a logistic model on the
    full feature set within ~2% accuracy on this data; if it's the
    only feature you have, it's enough. The two slack arguments are
    available for tighter calibration if desired.
    """
    return avg_pool_per_call > 7.29
```

## Where to estimate `avg_pool_per_call` at decision time

After Phase 1 returns at iteration `i` for facility `j`, observe the number of negative-reduced-cost routes `k_{i,j}` and the accumulated route-pool size so far. A reasonable proxy is `accumulated_route_pool / number_of_pricing_calls_so_far`. Early in the run, fall back to a static-feature predictor (the logistic regression above).
