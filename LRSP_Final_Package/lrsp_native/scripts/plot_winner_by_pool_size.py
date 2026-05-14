"""
Plot DP-vs-IP performance as a function of route-pool size.

Reads `results/lrsp_dp_vs_ip_full/raw_results.csv` (no re-running) and
produces:

  fig_winner_by_pool_size.png   — composite figure with two panels:
      (a) scatter of total runtime vs route-pool size, colored by engine,
          with timeouts plotted at the budget ceiling as open markers.
      (b) completion-aware win-rate by pool-size bucket: for each bucket,
          stacked bar shows % "DP useful" / % "IP useful" / % "both timed
          out" — same definition as the headline tally in summary.md.

Pool-size proxy. Per-pricing-call pool = total_columns / pricing_calls.
That isn't the *true* per-call Phase 1 pool (which we don't capture in
the CSV), but it's the closest available proxy. We bucket in log-spaced
ranges so the regimes separate cleanly along the x-axis.
"""

from __future__ import annotations

import argparse
import csv
import math
import sys
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CSV_PATH = REPO_ROOT / "results" / "lrsp_dp_vs_ip_full" / "raw_results.csv"
OUT_DIR  = REPO_ROOT / "results" / "lrsp_dp_vs_ip_full"


# Cell-attempts grid — must match the one paper_lrsp_dp_vs_ip_full.py used
# so the "both timed out" count can be reconstructed for cells with no row
# in the CSV.
SIZE_GRID = [
    (5, 2),  (5, 3),
    (8, 2),  (8, 4),
    (10, 2), (10, 3), (10, 5),
    (12, 3), (12, 4), (12, 6),
    (15, 3), (15, 5), (15, 8),
    (20, 4), (20, 6), (20, 10),
    (25, 5), (25, 8),
    (30, 6), (30, 10),
]
REGIMES = ["easy", "moderate", "tight"]
SEEDS = [1, 2, 3]
TIME_BUDGET_S = 30.0   # the budget the sweep used


def load_rows(path: Path) -> list[dict]:
    if not path.exists():
        raise SystemExit(f"missing {path} — run the sweep first")
    with path.open("r", newline="") as f:
        return list(csv.DictReader(f))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", type=Path, default=CSV_PATH)
    ap.add_argument("--out", type=Path, default=OUT_DIR / "fig_winner_by_pool_size.png")
    args = ap.parse_args()

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        import numpy as np
    except Exception as exc:
        raise SystemExit(f"matplotlib unavailable: {exc}")

    rows = load_rows(args.csv)

    # Index rows by (n, F, regime, seed, pricing) for paired comparisons.
    cells: dict[tuple, dict] = defaultdict(dict)
    for r in rows:
        key = (int(r["customers"]), int(r["facilities"]),
               r["regime"], int(r["seed"]))
        cells[key][r["pricing"]] = r

    # ---- pool-size proxy ----
    def pool_size(row) -> float:
        """Per-pricing-call route-pool proxy: total_columns / pricing_calls.
        Falls back to total_columns when pricing_calls is 0 (shouldn't
        happen, but defensive)."""
        pc = int(row["pricing_calls"])
        tc = int(row["total_columns"])
        if pc <= 0:
            return float(tc)
        return tc / pc

    # ---- panel (a): scatter ----
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5),
                             gridspec_kw={"width_ratios": [1, 1.1]})
    ax_a, ax_b = axes

    dp_x, dp_y = [], []
    ip_x, ip_y = [], []
    dp_to_x = []
    ip_to_x = []
    for r in rows:
        x = pool_size(r)
        y = float(r["total_seconds"])
        if r["pricing"] == "dp":
            dp_x.append(x); dp_y.append(y)
        else:
            ip_x.append(x); ip_y.append(y)

    # Plot timeouts as open triangles at the budget ceiling. Pool size for
    # a TIMEOUT cell is unknown (run never finished), so we infer from the
    # OTHER engine on the same cell when available.
    for (n, F, regime, seed), pair in cells.items():
        for engine in ("dp", "ip"):
            if engine in pair:
                continue
            other = "ip" if engine == "dp" else "dp"
            if other in pair:
                x = pool_size(pair[other])
            else:
                # both timed out: we have no pool-size signal; skip.
                continue
            (dp_to_x if engine == "dp" else ip_to_x).append(x)

    ax_a.scatter(dp_x, dp_y, s=24, alpha=0.55, color="C0", label="DP completed")
    ax_a.scatter(ip_x, ip_y, s=24, alpha=0.55, color="C1", marker="s",
                 label="IP completed")
    if dp_to_x:
        ax_a.scatter(dp_to_x, [TIME_BUDGET_S * 1.05] * len(dp_to_x),
                     marker="v", facecolors="none", edgecolors="C0",
                     s=30, alpha=0.6, label="DP timeout")
    if ip_to_x:
        ax_a.scatter(ip_to_x, [TIME_BUDGET_S * 1.18] * len(ip_to_x),
                     marker="v", facecolors="none", edgecolors="C1",
                     s=30, alpha=0.6, label="IP timeout")
    ax_a.axhline(TIME_BUDGET_S, color="grey", linestyle=":", linewidth=1)
    ax_a.set_xscale("log")
    ax_a.set_yscale("log")
    ax_a.set_xlabel("route-pool size (total_columns / pricing_calls)")
    ax_a.set_ylabel("total runtime (s, log)")
    ax_a.set_title("(a) Runtime vs route-pool size")
    ax_a.grid(True, which="both", alpha=0.25)
    ax_a.legend(loc="lower right", fontsize=8)

    # ---- panel (b): win-rate by pool-size bucket ----
    # Bucket on per-cell pool size. We use the IP pool-size proxy when IP
    # completed (more reliable); fall back to DP if only DP completed.
    # Cells where BOTH engines timed out have no pool-size signal and are
    # excluded from the bucketing — they're reported separately to keep
    # the message clean.
    bucket_edges = [0, 2, 5, 10, 20, math.inf]
    bucket_labels = ["1–2", "2–5", "5–10", "10–20", ">20"]

    def bucketize(p: float) -> int:
        for i in range(len(bucket_edges) - 1):
            if bucket_edges[i] <= p < bucket_edges[i + 1]:
                return i
        return len(bucket_labels) - 1

    attempted = [(n, F, r, s) for (n, F) in SIZE_GRID
                 for r in REGIMES for s in SEEDS]
    bucket_counts: dict[int, dict[str, int]] = {
        i: {"dp_faster": 0, "ip_faster": 0, "dp_only": 0, "ip_only": 0}
        for i in range(len(bucket_labels))
    }
    cells_excluded_both_to = 0
    for (n, F, regime, seed) in attempted:
        c = cells.get((n, F, regime, seed), {})
        has_dp = "dp" in c
        has_ip = "ip" in c
        if not has_dp and not has_ip:
            cells_excluded_both_to += 1
            continue
        p = pool_size(c["ip"]) if has_ip else pool_size(c["dp"])
        b = bucketize(p)
        if has_dp and has_ip:
            dp_t = float(c["dp"]["total_seconds"])
            ip_t = float(c["ip"]["total_seconds"])
            if dp_t < ip_t:
                bucket_counts[b]["dp_faster"] += 1
            else:
                bucket_counts[b]["ip_faster"] += 1
        elif has_dp:
            bucket_counts[b]["dp_only"] += 1
        else:
            bucket_counts[b]["ip_only"] += 1

    xs = list(range(len(bucket_labels)))
    totals = [sum(bucket_counts[i].values()) for i in xs]
    dp_fast_pct = [100 * bucket_counts[i]["dp_faster"] / max(totals[i], 1) for i in xs]
    dp_only_pct = [100 * bucket_counts[i]["dp_only"]   / max(totals[i], 1) for i in xs]
    ip_fast_pct = [100 * bucket_counts[i]["ip_faster"] / max(totals[i], 1) for i in xs]
    ip_only_pct = [100 * bucket_counts[i]["ip_only"]   / max(totals[i], 1) for i in xs]

    # Stack: DP faster (dark blue) + DP-only (light blue) | IP faster (dark
    # orange) + IP-only (light orange).
    ax_b.bar(xs, dp_fast_pct, color="#1f77b4", label="DP faster (both finished)")
    ax_b.bar(xs, dp_only_pct, bottom=dp_fast_pct, color="#9ecae1",
             label="DP only finished (IP timeout)")
    base_ip = [a + b for a, b in zip(dp_fast_pct, dp_only_pct)]
    ax_b.bar(xs, ip_fast_pct, bottom=base_ip, color="#ff7f0e",
             label="IP faster (both finished)")
    base_ip_only = [a + b for a, b in zip(base_ip, ip_fast_pct)]
    ax_b.bar(xs, ip_only_pct, bottom=base_ip_only, color="#ffbb78",
             label="IP only finished (DP timeout)")

    # Annotate cell counts above each bar.
    for i, t in enumerate(totals):
        ax_b.annotate(f"n={t}", (xs[i], 102), ha="center", fontsize=9,
                      color="dimgray")

    ax_b.set_xticks(xs)
    ax_b.set_xticklabels(bucket_labels)
    ax_b.set_xlabel("route-pool size bucket\n(total_columns / pricing_calls)")
    ax_b.set_ylabel("% of cells in bucket")
    ax_b.set_ylim(0, 110)
    ax_b.set_title(
        f"(b) Outcome by route-pool size\n"
        f"(excludes {cells_excluded_both_to} \"both timed out\" cells "
        f"with no pool-size signal)"
    )
    ax_b.legend(loc="upper left", fontsize=8)
    ax_b.grid(True, axis="y", alpha=0.25)

    fig.suptitle(
        "DP vs IP performance as a function of route-pool size\n"
        "(left: per-instance scatter; right: per-bucket outcome mix)",
        fontsize=11,
    )
    fig.tight_layout()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.out, dpi=200, bbox_inches="tight")
    plt.close(fig)
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
