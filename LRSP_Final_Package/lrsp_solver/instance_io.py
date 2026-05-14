"""
Serialization for LRSPInstance.

Two formats:

- JSON (`*.lrsp.json`) — canonical storage. Lossless, includes everything
  the LRSPInstance dataclass carries plus a `format_version` field.
- Akca `.txt` — the format the C runner consumes. Read support already
  lives in `lrsp_solver/instance.py::load_lrsp_instance`; this module adds
  the writer.

Round-trip:

  instance       → write_lrsp_json     →  *.lrsp.json
  *.lrsp.json    → read_lrsp_json      →  instance
  instance       → write_akca_txt      →  *.txt
  *.txt          → load_lrsp_instance  →  instance   (lrsp_solver/instance.py)

The Akca format does not preserve facility-specific opening costs across
unequal facilities (it uses a single `facility_capacity` scalar shared by
every facility row). The generator emits uniform capacity by construction,
so this is lossless for instances we generate; loading back through the
Akca path will work bit-for-bit. Heterogeneous external instances should
go through the JSON route.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from .instance import Customer, Facility, LRSPInstance


JSON_FORMAT_VERSION = "lrsp.v1"


# ---------------------------------------------------------------------------
# JSON
# ---------------------------------------------------------------------------


def instance_to_dict(instance: LRSPInstance) -> dict[str, Any]:
    return {
        "format_version": JSON_FORMAT_VERSION,
        "name": instance.name,
        "customers": [
            {"id": c.id, "x": c.x, "y": c.y, "demand": c.demand}
            for c in instance.customers
        ],
        "facilities": [
            {
                "id": f.id, "x": f.x, "y": f.y,
                "opening_cost": f.opening_cost,
                "capacity": f.capacity,
            }
            for f in instance.facilities
        ],
        "vehicle_capacity": instance.vehicle_capacity,
        "vehicle_fixed_cost": instance.vehicle_fixed_cost,
        "vehicle_operating_cost": instance.vehicle_operating_cost,
        "vehicle_time_limit": instance.vehicle_time_limit,
        "notes": list(instance.notes),
    }


def instance_from_dict(d: dict[str, Any]) -> LRSPInstance:
    if d.get("format_version") != JSON_FORMAT_VERSION:
        # Forward compatibility: accept older but warn? For now, accept any.
        pass
    return LRSPInstance(
        name=str(d["name"]),
        customers=[
            Customer(
                id=int(c["id"]),
                x=float(c["x"]),
                y=float(c["y"]),
                demand=float(c["demand"]),
            )
            for c in d["customers"]
        ],
        facilities=[
            Facility(
                id=int(f["id"]),
                x=float(f["x"]),
                y=float(f["y"]),
                opening_cost=float(f["opening_cost"]),
                capacity=float(f["capacity"]),
            )
            for f in d["facilities"]
        ],
        vehicle_capacity=float(d["vehicle_capacity"]),
        vehicle_fixed_cost=float(d["vehicle_fixed_cost"]),
        vehicle_operating_cost=float(d["vehicle_operating_cost"]),
        vehicle_time_limit=(
            float(d["vehicle_time_limit"])
            if d.get("vehicle_time_limit") is not None
            else None
        ),
        notes=list(d.get("notes", [])),
    )


def write_lrsp_json(instance: LRSPInstance, path: str | Path, *,
                    indent: int = 2) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(instance_to_dict(instance), f, indent=indent)
        f.write("\n")


def read_lrsp_json(path: str | Path) -> LRSPInstance:
    path = Path(path)
    with path.open("r", encoding="utf-8") as f:
        return instance_from_dict(json.load(f))


# ---------------------------------------------------------------------------
# Akca .txt writer
# ---------------------------------------------------------------------------


def write_akca_txt(
    instance: LRSPInstance,
    path: str | Path,
    *,
    num_vehicles_per_facility: int = 50,
    customer_service_time: float = 0.0,
) -> None:
    """
    Emit `instance` in the Akca .txt format consumed by:

      - `lrsp_solver/instance.py::load_lrsp_instance` (Python)
      - `lrsp_native/src/instance_io.c::lrsp_instance_load_akca_txt` (C)

    Layout (whitespace-separated):

        line 1: <num_facilities> <num_customers>
        line 2: <opening_cost_1> ... <opening_cost_F>
        line 3: <vehicle_fixed_cost> <num_vehicles_per_facility>
        line 4: <vehicle_capacity> <facility_capacity> <vehicle_time_limit>
        next num_customers rows: <id> <x> <y> <service_time> <demand>
        next num_facilities rows: <id> <x> <y> 0 0

    Akca's format carries a single `facility_capacity` scalar (shared by every
    facility row). We write the first facility's capacity and assume the
    instance has uniform facility capacities — true for everything the
    generator produces. For heterogeneous-capacity instances the JSON path
    is lossless and should be preferred.

    `customer_service_time` is included as the 4th column on customer rows.
    Our solver ignores it (Phase 1 only models capacity + duty time), but
    the field is part of the format and must be present.
    """

    if instance.vehicle_time_limit is None or instance.vehicle_time_limit <= 0:
        raise ValueError(
            "Akca .txt requires a positive vehicle_time_limit; this instance "
            f"has {instance.vehicle_time_limit!r}. Use write_lrsp_json for "
            "LRP-style (no scheduling) instances."
        )

    F = len(instance.facilities)
    C = len(instance.customers)
    if F == 0 or C == 0:
        raise ValueError("Akca .txt requires at least one facility and one customer.")

    capacities = {f.capacity for f in instance.facilities}
    if len(capacities) != 1:
        raise ValueError(
            "Akca .txt requires uniform facility capacity; this instance has "
            f"{len(capacities)} distinct capacities. Use write_lrsp_json instead."
        )
    facility_capacity = next(iter(capacities))

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as out:
        out.write(f"{F} {C}\n")
        out.write(" ".join(_fmt(f.opening_cost) for f in instance.facilities) + "\n")
        out.write(
            f"{_fmt(instance.vehicle_fixed_cost)} {int(num_vehicles_per_facility)}\n"
        )
        out.write(
            f"{_fmt(instance.vehicle_capacity)} {_fmt(facility_capacity)} "
            f"{_fmt(instance.vehicle_time_limit)}\n"
        )
        for c in instance.customers:
            out.write(
                f"{c.id} {_fmt(c.x)} {_fmt(c.y)} "
                f"{_fmt(customer_service_time)} {_fmt(c.demand)}\n"
            )
        for f in instance.facilities:
            out.write(f"{f.id} {_fmt(f.x)} {_fmt(f.y)} 0 0\n")


def _fmt(value: float) -> str:
    """Format a number for Akca .txt. Integer values get the bare integer
    spelling so the file matches the human-curated Akca corpus character for
    character; everything else uses `%.17g`, which is the round-trip
    precision for IEEE-754 doubles (and matches what `repr(float)` does)."""
    if isinstance(value, int) or float(value).is_integer():
        return str(int(value))
    return f"{value:.17g}"
