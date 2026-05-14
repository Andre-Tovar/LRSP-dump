"""
Bundled LRSP instance library.

The corpus lives in `lrsp_solver/instance_db/` as a flat folder of
`*.lrsp.json` files plus a `manifest.json` index. Both the Python solver
(via `read_lrsp_json`) and the C solver (via `write_akca_txt` adapters or
the runner directly on the bundled `.txt` companions) can consume it.

Typical use:

    from lrsp_solver.instance_database import iter_database_instances
    for record, instance in iter_database_instances():
        ...

Every record carries the metadata used to build the instance (size,
regime, seed) so callers can filter without reopening every JSON file.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator, List, Tuple

from .instance import LRSPInstance
from .instance_io import read_lrsp_json


_DB_DIR = Path(__file__).resolve().parent / "instance_db"
_MANIFEST = _DB_DIR / "manifest.json"
_INSTANCES_DIR = _DB_DIR / "instances"


@dataclass(frozen=True, slots=True)
class InstanceRecord:
    instance_id: str             # canonical name (== file stem)
    json_path: Path              # absolute path to .lrsp.json
    txt_path: Path | None        # absolute path to .txt companion, if present
    num_customers: int
    num_facilities: int
    regime: str                  # e.g. "easy" | "moderate" | "tight"
    seed: int
    extras: dict[str, object] = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Manifest I/O — small helpers used by both this module and the builder.
# ---------------------------------------------------------------------------


def database_directory() -> Path:
    return _DB_DIR


def manifest_path() -> Path:
    return _MANIFEST


def instances_directory() -> Path:
    return _INSTANCES_DIR


def load_manifest() -> dict:
    if not _MANIFEST.exists():
        return {"format_version": "lrsp_db.v1", "instances": []}
    with _MANIFEST.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_manifest(records: List[InstanceRecord]) -> None:
    payload = {
        "format_version": "lrsp_db.v1",
        "instances": [
            {
                "instance_id": r.instance_id,
                "json": r.json_path.relative_to(_DB_DIR).as_posix(),
                "txt": (
                    r.txt_path.relative_to(_DB_DIR).as_posix()
                    if r.txt_path else None
                ),
                "num_customers": r.num_customers,
                "num_facilities": r.num_facilities,
                "regime": r.regime,
                "seed": r.seed,
                **{k: v for k, v in r.extras.items()},
            }
            for r in records
        ],
    }
    _MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    with _MANIFEST.open("w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)
        f.write("\n")


# ---------------------------------------------------------------------------
# Public reader API
# ---------------------------------------------------------------------------


def list_database_instances() -> List[InstanceRecord]:
    """All bundled instances, in manifest order."""
    manifest = load_manifest()
    out: List[InstanceRecord] = []
    for entry in manifest.get("instances", []):
        json_rel = entry["json"]
        txt_rel = entry.get("txt")
        out.append(InstanceRecord(
            instance_id=entry["instance_id"],
            json_path=_DB_DIR / json_rel,
            txt_path=(_DB_DIR / txt_rel) if txt_rel else None,
            num_customers=int(entry["num_customers"]),
            num_facilities=int(entry["num_facilities"]),
            regime=str(entry.get("regime", "unknown")),
            seed=int(entry.get("seed", 0)),
            extras={
                k: v for k, v in entry.items()
                if k not in {
                    "instance_id", "json", "txt",
                    "num_customers", "num_facilities", "regime", "seed",
                }
            },
        ))
    return out


def load_database_instance(record: InstanceRecord) -> LRSPInstance:
    return read_lrsp_json(record.json_path)


def iter_database_instances() -> Iterator[Tuple[InstanceRecord, LRSPInstance]]:
    """Yields (record, instance) pairs in manifest order."""
    for record in list_database_instances():
        yield record, load_database_instance(record)


def filter_instances(
    *,
    customers: range | tuple[int, int] | None = None,
    facilities: range | tuple[int, int] | None = None,
    regimes: list[str] | None = None,
    seeds: list[int] | None = None,
) -> List[InstanceRecord]:
    """Convenience filter on the manifest."""

    def _in_bounds(value: int, bounds) -> bool:
        if bounds is None:
            return True
        if isinstance(bounds, range):
            return value in bounds
        lo, hi = bounds
        return lo <= value <= hi

    out: List[InstanceRecord] = []
    for r in list_database_instances():
        if not _in_bounds(r.num_customers, customers):
            continue
        if not _in_bounds(r.num_facilities, facilities):
            continue
        if regimes and r.regime not in regimes:
            continue
        if seeds and r.seed not in seeds:
            continue
        out.append(r)
    return out
