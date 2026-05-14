"""
Run the LRSP_PRICING_HYBRID engine on every cell of the dense corpus, and
compare its total runtime / completion against the DP and IP results
already captured in `cells.csv`.

This is the validation that the hybrid selector actually works — i.e.
that it correctly picks DP on cells where DP wins and IP on cells where
IP wins, and therefore has a runtime distribution that's at least as
good as the better of the two on each cell.

Reads:    results/lrsp_dp_vs_ip_dense/cells.csv
Writes:   results/lrsp_dp_vs_ip_dense/hybrid_validation.csv
          results/lrsp_dp_vs_ip_dense/hybrid_validation.md
          results/lrsp_dp_vs_ip_dense/fig_hybrid_vs_engines.png
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import statistics
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
RUN_LRSP = REPO_ROOT / "lrsp_native" / "build" / "bin" / "run_lrsp.exe"
INST_DIR = REPO_ROOT / "lrsp_solver" / "instance_db" / "instances"

CELLS_CSV = REPO_ROOT / "results" / "lrsp_dp_vs_ip_dense" / "cells.csv"
OUT_DIR = REPO_ROOT / "results" / "lrsp_dp_vs_ip_dense"


_RE = {
    "status": re.compile(r"^status:\s+(\S+)"),
    "iters": re.compile(r"^iterations:\s+(\d+)"),
    "calls": re.compile(r"^pricing_calls:\s+(\d+)"),
    "total_cols": re.compile(r"^total_columns:\s+(\d+)"),
    "phase2": re.compile(r"^phase2_pairing_columns:\s+(\d+)"),
    "total": re.compile(r"^total_runtime:\s+([\d.]+)s"),
    "master": re.compile(r"^master_runtime:\s+([\d.]+)s"),
    "pricing": re.compile(r"^pricing_runtime:\s+([\d.]+)s"),
    "root_lp": re.compile(r"^root_lp_objective:\s+([\-\d.eE+]+)"),
    "integer": re.compile(r"^integer_objective:\s+([\-\d.eE+]+)"),
}


def parse_run(stdout: str) -> dict:
    out: dict = {}
    for line in stdout.splitlines():
        for k, rx in _RE.items():
            m = rx.match(line.strip())
            if m and k not in out:
                out[k] = m.group(1)
    return out


def run_hybrid(instance_path: Path, time_limit: float) -> dict | None:
    cmd = [str(RUN_LRSP),
           "--instance", str(instance_path),
           "--pricing", "hybrid",
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
    return {
        "completed": 1,
        "status": f.get("status", "?"),
        "iterations": int(f.get("iters", 0)),
        "pricing_calls": int(f.get("calls", 0)),
        "total_columns": int(f.get("total_cols", 0)),
        "phase2_pairing_columns": int(f.get("phase2", 0)),
        "total_seconds": float(f["total"]),
        "master_seconds": float(f.get("master", 0.0)),
        "pricing_seconds": float(f.get("pricing", 0.0)),
        "root_lp": float(f["root_lp"]) if "root_lp" in f else None,
        "integer": float(f["integer"]) if "integer" in f else None,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--time-limit-seconds", type=float, default=30.0)
    ap.add_argument("--cells", type=Path, default=CELLS_CSV)
    ap.add_argument("--out-dir", type=Path, default=OUT_DIR)
    args = ap.parse_args()

    if not RUN_LRSP.exists():
        print(f"missing {RUN_LRSP}; build first")
        return 1
    if not args.cells.exists():
        print(f"missing {args.cells}; run paper_lrsp_dp_vs_ip_dense.py first")
        return 1

    with args.cells.open("r", newline="") as h:
        cells = list(csv.DictReader(h))
    print(f"loaded {len(cells)} cells")

    # Resume support.
    out_csv = args.out_dir / "hybrid_validation.csv"
    done: dict[str, dict] = {}
    if out_csv.exists():
        with out_csv.open("r", newline="") as h:
            for r in csv.DictReader(h):
                done[r["instance"]] = r
        print(f"resuming with {len(done)} hybrid rows")

    # Per-regime hybrid cutoff: once all 3 seeds at some (N, regime) time
    # out, skip hybrid in that regime for any larger N. Persisted to
    # hybrid_cutoff_state.json so resume keeps the cutoff.
    SEEDS_PER_CELL = 3
    cutoff_path = args.out_dir / "hybrid_cutoff_state.json"
    dead_regimes: set[str] = set()
    if cutoff_path.exists():
        try:
            with cutoff_path.open("r") as fh:
                dead_regimes = set(json.load(fh))
        except (OSError, ValueError):
            pass
    timeout_counts: dict[tuple[int, str], int] = defaultdict(int)

    # Backfill from any TIMEOUT rows already in hybrid_validation.csv,
    # plus from cells that should have been seen but have no row at all.
    # We iterate cells.csv in order and treat "no row" cells before the
    # furthest reached cell as timeouts.
    max_seen_idx = -1
    cells_by_id: dict[str, dict] = {c["instance"]: c for c in cells}
    for i, c in enumerate(cells):
        inst = c["instance"]
        if inst in done:
            if i > max_seen_idx:
                max_seen_idx = i
            if int(done[inst].get("hybrid_completed", 0)) == 0:
                key = (int(c["n"]), c["regime"])
                timeout_counts[key] += 1
                if timeout_counts[key] >= SEEDS_PER_CELL:
                    dead_regimes.add(c["regime"])
    for i, c in enumerate(cells):
        if i > max_seen_idx:
            break
        if c["instance"] not in done:
            key = (int(c["n"]), c["regime"])
            if c["regime"] in dead_regimes:
                continue
            timeout_counts[key] += 1
            if timeout_counts[key] >= SEEDS_PER_CELL:
                dead_regimes.add(c["regime"])

    def save_cutoff_state() -> None:
        try:
            args.out_dir.mkdir(parents=True, exist_ok=True)
            with cutoff_path.open("w") as fh:
                json.dump(sorted(dead_regimes), fh)
        except OSError:
            pass

    save_cutoff_state()
    if dead_regimes:
        print(f"  resume cutoff: hybrid dead in regimes {sorted(dead_regimes)}")

    rows: list[dict] = []
    for idx, c in enumerate(cells, 1):
        instance_id = c["instance"]
        txt_p = INST_DIR / f"{instance_id}.txt"
        if not txt_p.exists():
            continue

        line = (f"  [{idx}/{len(cells)}] {instance_id} "
                f"(N={c['n']} F={c['f']} {c['regime']} s{c['seed']})")

        if instance_id in done:
            line += "  HYBRID=skipped"
            rows.append(done[instance_id])
            print(line, flush=True)
            continue

        if c["regime"] in dead_regimes:
            line += "  HYBRID=cutoff"
            print(line, flush=True)
            continue

        h = run_hybrid(txt_p, args.time_limit_seconds)
        if h is None:
            row = {
                "instance": instance_id,
                "n": c["n"], "f": c["f"], "regime": c["regime"], "seed": c["seed"],
                "hybrid_completed": 0,
                "hybrid_total_seconds": "",
                "hybrid_master_seconds": "",
                "hybrid_pricing_seconds": "",
                "hybrid_iterations": "",
                "hybrid_pricing_calls": "",
                "hybrid_total_columns": "",
                "hybrid_phase2_pairing_columns": "",
                "hybrid_root_lp": "",
                "hybrid_integer": "",
            }
            line += "  HYBRID=TIMEOUT"
            key = (int(c["n"]), c["regime"])
            timeout_counts[key] += 1
            if (timeout_counts[key] >= SEEDS_PER_CELL
                    and c["regime"] not in dead_regimes):
                dead_regimes.add(c["regime"])
                save_cutoff_state()
                line += (f" [cutoff:HYBRID dead in "
                         f"{c['regime']} from N={int(c['n'])+1}+]")
        else:
            row = {
                "instance": instance_id,
                "n": c["n"], "f": c["f"], "regime": c["regime"], "seed": c["seed"],
                "hybrid_completed": 1,
                "hybrid_total_seconds": f"{h['total_seconds']:.6f}",
                "hybrid_master_seconds": f"{h['master_seconds']:.6f}",
                "hybrid_pricing_seconds": f"{h['pricing_seconds']:.6f}",
                "hybrid_iterations": h["iterations"],
                "hybrid_pricing_calls": h["pricing_calls"],
                "hybrid_total_columns": h["total_columns"],
                "hybrid_phase2_pairing_columns": h["phase2_pairing_columns"],
                "hybrid_root_lp": "" if h["root_lp"] is None else f"{h['root_lp']:.6f}",
                "hybrid_integer": "" if h["integer"] is None else f"{h['integer']:.6f}",
            }
            line += f"  HYBRID={h['total_seconds']:.2f}s"
        rows.append(row)
        print(line, flush=True)

        if idx % 5 == 0:
            args.out_dir.mkdir(parents=True, exist_ok=True)
            with out_csv.open("w", newline="") as f_out:
                w = csv.DictWriter(f_out, fieldnames=list(rows[0].keys()))
                w.writeheader()
                w.writerows(rows)

    # Final write.
    args.out_dir.mkdir(parents=True, exist_ok=True)
    with out_csv.open("w", newline="") as f_out:
        w = csv.DictWriter(f_out, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"\nwrote {out_csv}")

    # ---- Compare hybrid to DP / IP from cells.csv ----
    by_inst = {c["instance"]: c for c in cells}
    n_total = len(rows)
    n_hyb_done = sum(int(r["hybrid_completed"]) for r in rows)
    n_dp_done = sum(int(c["dp_completed"]) for c in cells)
    n_ip_done = sum(int(c["ip_completed"]) for c in cells)

    # For each cell, compute "ideal" runtime = min(DP, IP) when both
    # finished, else whichever finished. Compare hybrid to that.
    ideal_runtimes = []
    hybrid_runtimes = []
    hybrid_better_count = 0
    hybrid_match_count = 0
    hybrid_worse_count = 0
    for r in rows:
        if not int(r["hybrid_completed"]):
            continue
        c = by_inst[r["instance"]]
        h_t = float(r["hybrid_total_seconds"])
        candidates = []
        if int(c["dp_completed"]):
            candidates.append(("dp", float(c["dp_total_seconds"])))
        if int(c["ip_completed"]):
            candidates.append(("ip", float(c["ip_total_seconds"])))
        if not candidates:
            continue
        ideal = min(t for _, t in candidates)
        ideal_runtimes.append(ideal)
        hybrid_runtimes.append(h_t)
        ratio = h_t / max(ideal, 1e-12)
        if ratio < 0.95:
            hybrid_better_count += 1
        elif ratio < 1.10:
            hybrid_match_count += 1
        else:
            hybrid_worse_count += 1

    md = args.out_dir / "hybrid_validation.md"
    lines: list[str] = []
    lines.append("# Hybrid pricing-engine — validation")
    lines.append("")
    lines.append(
        "Compares the in-solver `LRSP_PRICING_HYBRID` mode (threshold rule "
        f"with T={7.29}) against the DP-only and IP-only data from "
        "`cells.csv`. Each cell uses the same instance file and the same "
        f"{args.time_limit_seconds:.0f}-second budget."
    )
    lines.append("")
    lines.append("## Completion")
    lines.append("")
    lines.append(f"- DP completed: {n_dp_done} / {n_total} ({n_dp_done/n_total*100:.1f}%)")
    lines.append(f"- IP completed: {n_ip_done} / {n_total} ({n_ip_done/n_total*100:.1f}%)")
    lines.append(f"- **Hybrid completed: {n_hyb_done} / {n_total} ({n_hyb_done/n_total*100:.1f}%)**")
    lines.append("")

    if hybrid_runtimes:
        lines.append("## Runtime vs the per-cell oracle")
        lines.append("")
        lines.append(
            "Per cell, define \"oracle\" = min(DP runtime, IP runtime) over "
            "engines that completed. The closer hybrid is to the oracle, "
            "the better the selector is doing."
        )
        lines.append("")
        lines.append(f"- Cells with both hybrid AND at least one of DP/IP completed: {len(hybrid_runtimes)}")
        ratios = [h / max(o, 1e-12) for h, o in zip(hybrid_runtimes, ideal_runtimes)]
        lines.append(
            f"- Hybrid / oracle ratio: median {statistics.median(ratios):.2f}×, "
            f"mean {statistics.fmean(ratios):.2f}×, max {max(ratios):.2f}×."
        )
        lines.append(
            f"- Hybrid better than oracle (impossible if selector is "
            f"deterministic): {hybrid_better_count}"
        )
        lines.append(
            f"- Hybrid within 10% of oracle: {hybrid_match_count}"
        )
        lines.append(
            f"- Hybrid more than 10% slower than oracle: {hybrid_worse_count}"
        )
        lines.append("")

    # Optional plot
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        # Stacked: completion rates by engine
        labels = ["DP", "IP", "Hybrid"]
        completed = [
            sum(int(by_inst[r["instance"]]["dp_completed"]) for r in rows),
            sum(int(by_inst[r["instance"]]["ip_completed"]) for r in rows),
            sum(int(r["hybrid_completed"]) for r in rows),
        ]
        fig, ax = plt.subplots(figsize=(8, 4.5))
        ax.bar(labels, [100 * x / max(n_total, 1) for x in completed],
               color=["C0", "C1", "C2"])
        for i, v in enumerate(completed):
            ax.text(i, 100 * v / max(n_total, 1) + 1, f"{v} cells",
                    ha="center", fontsize=10)
        ax.set_ylim(0, 110)
        ax.set_ylabel("% of cells completed")
        ax.set_title(f"Completion rate per engine, {n_total} cells, "
                     f"{args.time_limit_seconds:.0f}s budget")
        fig.tight_layout()
        png = args.out_dir / "fig_hybrid_vs_engines.png"
        fig.savefig(png, dpi=200, bbox_inches="tight")
        plt.close(fig)
        lines.append("## Figure")
        lines.append("")
        lines.append(f"![completion]({png.name})")
        lines.append("")
    except Exception as exc:
        print(f"plot skipped: {exc}")

    md.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {md}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
