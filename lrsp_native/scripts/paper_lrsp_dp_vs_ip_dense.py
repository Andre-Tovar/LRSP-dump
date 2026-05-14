"""
Dense LRSP DP-vs-IP sweep + hybrid-model feature capture.

Sampling design:
    N = 5, 6, 7, ..., 30        (26 values; one per integer)
    F = max(2, N // 3)          (deterministic — no F-confound)
    regimes = easy / moderate / tight   (3)
    seeds = 1, 2, 3                     (3)
    => 26 * 3 * 3 = 234 cells
    Both engines per cell, time budget 30 s each
    => up to ~234 * 2 * 30 = 14 040 s ≈ 4 hours wall clock worst case.

For each cell we record:

    Instance-level (computed once from the JSON; same for DP and IP rows):
        N, F, regime, seed
        bv_factor, bf_factor, gt_factor               # the regime knobs
        total_demand, mean_demand, max_demand, std_demand
        vehicle_capacity, vehicle_time_limit, vehicle_fixed_cost
        mean_cust_nearest_fac_dist, max_cust_nearest_fac_dist
        mean_cust_cust_dist
        demand_to_capacity_ratio   = total_demand / sum_facility_capacity
        max_singleton_round_trip   = 2 * op * max_i min_j d(i,j)
        avg_singleton_round_trip   = 2 * op * mean_i min_j d(i,j)
        gt_slack                   = vehicle_time_limit / max_singleton_round_trip
        bv_slack                   = vehicle_capacity / max_demand

    Per-(engine, instance) (output of run_lrsp.exe):
        pricing                     ("dp" or "ip")
        status, iterations, pricing_calls
        total_columns, seed_columns, phase1_route_columns, phase2_pairing_columns
        max_routes_per_column, avg_routes_per_pairing
        reached_optimality
        total_seconds, master_seconds, pricing_seconds
        root_lp_objective, integer_objective
        open_facilities

    Derived (computed at write time):
        avg_pool_per_call          = total_columns / pricing_calls
        timed_out                  = 1 if no row produced

We deliberately capture the regime knobs as numbers (bv_factor, bf_factor,
gt_factor) so a downstream classifier can use them as continuous inputs
rather than categorical labels.

Usage:
    python lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py
    python lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py --reanalyze

Outputs land in `results/lrsp_dp_vs_ip_dense/`:
    raw_results.csv              one row per (instance, engine) successful run
    cells.csv                    one row per (n, F, regime, seed) attempt:
                                 instance features + DP outcome + IP outcome
                                 (this is the model-training table)
    summary.md                   end-to-end argument with figures
    fig_*.png                    figures listed in the report
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import statistics
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
RUN_LRSP = REPO_ROOT / "lrsp_native" / "build" / "bin" / "run_lrsp.exe"

if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))


# -----------------------------------------------------------------------------
# Sweep design
# -----------------------------------------------------------------------------


N_VALUES = list(range(5, 31))                    # 5..30
F_FOR = lambda n: max(2, n // 3)                 # deterministic
REGIMES = ["easy", "moderate", "tight"]
SEEDS = [1, 2, 3]

# Regime factors (must mirror lrsp_solver/instance_generator.py::REGIMES).
REGIME_FACTORS: dict[str, dict[str, float]] = {
    "easy":     {"bv": 4.0, "bf": 3.0, "gt": 4.0},
    "moderate": {"bv": 2.5, "bf": 2.0, "gt": 2.5},
    "tight":    {"bv": 1.7, "bf": 1.4, "gt": 1.7},
}


# -----------------------------------------------------------------------------
# Run-result schema
# -----------------------------------------------------------------------------


@dataclass(slots=True)
class InstanceFeatures:
    n: int
    f: int
    regime: str
    seed: int

    bv_factor: float
    bf_factor: float
    gt_factor: float

    total_demand: float
    mean_demand: float
    max_demand: float
    std_demand: float

    vehicle_capacity: float
    vehicle_time_limit: float
    vehicle_fixed_cost: float

    mean_cust_nearest_fac_dist: float
    max_cust_nearest_fac_dist: float
    mean_cust_cust_dist: float

    demand_to_capacity_ratio: float
    max_singleton_round_trip: float
    avg_singleton_round_trip: float
    gt_slack: float
    bv_slack: float


@dataclass(slots=True)
class EngineRow:
    pricing: str
    status: str
    iterations: int
    pricing_calls: int
    total_columns: int
    seed_columns: int
    phase1_route_columns: int
    phase2_pairing_columns: int
    max_routes_per_column: int
    avg_routes_per_pairing: float
    reached_optimality: int
    total_seconds: float
    master_seconds: float
    pricing_seconds: float
    root_lp: Optional[float]
    integer: Optional[float]
    open_facilities: int


_RE = {
    "status": re.compile(r"^status:\s+(\S+)"),
    "iterations": re.compile(r"^iterations:\s+(\d+)"),
    "pricing_calls": re.compile(r"^pricing_calls:\s+(\d+)"),
    "total_columns": re.compile(r"^total_columns:\s+(\d+)"),
    "seed_columns": re.compile(r"^seed_columns:\s+(\d+)"),
    "phase1_route": re.compile(r"^phase1_route_columns:\s+(\d+)"),
    "phase2_pairing": re.compile(r"^phase2_pairing_columns:\s+(\d+)"),
    "max_routes_per_col": re.compile(r"^max_routes_per_column:\s+(\d+)"),
    "avg_routes_per_pair": re.compile(r"^avg_routes_per_pairing:\s+([\d.]+)"),
    "reached": re.compile(r"^reached_optimality:\s+(\d+)"),
    "total": re.compile(r"^total_runtime:\s+([\d.]+)s"),
    "master": re.compile(r"^master_runtime:\s+([\d.]+)s"),
    "pricing": re.compile(r"^pricing_runtime:\s+([\d.]+)s"),
    "root_lp": re.compile(r"^root_lp_objective:\s+([\-\d.eE+]+)"),
    "integer": re.compile(r"^integer_objective:\s+([\-\d.eE+]+)"),
    "open": re.compile(r"^open_facilities:\s+(\d+)"),
}


def parse_run(stdout: str) -> dict:
    fields: dict = {}
    for line in stdout.splitlines():
        s = line.strip()
        for k, rx in _RE.items():
            m = rx.match(s)
            if m and k not in fields:
                fields[k] = m.group(1)
    return fields


def run_engine(instance_path: Path, pricing: str, time_limit: float
               ) -> Optional[EngineRow]:
    cmd = [str(RUN_LRSP),
           "--instance", str(instance_path),
           "--pricing", pricing,
           "--max-iterations", "50",
           "--max-cols-per-facility", "16",
           "--time-limit-seconds", str(time_limit)]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True,
                             timeout=time_limit + 10)
    except subprocess.TimeoutExpired:
        return None
    if out.returncode != 0:
        return None
    f = parse_run(out.stdout)
    if not f.get("total"):
        return None
    return EngineRow(
        pricing=pricing,
        status=f.get("status", "?"),
        iterations=int(f.get("iterations", 0)),
        pricing_calls=int(f.get("pricing_calls", 0)),
        total_columns=int(f.get("total_columns", 0)),
        seed_columns=int(f.get("seed_columns", 0)),
        phase1_route_columns=int(f.get("phase1_route", 0)),
        phase2_pairing_columns=int(f.get("phase2_pairing", 0)),
        max_routes_per_column=int(f.get("max_routes_per_col", 0)),
        avg_routes_per_pairing=float(f.get("avg_routes_per_pair", 0.0)),
        reached_optimality=int(f.get("reached", 0)),
        total_seconds=float(f["total"]),
        master_seconds=float(f.get("master", 0.0)),
        pricing_seconds=float(f.get("pricing", 0.0)),
        root_lp=float(f["root_lp"]) if "root_lp" in f else None,
        integer=float(f["integer"]) if "integer" in f else None,
        open_facilities=int(f.get("open", 0)),
    )


# -----------------------------------------------------------------------------
# Instance feature extraction
# -----------------------------------------------------------------------------


def euclidean(p, q) -> float:
    dx = p[0] - q[0]
    dy = p[1] - q[1]
    return math.hypot(dx, dy)


def compute_instance_features(json_path: Path, n: int, f: int,
                               regime: str, seed: int) -> InstanceFeatures:
    """Read the bundled .lrsp.json and compute features useful for the
    future hybrid-decision classifier."""
    import json
    with json_path.open("r", encoding="utf-8") as h:
        d = json.load(h)
    cust = d["customers"]
    fac = d["facilities"]
    op = float(d["vehicle_operating_cost"])

    demands = [float(c["demand"]) for c in cust]
    cust_xy = [(float(c["x"]), float(c["y"])) for c in cust]
    fac_xy = [(float(f["x"]), float(f["y"])) for f in fac]

    nearest_fac_dist = [
        min(op * euclidean(c, f) for f in fac_xy) for c in cust_xy
    ]
    cust_cust_dists: list[float] = []
    for i in range(len(cust_xy)):
        for j in range(i + 1, len(cust_xy)):
            cust_cust_dists.append(op * euclidean(cust_xy[i], cust_xy[j]))

    total_demand = float(sum(demands))
    sum_fac_cap = float(sum(float(f["capacity"]) for f in fac))

    max_singleton_round_trip = 2.0 * max(nearest_fac_dist) if nearest_fac_dist else 0.0
    avg_singleton_round_trip = 2.0 * statistics.fmean(nearest_fac_dist) if nearest_fac_dist else 0.0

    factors = REGIME_FACTORS[regime]
    return InstanceFeatures(
        n=n, f=f, regime=regime, seed=seed,
        bv_factor=factors["bv"], bf_factor=factors["bf"], gt_factor=factors["gt"],
        total_demand=total_demand,
        mean_demand=statistics.fmean(demands) if demands else 0.0,
        max_demand=max(demands) if demands else 0.0,
        std_demand=statistics.pstdev(demands) if len(demands) > 1 else 0.0,
        vehicle_capacity=float(d["vehicle_capacity"]),
        vehicle_time_limit=float(d["vehicle_time_limit"]),
        vehicle_fixed_cost=float(d["vehicle_fixed_cost"]),
        mean_cust_nearest_fac_dist=(
            statistics.fmean(nearest_fac_dist) if nearest_fac_dist else 0.0
        ),
        max_cust_nearest_fac_dist=max(nearest_fac_dist) if nearest_fac_dist else 0.0,
        mean_cust_cust_dist=(
            statistics.fmean(cust_cust_dists) if cust_cust_dists else 0.0
        ),
        demand_to_capacity_ratio=total_demand / max(sum_fac_cap, 1e-9),
        max_singleton_round_trip=max_singleton_round_trip,
        avg_singleton_round_trip=avg_singleton_round_trip,
        gt_slack=(
            float(d["vehicle_time_limit"]) / max(max_singleton_round_trip, 1e-9)
        ),
        bv_slack=(
            float(d["vehicle_capacity"]) / max(max(demands) if demands else 1, 1e-9)
        ),
    )


# -----------------------------------------------------------------------------
# Instance generation
# -----------------------------------------------------------------------------


def ensure_instances() -> tuple[int, int]:
    from lrsp_solver import (
        generate_instance, regime_config, write_lrsp_json, write_akca_txt,
    )
    from lrsp_solver.instance import LRSPInstance

    inst_dir = REPO_ROOT / "lrsp_solver" / "instance_db" / "instances"
    inst_dir.mkdir(parents=True, exist_ok=True)
    have = made = 0
    for n in N_VALUES:
        f = F_FOR(n)
        for regime in REGIMES:
            for seed in SEEDS:
                instance_id = f"lrsp_n{n:03d}_f{f:02d}_{regime}_s{seed}"
                txt_p = inst_dir / f"{instance_id}.txt"
                json_p = inst_dir / f"{instance_id}.lrsp.json"
                if txt_p.exists() and json_p.exists():
                    have += 1
                    continue
                cfg = regime_config(regime, num_customers=n, num_facilities=f, seed=seed)
                inst = generate_instance(cfg)
                inst = LRSPInstance(
                    name=instance_id,
                    customers=inst.customers,
                    facilities=inst.facilities,
                    vehicle_capacity=inst.vehicle_capacity,
                    vehicle_fixed_cost=inst.vehicle_fixed_cost,
                    vehicle_operating_cost=inst.vehicle_operating_cost,
                    vehicle_time_limit=inst.vehicle_time_limit,
                    notes=inst.notes,
                )
                write_lrsp_json(inst, json_p)
                write_akca_txt(inst, txt_p)
                made += 1
    return have, made


# -----------------------------------------------------------------------------
# CSV I/O
# -----------------------------------------------------------------------------


# raw_results.csv: one row per successful (instance, engine) run.
RAW_HEADER = [
    "instance", "n", "f", "regime", "seed", "pricing",
    "status", "iterations", "pricing_calls",
    "total_columns", "seed_columns",
    "phase1_route_columns", "phase2_pairing_columns",
    "max_routes_per_column", "avg_routes_per_pairing",
    "reached_optimality",
    "total_seconds", "master_seconds", "pricing_seconds",
    "root_lp_objective", "integer_objective", "open_facilities",
]

# cells.csv: one row per attempted cell — the model training table.
CELL_HEADER = [
    # cell coordinates
    "instance", "n", "f", "regime", "seed",
    # regime numerical knobs
    "bv_factor", "bf_factor", "gt_factor",
    # static instance features
    "total_demand", "mean_demand", "max_demand", "std_demand",
    "vehicle_capacity", "vehicle_time_limit", "vehicle_fixed_cost",
    "mean_cust_nearest_fac_dist", "max_cust_nearest_fac_dist",
    "mean_cust_cust_dist",
    "demand_to_capacity_ratio", "max_singleton_round_trip",
    "avg_singleton_round_trip", "gt_slack", "bv_slack",
    # DP outcome
    "dp_completed", "dp_total_seconds", "dp_master_seconds",
    "dp_pricing_seconds", "dp_iterations", "dp_pricing_calls",
    "dp_total_columns", "dp_phase1_route_columns",
    "dp_phase2_pairing_columns", "dp_max_routes_per_column",
    "dp_avg_pool_per_call",
    "dp_root_lp", "dp_integer",
    # IP outcome
    "ip_completed", "ip_total_seconds", "ip_master_seconds",
    "ip_pricing_seconds", "ip_iterations", "ip_pricing_calls",
    "ip_total_columns", "ip_phase1_route_columns",
    "ip_phase2_pairing_columns", "ip_max_routes_per_column",
    "ip_avg_pool_per_call",
    "ip_root_lp", "ip_integer",
    # decision target (label for the future hybrid model)
    "winner",                 # "dp" / "ip" / "tie" / "dp_only" / "ip_only" / "both_to"
    "dp_useful",              # 1 if DP was the winner OR the only one that finished
    "ip_useful",              # 1 if IP was the winner OR the only one that finished
    "speedup_ip_over_dp",     # dp / ip when both finished; 'inf' if DP timed out and IP didn't
    "log_speedup",            # log10 of the same; '' when not defined
    "avg_pool_per_call",      # average across whichever engines finished
]


def write_raw_csv(rows: list[tuple[str, InstanceFeatures, EngineRow]],
                  path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as h:
        w = csv.writer(h)
        w.writerow(RAW_HEADER)
        for instance_id, feat, r in rows:
            w.writerow([
                instance_id, feat.n, feat.f, feat.regime, feat.seed, r.pricing,
                r.status, r.iterations, r.pricing_calls,
                r.total_columns, r.seed_columns,
                r.phase1_route_columns, r.phase2_pairing_columns,
                r.max_routes_per_column, f"{r.avg_routes_per_pairing:.4f}",
                r.reached_optimality,
                f"{r.total_seconds:.6f}", f"{r.master_seconds:.6f}",
                f"{r.pricing_seconds:.6f}",
                "" if r.root_lp is None else f"{r.root_lp:.6f}",
                "" if r.integer is None else f"{r.integer:.6f}",
                r.open_facilities,
            ])


def load_raw_csv(path: Path) -> list[dict]:
    if not path.exists():
        return []
    with path.open("r", newline="") as h:
        return list(csv.DictReader(h))


def write_cells_csv(cells: list[dict], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as h:
        w = csv.writer(h)
        w.writerow(CELL_HEADER)
        for c in cells:
            w.writerow([c.get(k, "") for k in CELL_HEADER])


# -----------------------------------------------------------------------------
# Cell-level aggregation
# -----------------------------------------------------------------------------


def make_cell_row(instance_id: str, feat: InstanceFeatures,
                  dp: Optional[EngineRow], ip: Optional[EngineRow]) -> dict:
    base = {
        "instance": instance_id,
        "n": feat.n, "f": feat.f, "regime": feat.regime, "seed": feat.seed,
        "bv_factor": feat.bv_factor, "bf_factor": feat.bf_factor,
        "gt_factor": feat.gt_factor,
        "total_demand": feat.total_demand, "mean_demand": feat.mean_demand,
        "max_demand": feat.max_demand, "std_demand": feat.std_demand,
        "vehicle_capacity": feat.vehicle_capacity,
        "vehicle_time_limit": feat.vehicle_time_limit,
        "vehicle_fixed_cost": feat.vehicle_fixed_cost,
        "mean_cust_nearest_fac_dist": feat.mean_cust_nearest_fac_dist,
        "max_cust_nearest_fac_dist": feat.max_cust_nearest_fac_dist,
        "mean_cust_cust_dist": feat.mean_cust_cust_dist,
        "demand_to_capacity_ratio": feat.demand_to_capacity_ratio,
        "max_singleton_round_trip": feat.max_singleton_round_trip,
        "avg_singleton_round_trip": feat.avg_singleton_round_trip,
        "gt_slack": feat.gt_slack, "bv_slack": feat.bv_slack,
    }

    def engine_block(prefix: str, r: Optional[EngineRow]) -> dict:
        if r is None:
            return {f"{prefix}_completed": 0,
                    f"{prefix}_total_seconds": "",
                    f"{prefix}_master_seconds": "",
                    f"{prefix}_pricing_seconds": "",
                    f"{prefix}_iterations": "",
                    f"{prefix}_pricing_calls": "",
                    f"{prefix}_total_columns": "",
                    f"{prefix}_phase1_route_columns": "",
                    f"{prefix}_phase2_pairing_columns": "",
                    f"{prefix}_max_routes_per_column": "",
                    f"{prefix}_avg_pool_per_call": "",
                    f"{prefix}_root_lp": "",
                    f"{prefix}_integer": ""}
        avg_pool = (r.total_columns / r.pricing_calls) if r.pricing_calls > 0 else 0.0
        return {
            f"{prefix}_completed": 1,
            f"{prefix}_total_seconds": f"{r.total_seconds:.6f}",
            f"{prefix}_master_seconds": f"{r.master_seconds:.6f}",
            f"{prefix}_pricing_seconds": f"{r.pricing_seconds:.6f}",
            f"{prefix}_iterations": r.iterations,
            f"{prefix}_pricing_calls": r.pricing_calls,
            f"{prefix}_total_columns": r.total_columns,
            f"{prefix}_phase1_route_columns": r.phase1_route_columns,
            f"{prefix}_phase2_pairing_columns": r.phase2_pairing_columns,
            f"{prefix}_max_routes_per_column": r.max_routes_per_column,
            f"{prefix}_avg_pool_per_call": f"{avg_pool:.4f}",
            f"{prefix}_root_lp": "" if r.root_lp is None else f"{r.root_lp:.6f}",
            f"{prefix}_integer": "" if r.integer is None else f"{r.integer:.6f}",
        }

    base.update(engine_block("dp", dp))
    base.update(engine_block("ip", ip))

    # Decision target.
    if dp and ip:
        if dp.total_seconds < ip.total_seconds:
            winner = "dp"
        elif ip.total_seconds < dp.total_seconds:
            winner = "ip"
        else:
            winner = "tie"
        speedup = ip.total_seconds / max(dp.total_seconds, 1e-12)
        # We define "speedup_ip_over_dp" as dp / ip so >1 means IP wins.
        ratio = dp.total_seconds / max(ip.total_seconds, 1e-12)
        base["speedup_ip_over_dp"] = f"{ratio:.6f}"
        base["log_speedup"] = f"{math.log10(ratio):.4f}" if ratio > 0 else ""
    elif dp and not ip:
        winner = "dp_only"
        base["speedup_ip_over_dp"] = ""
        base["log_speedup"] = ""
    elif ip and not dp:
        winner = "ip_only"
        base["speedup_ip_over_dp"] = ""
        base["log_speedup"] = ""
    else:
        winner = "both_to"
        base["speedup_ip_over_dp"] = ""
        base["log_speedup"] = ""

    base["winner"] = winner
    base["dp_useful"] = 1 if winner in ("dp", "tie", "dp_only") else 0
    base["ip_useful"] = 1 if winner in ("ip", "tie", "ip_only") else 0

    # avg_pool_per_call: take whichever finished (prefer IP since it has
    # higher completion) for use as a single feature.
    if ip:
        ap = ip.total_columns / ip.pricing_calls if ip.pricing_calls > 0 else 0.0
        base["avg_pool_per_call"] = f"{ap:.4f}"
    elif dp:
        ap = dp.total_columns / dp.pricing_calls if dp.pricing_calls > 0 else 0.0
        base["avg_pool_per_call"] = f"{ap:.4f}"
    else:
        base["avg_pool_per_call"] = ""

    return base


# -----------------------------------------------------------------------------
# Plotting + summary
# -----------------------------------------------------------------------------


def make_plots(rows_by_engine: dict[tuple, EngineRow],
               cell_rows: list[dict], out_dir: Path,
               time_budget_s: float) -> dict[str, Path]:
    """Generate the figures the report references."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as exc:
        print(f"matplotlib unavailable: {exc}")
        return {}
    out_dir.mkdir(parents=True, exist_ok=True)
    paths: dict[str, Path] = {}

    Ns = sorted({c["n"] for c in cell_rows})

    # ---- F1: completion rate vs N (per regime) -----------------------------
    fig, axes = plt.subplots(1, len(REGIMES), figsize=(15, 4.5), sharey=True)
    for ax, regime in zip(axes, REGIMES):
        for engine, color, marker in [("dp", "C0", "o"), ("ip", "C1", "s")]:
            xs, ys = [], []
            for n in Ns:
                cells = [c for c in cell_rows
                         if c["n"] == n and c["regime"] == regime]
                if not cells:
                    continue
                key = f"{engine}_completed"
                completion = sum(int(c[key]) for c in cells) / len(cells)
                xs.append(n)
                ys.append(completion)
            if xs:
                ax.plot(xs, ys, marker=marker, color=color,
                        label=engine.upper(), linewidth=2, markersize=5)
        ax.set_xlabel("customers (N)")
        ax.set_title(f"regime: {regime}")
        ax.set_ylim(-0.05, 1.05)
        ax.grid(True, alpha=0.3)
        ax.legend()
    axes[0].set_ylabel("completion rate")
    fig.suptitle(f"Completion rate vs N (per regime), {time_budget_s:.0f}s budget")
    fig.tight_layout()
    p = out_dir / "fig_completion_dense.png"
    fig.savefig(p, dpi=200, bbox_inches="tight")
    plt.close(fig)
    paths["completion_dense"] = p

    # ---- F2: median runtime vs N (per regime), log-y -----------------------
    fig, axes = plt.subplots(1, len(REGIMES), figsize=(15, 4.5), sharey=True)
    for ax, regime in zip(axes, REGIMES):
        for engine, color, marker in [("dp", "C0", "o"), ("ip", "C1", "s")]:
            xs, ys = [], []
            for n in Ns:
                vals = [
                    float(c[f"{engine}_total_seconds"])
                    for c in cell_rows
                    if c["n"] == n and c["regime"] == regime
                    and int(c[f"{engine}_completed"]) == 1
                ]
                if vals:
                    xs.append(n)
                    ys.append(statistics.median(vals))
            if xs:
                ax.plot(xs, ys, marker=marker, color=color,
                        label=engine.upper(), linewidth=2, markersize=5)
        ax.set_yscale("log")
        ax.set_xlabel("customers (N)")
        ax.set_title(f"regime: {regime}")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()
    axes[0].set_ylabel("median runtime (s, log)")
    fig.suptitle("Median runtime vs N, completed runs only (per regime)")
    fig.tight_layout()
    p = out_dir / "fig_runtime_dense.png"
    fig.savefig(p, dpi=200, bbox_inches="tight")
    plt.close(fig)
    paths["runtime_dense"] = p

    # ---- F3: useful-rate vs N (per regime) — what the user asks at deploy --
    fig, axes = plt.subplots(1, len(REGIMES), figsize=(15, 4.5), sharey=True)
    for ax, regime in zip(axes, REGIMES):
        for engine, color, marker in [("dp", "C0", "o"), ("ip", "C1", "s")]:
            xs, ys = [], []
            for n in Ns:
                cells = [c for c in cell_rows
                         if c["n"] == n and c["regime"] == regime]
                if not cells:
                    continue
                key = f"{engine}_useful"
                rate = sum(int(c[key]) for c in cells) / len(cells)
                xs.append(n)
                ys.append(rate)
            if xs:
                ax.plot(xs, ys, marker=marker, color=color,
                        label=engine.upper(), linewidth=2, markersize=5)
        ax.set_xlabel("customers (N)")
        ax.set_title(f"regime: {regime}")
        ax.set_ylim(-0.05, 1.05)
        ax.grid(True, alpha=0.3)
        ax.legend()
    axes[0].set_ylabel("useful rate (= completed AND not slower than the other)")
    fig.suptitle("Engine usefulness rate vs N (per regime)")
    fig.tight_layout()
    p = out_dir / "fig_useful_rate.png"
    fig.savefig(p, dpi=200, bbox_inches="tight")
    plt.close(fig)
    paths["useful_rate"] = p

    # ---- F4: winner-by-pool-size (refined version) -------------------------
    bucket_edges = [0, 5, 10, 15, 20, 30, math.inf]
    bucket_labels = ["≤5", "5–10", "10–15", "15–20", "20–30", ">30"]

    def bucketize(p: float) -> int:
        for i in range(len(bucket_edges) - 1):
            if bucket_edges[i] <= p < bucket_edges[i + 1]:
                return i
        return len(bucket_labels) - 1

    bucket_counts = {i: {"dp_faster": 0, "ip_faster": 0,
                         "dp_only": 0, "ip_only": 0, "both_to": 0}
                     for i in range(len(bucket_labels))}
    bucket_no_pool = {"both_to": 0}
    for c in cell_rows:
        winner = c["winner"]
        ap = c.get("avg_pool_per_call", "")
        if winner == "both_to" or ap == "":
            bucket_no_pool["both_to"] += 1
            continue
        b = bucketize(float(ap))
        if winner == "dp":
            bucket_counts[b]["dp_faster"] += 1
        elif winner == "ip":
            bucket_counts[b]["ip_faster"] += 1
        elif winner == "dp_only":
            bucket_counts[b]["dp_only"] += 1
        elif winner == "ip_only":
            bucket_counts[b]["ip_only"] += 1
        elif winner == "tie":
            bucket_counts[b]["dp_faster"] += 1  # ties → DP-favorable (microseconds)

    fig, ax = plt.subplots(figsize=(10, 5.5))
    xs = list(range(len(bucket_labels)))
    totals = [sum(bucket_counts[i].values()) for i in xs]
    dp_fast_pct = [100 * bucket_counts[i]["dp_faster"] / max(totals[i], 1) for i in xs]
    dp_only_pct = [100 * bucket_counts[i]["dp_only"]   / max(totals[i], 1) for i in xs]
    ip_fast_pct = [100 * bucket_counts[i]["ip_faster"] / max(totals[i], 1) for i in xs]
    ip_only_pct = [100 * bucket_counts[i]["ip_only"]   / max(totals[i], 1) for i in xs]

    ax.bar(xs, dp_fast_pct, color="#1f77b4", label="DP faster (both finished)")
    ax.bar(xs, dp_only_pct, bottom=dp_fast_pct, color="#9ecae1",
           label="DP only finished")
    base_ip = [a + b for a, b in zip(dp_fast_pct, dp_only_pct)]
    ax.bar(xs, ip_fast_pct, bottom=base_ip, color="#ff7f0e",
           label="IP faster (both finished)")
    base_ip_only = [a + b for a, b in zip(base_ip, ip_fast_pct)]
    ax.bar(xs, ip_only_pct, bottom=base_ip_only, color="#ffbb78",
           label="IP only finished")
    for i, t in enumerate(totals):
        ax.annotate(f"n={t}", (xs[i], 102), ha="center", fontsize=9, color="dimgray")
    ax.set_xticks(xs)
    ax.set_xticklabels(bucket_labels)
    ax.set_xlabel("avg route-pool size per pricing call (total_columns / pricing_calls)")
    ax.set_ylabel("% of cells in bucket")
    ax.set_ylim(0, 110)
    ax.set_title(
        f"Outcome by route-pool size (excludes {bucket_no_pool['both_to']} "
        f"\"both timed out\" cells with no pool-size signal)"
    )
    ax.legend(loc="upper left", fontsize=9)
    ax.grid(True, axis="y", alpha=0.25)
    fig.tight_layout()
    p = out_dir / "fig_winner_by_pool_size_dense.png"
    fig.savefig(p, dpi=200, bbox_inches="tight")
    plt.close(fig)
    paths["winner_by_pool_size_dense"] = p

    # ---- F5: scatter of speedup-ratio vs avg_pool, log-log ---------------
    fig, ax = plt.subplots(figsize=(9, 5.5))
    xs_pool = []
    ys_ratio = []
    colors = []
    cmap = {"easy": "C2", "moderate": "C0", "tight": "C1"}
    for c in cell_rows:
        if c["winner"] not in ("dp", "ip", "tie"):
            continue
        ap = c.get("avg_pool_per_call", "")
        sp = c.get("speedup_ip_over_dp", "")
        if ap == "" or sp == "":
            continue
        try:
            xs_pool.append(float(ap))
            ys_ratio.append(float(sp))
            colors.append(cmap[c["regime"]])
        except ValueError:
            continue
    if xs_pool:
        ax.scatter(xs_pool, ys_ratio, c=colors, s=24, alpha=0.7)
    ax.axhline(1.0, color="grey", linestyle="--", label="parity")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("avg route-pool per pricing call")
    ax.set_ylabel("DP runtime / IP runtime  (>1 means IP faster)")
    ax.set_title("DP/IP runtime ratio vs route-pool size, both-completed cells")
    # Custom legend
    from matplotlib.patches import Patch
    handles = [Patch(color=cmap[r], label=r) for r in REGIMES]
    handles.append(plt.Line2D([0], [0], color="grey", linestyle="--", label="parity"))
    ax.legend(handles=handles, loc="upper left")
    ax.grid(True, which="both", alpha=0.25)
    fig.tight_layout()
    p = out_dir / "fig_speedup_vs_pool.png"
    fig.savefig(p, dpi=200, bbox_inches="tight")
    plt.close(fig)
    paths["speedup_vs_pool"] = p

    return paths


# -----------------------------------------------------------------------------
# Markdown summary
# -----------------------------------------------------------------------------


def write_summary(cell_rows: list[dict], paths: dict[str, Path], out: Path,
                  time_budget_s: float) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    n_cells = len(cell_rows)
    Ns = sorted({c["n"] for c in cell_rows})

    # Aggregates
    dp_done = sum(int(c["dp_completed"]) for c in cell_rows)
    ip_done = sum(int(c["ip_completed"]) for c in cell_rows)
    both_done = sum(1 for c in cell_rows
                    if int(c["dp_completed"]) and int(c["ip_completed"]))
    both_to = sum(1 for c in cell_rows
                  if not int(c["dp_completed"]) and not int(c["ip_completed"]))

    def per_regime(regime):
        rs = [c for c in cell_rows if c["regime"] == regime]
        n = len(rs)
        du = sum(int(c["dp_useful"]) for c in rs)
        iu = sum(int(c["ip_useful"]) for c in rs)
        bt = sum(1 for c in rs if c["winner"] == "both_to")
        return n, du, iu, bt

    lines: list[str] = []
    lines.append("# LRSP DP vs IP — dense sweep (model-training data set)")
    lines.append("")
    lines.append(
        f"This sweep is the data set the future hybrid pricing-engine "
        f"selector will train on. It samples N continuously from "
        f"{N_VALUES[0]} to {N_VALUES[-1]} with a deterministic facility "
        f"count F = max(2, N // 3) — no F-confound. Three regimes "
        f"({', '.join(REGIMES)}) and three seeds per regime."
    )
    lines.append("")
    lines.append("## Sweep parameters")
    lines.append("")
    lines.append(f"- N (customers): {N_VALUES[0]}..{N_VALUES[-1]} (every integer, {len(N_VALUES)} values)")
    lines.append(f"- F (facilities): deterministic, F(N) = max(2, N // 3)")
    lines.append(f"- Regimes: {REGIMES} (β_v / β_f / γ_t per the table below)")
    lines.append(f"- Seeds: {SEEDS}")
    lines.append(f"- Total cells: {len(N_VALUES)} × {len(REGIMES)} × {len(SEEDS)} = {n_cells}")
    lines.append(f"- Both engines per cell, time budget {time_budget_s:.0f} s each.")
    lines.append("")
    lines.append("Regime knobs (numerical, captured per-cell as features):")
    lines.append("")
    lines.append("| Regime   | β_v | β_f | γ_t |")
    lines.append("|----------|-----|-----|-----|")
    for r, k in REGIME_FACTORS.items():
        lines.append(f"| {r} | {k['bv']} | {k['bf']} | {k['gt']} |")
    lines.append("")

    # Top-line aggregates
    lines.append("## Top-line numbers")
    lines.append("")
    lines.append(
        f"- {n_cells} cells attempted. DP completed {dp_done} "
        f"({dp_done/n_cells*100:.1f}%); IP completed {ip_done} "
        f"({ip_done/n_cells*100:.1f}%). Both completed: {both_done} cells; "
        f"both timed out: {both_to} cells."
    )
    lines.append("")
    lines.append("| Regime | total | DP useful | IP useful | both timed out |")
    lines.append("|--------|------:|----------:|----------:|---------------:|")
    for r in REGIMES:
        n, du, iu, bt = per_regime(r)
        lines.append(
            f"| {r} | {n} | "
            f"{du} ({du/n*100:.0f}%) | {iu} ({iu/n*100:.0f}%) | "
            f"{bt} ({bt/n*100:.0f}%) |"
        )
    n_t, du_t, iu_t, bt_t = (
        n_cells, sum(int(c["dp_useful"]) for c in cell_rows),
        sum(int(c["ip_useful"]) for c in cell_rows),
        sum(1 for c in cell_rows if c["winner"] == "both_to"),
    )
    lines.append(
        f"| **all** | {n_t} | "
        f"**{du_t} ({du_t/n_t*100:.0f}%)** | "
        f"**{iu_t} ({iu_t/n_t*100:.0f}%)** | "
        f"{bt_t} ({bt_t/n_t*100:.0f}%) |"
    )
    lines.append("")
    lines.append(
        "*\"useful\" = engine completed within budget AND was no slower than "
        "the other engine that also completed (or was the only one to "
        "complete). Same definition used in the v1 sparse sweep so the two "
        "are directly comparable.*"
    )
    lines.append("")

    # Find DP-completion cliff per regime (smallest N where DP completion < 50%)
    lines.append("## DP completion cliff (per regime)")
    lines.append("")
    lines.append("Smallest N where DP completion rate falls below 50%:")
    lines.append("")
    for regime in REGIMES:
        cliff = None
        for n in Ns:
            cells = [c for c in cell_rows
                     if c["n"] == n and c["regime"] == regime]
            if not cells:
                continue
            rate = sum(int(c["dp_completed"]) for c in cells) / len(cells)
            if rate < 0.5 and cliff is None:
                cliff = n
                break
        lines.append(
            f"- **{regime}**: " +
            (f"DP collapses at N = {cliff}." if cliff is not None
             else "DP completion stays ≥ 50% across the full N range.")
        )
    lines.append("")

    # Figures
    lines.append("## Figures")
    lines.append("")
    figure_blurbs = [
        ("completion_dense",
         "**Completion rate vs N**, faceted by regime. Each point is the "
         "fraction of (F, seed) replicates the engine finished within the "
         "time budget. The DP curves show the regime-dependent collapse "
         "the v1 sweep only hinted at."),
        ("runtime_dense",
         "**Median runtime vs N**, faceted by regime, log-Y. Only includes "
         "completed runs. Where DP is absent it timed out."),
        ("useful_rate",
         "**Engine usefulness rate vs N**, faceted by regime. \"Useful\" = "
         "completed AND not slower than the other engine. This is the "
         "right metric for picking a default engine."),
        ("winner_by_pool_size_dense",
         "**Outcome by route-pool size**. Per-cell pool size is "
         "total_columns / pricing_calls. Both-timed-out cells are "
         "excluded since they have no pool-size signal."),
        ("speedup_vs_pool",
         "**DP/IP runtime ratio vs route-pool size**, both-completed "
         "cells only. Y > 1 means IP wins. Color-coded by regime."),
    ]
    for key, blurb in figure_blurbs:
        if key in paths:
            rel = paths[key].name
            lines.append(f"### {key.replace('_', ' ').title()}")
            lines.append("")
            lines.append(f"![{key}]({rel})")
            lines.append("")
            lines.append(blurb)
            lines.append("")

    # Hybrid model spec
    lines.append("## Hybrid pricing-engine selector — data set spec")
    lines.append("")
    lines.append(
        "The companion file `cells.csv` is the model-training table. One "
        "row per attempted (n, F, regime, seed) cell; columns split into "
        "**features** (input to the selector) and **outcomes** (training "
        "labels)."
    )
    lines.append("")
    lines.append("### Features (known before pricing call)")
    lines.append("")
    lines.append("Static instance features:")
    lines.append("")
    lines.append(
        "- `n`, `f` — customer / facility counts."
    )
    lines.append(
        "- `bv_factor`, `bf_factor`, `gt_factor` — regime knobs as "
        "continuous numbers; captures vehicle-capacity, facility-capacity, "
        "and time-limit slack."
    )
    lines.append(
        "- `total_demand`, `mean_demand`, `max_demand`, `std_demand` — "
        "demand distribution."
    )
    lines.append(
        "- `vehicle_capacity`, `vehicle_time_limit`, `vehicle_fixed_cost` — "
        "vehicle parameters."
    )
    lines.append(
        "- `mean_cust_nearest_fac_dist`, `max_cust_nearest_fac_dist`, "
        "`mean_cust_cust_dist` — geometric structure."
    )
    lines.append(
        "- `demand_to_capacity_ratio` = total_demand / Σ facility_capacity. "
        "How tightly we're packing facilities."
    )
    lines.append(
        "- `max_singleton_round_trip`, `avg_singleton_round_trip` — "
        "shortest one-customer trips, in cost units."
    )
    lines.append(
        "- `gt_slack` = vehicle_time_limit / max_singleton_round_trip. "
        "Effective per-vehicle time slack — a strong predictor."
    )
    lines.append(
        "- `bv_slack` = vehicle_capacity / max_demand. Effective vehicle-"
        "capacity slack — also a strong predictor."
    )
    lines.append("")
    lines.append("Phase 1 / pricing-state features (observable per call):")
    lines.append("")
    lines.append(
        "- `*_pricing_calls`, `*_total_columns`, `*_phase1_route_columns`, "
        "`*_phase2_pairing_columns` — engine-specific aggregates over the "
        "whole CG run."
    )
    lines.append(
        "- `*_avg_pool_per_call` = total_columns / pricing_calls. The "
        "single best one-shot predictor for which engine is faster (see "
        "`fig_winner_by_pool_size_dense.png`)."
    )
    lines.append(
        "- `avg_pool_per_call` (without engine prefix) — the same proxy "
        "computed from whichever engine finished, intended as the "
        "deployed selector's runtime feature."
    )
    lines.append("")
    lines.append("### Outcomes (training labels)")
    lines.append("")
    lines.append(
        "- `winner` ∈ {`dp`, `ip`, `tie`, `dp_only`, `ip_only`, `both_to`} — "
        "categorical label."
    )
    lines.append(
        "- `dp_useful`, `ip_useful` — binary labels per engine. **The "
        "primary classification target** for a \"is this engine useful "
        "here?\" model."
    )
    lines.append(
        "- `speedup_ip_over_dp` = dp_total_seconds / ip_total_seconds when "
        "both finished (>1 → IP wins, <1 → DP wins). **The regression "
        "target** for a \"how much faster is one over the other?\" model."
    )
    lines.append(
        "- `log_speedup` = log10 of the above. Convenient when fitting "
        "linear models."
    )
    lines.append("")

    # Future model recommendation
    lines.append("### Suggested approach for the future selector")
    lines.append("")
    lines.append(
        "Based on `fig_winner_by_pool_size_dense.png` and the dominance "
        "of `avg_pool_per_call` as a single feature, a useful baseline "
        "is the threshold rule:"
    )
    lines.append("")
    lines.append("```")
    lines.append("if avg_pool_per_call <= POOL_THRESHOLD:")
    lines.append("    use_dp = True       # tiny pool — DP overhead < HiGHS overhead")
    lines.append("else:")
    lines.append("    use_dp = False      # large pool — DP exponential dominates")
    lines.append("```")
    lines.append("")
    lines.append(
        "Tune `POOL_THRESHOLD` on this data; the v1 result suggests ~5-8. "
        "For a learned model, fit a logistic regression / gradient-"
        "boosted tree on the features above with `dp_useful` as the "
        "target. Use `gt_slack`, `bv_slack`, `bv_factor`, `bf_factor`, "
        "and an estimate of `avg_pool_per_call` (which can be predicted "
        "from instance features alone after a few CG iterations) as "
        "inputs. Cross-validate by leaving out one regime at a time."
    )
    lines.append("")

    # Reproducing
    lines.append("## Reproducing")
    lines.append("")
    lines.append("```bash")
    lines.append(
        f"python lrsp_native/scripts/paper_lrsp_dp_vs_ip_dense.py "
        f"--time-limit-seconds {time_budget_s:.0f}"
    )
    lines.append("```")
    lines.append("")
    lines.append(
        "Add `--reanalyze` to rebuild this report and the figures from "
        "the existing `cells.csv` / `raw_results.csv` without re-running "
        "the C solver."
    )

    out.write_text("\n".join(lines), encoding="utf-8")


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------


def attempted_cells():
    for n in N_VALUES:
        f = F_FOR(n)
        for regime in REGIMES:
            for seed in SEEDS:
                yield (n, f, regime, seed)


def main(argv: Optional[list[str]] = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--time-limit-seconds", type=float, default=30.0)
    ap.add_argument("--reanalyze", action="store_true")
    ap.add_argument("--out-dir", type=Path,
                    default=REPO_ROOT / "results" / "lrsp_dp_vs_ip_dense")
    args = ap.parse_args(argv)

    raw_path  = args.out_dir / "raw_results.csv"
    cell_path = args.out_dir / "cells.csv"
    md_path   = args.out_dir / "summary.md"

    if args.reanalyze:
        # We rebuild from cells.csv directly; raw_results.csv is parallel
        # data and not needed for plotting.
        if not cell_path.exists():
            print(f"missing {cell_path}; cannot --reanalyze without prior data")
            return 1
        with cell_path.open("r", newline="") as h:
            cells = list(csv.DictReader(h))
        # Plots want numeric types — DictReader gives us strings, that's OK
        # because make_plots converts where it needs to.
        # (But the loops below assume some int parses — coerce inline below.)
        # Simplest: write a thin coercion.
        for c in cells:
            c["n"] = int(c["n"])
            c["f"] = int(c["f"])
            c["seed"] = int(c["seed"])
            c["dp_completed"] = int(c["dp_completed"])
            c["ip_completed"] = int(c["ip_completed"])
            c["dp_useful"] = int(c["dp_useful"])
            c["ip_useful"] = int(c["ip_useful"])
        paths = make_plots({}, cells, args.out_dir, args.time_limit_seconds)
        write_summary(cells, paths, md_path, args.time_limit_seconds)
        print(f"rewrote {md_path}")
        for v in paths.values():
            print(f"  rewrote {v}")
        return 0

    if not RUN_LRSP.exists():
        print(f"missing {RUN_LRSP}; build first")
        return 1

    print("ensuring instance corpus exists ...")
    have, made = ensure_instances()
    print(f"  existed: {have}   generated: {made}")

    # Resume support: skip cells we already have rows for.
    raw_rows: list[tuple[str, InstanceFeatures, EngineRow]] = []
    already_done: set[tuple[str, str]] = set()
    if raw_path.exists():
        existing = load_raw_csv(raw_path)
        # Reload existing engine rows to seed our running list. We need
        # the InstanceFeatures too, which we can recompute on the fly.
        feature_cache: dict[str, InstanceFeatures] = {}
        for r in existing:
            instance_id = r["instance"]
            feat = feature_cache.get(instance_id)
            if feat is None:
                json_p = (REPO_ROOT / "lrsp_solver" / "instance_db" /
                          "instances" / f"{instance_id}.lrsp.json")
                feat = compute_instance_features(
                    json_p, int(r["n"]), int(r["f"]), r["regime"], int(r["seed"]))
                feature_cache[instance_id] = feat
            er = EngineRow(
                pricing=r["pricing"],
                status=r["status"],
                iterations=int(r["iterations"]),
                pricing_calls=int(r["pricing_calls"]),
                total_columns=int(r["total_columns"]),
                seed_columns=int(r["seed_columns"]),
                phase1_route_columns=int(r["phase1_route_columns"]),
                phase2_pairing_columns=int(r["phase2_pairing_columns"]),
                max_routes_per_column=int(r["max_routes_per_column"]),
                avg_routes_per_pairing=float(r["avg_routes_per_pairing"]),
                reached_optimality=int(r["reached_optimality"]),
                total_seconds=float(r["total_seconds"]),
                master_seconds=float(r["master_seconds"]),
                pricing_seconds=float(r["pricing_seconds"]),
                root_lp=float(r["root_lp_objective"]) if r["root_lp_objective"] else None,
                integer=float(r["integer_objective"]) if r["integer_objective"] else None,
                open_facilities=int(r["open_facilities"]),
            )
            raw_rows.append((instance_id, feat, er))
            already_done.add((instance_id, er.pricing))
    print(f"resuming with {len(raw_rows)} existing rows")

    # Run.
    by_pair: dict[tuple, dict[str, EngineRow]] = {}
    feat_by_id: dict[str, InstanceFeatures] = {}
    for instance_id, feat, er in raw_rows:
        feat_by_id[instance_id] = feat
        by_pair.setdefault(instance_id, {})[er.pricing] = er

    inst_dir = REPO_ROOT / "lrsp_solver" / "instance_db" / "instances"
    cells_total = len(N_VALUES) * len(REGIMES) * len(SEEDS)

    # Per-regime engine cutoff: once all SEEDS seeds at some (n, regime) time
    # out for an engine, skip that engine in that regime for any larger n.
    # Avoids burning hours on cells we already know will time out.
    timeout_counts: dict[tuple[int, str, str], int] = {}
    dead_engines: dict[str, set[str]] = {r: set() for r in REGIMES}

    # Persist cutoff state to disk. Once both engines are dead for a
    # regime, no rows get written for those cells, so reconstruction from
    # raw_results.csv alone is blind. We snapshot dead_engines after every
    # change to a JSON file and load it on resume.
    cutoff_state_path = args.out_dir / "cutoff_state.json"

    def save_cutoff_state() -> None:
        try:
            with cutoff_state_path.open("w") as fh:
                json.dump({r: sorted(dead_engines[r]) for r in REGIMES}, fh)
        except OSError:
            pass

    if cutoff_state_path.exists():
        try:
            with cutoff_state_path.open("r") as fh:
                loaded = json.load(fh)
            for r, engines in loaded.items():
                if r in dead_engines:
                    dead_engines[r] = set(engines)
        except (OSError, ValueError):
            pass

    # Backfill cutoff state from existing rows. Iteration order is
    # deterministic, so we walk cells in order and evolve cutoff state in
    # the same way the run loop does. For each cell up to the latest one
    # that produced any row: if an engine is already cutoff, skip; if it
    # has a row, no timeout; if it has neither, count as a timeout (which
    # may trigger cutoff for larger N).
    ordered_cells = list(attempted_cells())
    cell_index: dict[tuple[int, str, int], int] = {
        (n, regime, seed): i
        for i, (n, _f, regime, seed) in enumerate(ordered_cells)
    }
    engines_per_cell: dict[tuple[int, str, int], set[str]] = {}
    max_attempted_idx = -1
    for instance_id, feat, er in raw_rows:
        key = (feat.n, feat.regime, feat.seed)
        engines_per_cell.setdefault(key, set()).add(er.pricing)
        idx = cell_index.get(key, -1)
        if idx > max_attempted_idx:
            max_attempted_idx = idx
    for i, (n_a, _f_a, regime_a, seed_a) in enumerate(ordered_cells):
        if i > max_attempted_idx:
            break
        engines_done = engines_per_cell.get(
            (n_a, regime_a, seed_a), set())
        for pricing in ("dp", "ip"):
            if pricing in dead_engines[regime_a]:
                continue
            if pricing in engines_done:
                continue
            timeout_counts[(n_a, regime_a, pricing)] = \
                timeout_counts.get((n_a, regime_a, pricing), 0) + 1
            if (timeout_counts[(n_a, regime_a, pricing)] >= len(SEEDS)
                    and pricing not in dead_engines[regime_a]):
                dead_engines[regime_a].add(pricing)
    save_cutoff_state()
    for regime_a in REGIMES:
        if dead_engines[regime_a]:
            print(f"  resume cutoff: {regime_a} regime has dead engines "
                  f"{sorted(dead_engines[regime_a])}")

    cell_idx = 0
    for n, f, regime, seed in attempted_cells():
        cell_idx += 1
        instance_id = f"lrsp_n{n:03d}_f{f:02d}_{regime}_s{seed}"
        json_p = inst_dir / f"{instance_id}.lrsp.json"
        txt_p = inst_dir / f"{instance_id}.txt"
        if not txt_p.exists() or not json_p.exists():
            print(f"  [{cell_idx}/{cells_total}] {instance_id}: MISSING")
            continue
        feat = feat_by_id.get(instance_id) or compute_instance_features(
            json_p, n, f, regime, seed)
        feat_by_id[instance_id] = feat

        line = f"  [{cell_idx}/{cells_total}] N={n} F={f} {regime} s{seed}"
        for pricing in ("dp", "ip"):
            if (instance_id, pricing) in already_done:
                line += f"  {pricing.upper()}=skipped"
                continue
            if pricing in dead_engines[regime]:
                line += f"  {pricing.upper()}=cutoff"
                continue
            er = run_engine(txt_p, pricing, args.time_limit_seconds)
            if er is None:
                line += f"  {pricing.upper()}=TIMEOUT"
                timeout_counts[(n, regime, pricing)] = \
                    timeout_counts.get((n, regime, pricing), 0) + 1
                if timeout_counts[(n, regime, pricing)] >= len(SEEDS):
                    if pricing not in dead_engines[regime]:
                        dead_engines[regime].add(pricing)
                        save_cutoff_state()
                        line += (f" [cutoff:{pricing.upper()} dead in "
                                 f"{regime} from N={n+1}+]")
            else:
                raw_rows.append((instance_id, feat, er))
                by_pair.setdefault(instance_id, {})[pricing] = er
                already_done.add((instance_id, pricing))
                line += (
                    f"  {pricing.upper()}={er.total_seconds:.2f}s"
                    f"(p2={er.phase2_pairing_columns},"
                    f"pool={er.total_columns / max(er.pricing_calls,1):.1f})"
                )
        print(line, flush=True)

        if cell_idx % 5 == 0:
            write_raw_csv(raw_rows, raw_path)

    # Final write-out.
    write_raw_csv(raw_rows, raw_path)

    # Build cells.csv (one row per attempted cell).
    cell_rows: list[dict] = []
    for n, f, regime, seed in attempted_cells():
        instance_id = f"lrsp_n{n:03d}_f{f:02d}_{regime}_s{seed}"
        feat = feat_by_id.get(instance_id)
        if feat is None:
            json_p = inst_dir / f"{instance_id}.lrsp.json"
            if not json_p.exists():
                continue
            feat = compute_instance_features(json_p, n, f, regime, seed)
        engines = by_pair.get(instance_id, {})
        cell_rows.append(make_cell_row(
            instance_id, feat,
            engines.get("dp"), engines.get("ip")))
    write_cells_csv(cell_rows, cell_path)

    # Coerce cell_rows for plotting.
    for c in cell_rows:
        for k in ("n", "f", "seed", "dp_completed", "ip_completed",
                  "dp_useful", "ip_useful"):
            c[k] = int(c[k])

    paths = make_plots({}, cell_rows, args.out_dir, args.time_limit_seconds)
    write_summary(cell_rows, paths, md_path, args.time_limit_seconds)

    print(f"\nwrote {raw_path}")
    print(f"wrote {cell_path}")
    print(f"wrote {md_path}")
    for v in paths.values():
        print(f"  wrote {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
