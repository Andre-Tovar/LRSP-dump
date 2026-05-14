from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Mapping

from .instance import Arc, MESPPRCInstance, Node, NodeType

INSTANCE_SCHEMA = "mespprc.instance.v1"
INSTANCE_SCHEMA_VERSION = 1


def instance_to_dict(instance: MESPPRCInstance) -> dict[str, Any]:
    """
    Convert an in-memory MESPPRC instance to a deterministic JSON-ready dict.

    The snapshot intentionally stores the solver-ready graph, not just a
    generator seed, so future tests are insulated from generator changes.
    """

    instance.validate()

    return {
        "schema": INSTANCE_SCHEMA,
        "schema_version": INSTANCE_SCHEMA_VERSION,
        "local_limits": [float(value) for value in instance.local_limits],
        "global_limits": [float(value) for value in instance.global_limits],
        "required_customers": [int(value) for value in instance.required_customers()],
        "exact_once_service": bool(instance.exact_once_service),
        "nodes": [
            {
                "id": int(node.id),
                "node_type": NodeType(node.node_type).name,
            }
            for node in sorted(instance.nodes.values(), key=lambda node: node.id)
        ],
        "arcs": [
            {
                "tail": int(arc.tail),
                "head": int(arc.head),
                "cost": float(arc.cost),
                "local_res": [float(value) for value in arc.local_res],
                "global_res": [float(value) for value in arc.global_res],
            }
            for arc in sorted(
                instance.arcs.values(),
                key=lambda arc: (arc.tail, arc.head),
            )
        ],
    }


def instance_from_dict(data: Mapping[str, Any]) -> MESPPRCInstance:
    """Rebuild and validate a MESPPRC instance from `instance_to_dict` output."""

    if data.get("schema") != INSTANCE_SCHEMA:
        raise ValueError(
            f"Unsupported MESPPRC instance schema: {data.get('schema')!r}."
        )
    if data.get("schema_version") != INSTANCE_SCHEMA_VERSION:
        raise ValueError(
            "Unsupported MESPPRC instance schema_version: "
            f"{data.get('schema_version')!r}."
        )

    instance = MESPPRCInstance(
        local_limits=_float_list(data.get("local_limits", []), "local_limits"),
        global_limits=_float_list(data.get("global_limits", []), "global_limits"),
        required_customers=_int_list(
            data.get("required_customers", []),
            "required_customers",
        ),
        exact_once_service=bool(data.get("exact_once_service", True)),
    )

    for raw_node in _list_value(data, "nodes"):
        if not isinstance(raw_node, Mapping):
            raise ValueError("Each node entry must be an object.")
        instance.add_node(
            Node(
                id=int(raw_node["id"]),
                node_type=_node_type(raw_node["node_type"]),
            )
        )

    for raw_arc in _list_value(data, "arcs"):
        if not isinstance(raw_arc, Mapping):
            raise ValueError("Each arc entry must be an object.")
        instance.add_arc(
            Arc(
                tail=int(raw_arc["tail"]),
                head=int(raw_arc["head"]),
                cost=float(raw_arc["cost"]),
                local_res=_float_list(raw_arc.get("local_res", []), "local_res"),
                global_res=_float_list(raw_arc.get("global_res", []), "global_res"),
            )
        )

    instance.validate()
    return instance


def write_instance_json(
    path: str | Path,
    instance: MESPPRCInstance,
) -> None:
    """Write a deterministic, pretty JSON snapshot for a solver-ready instance."""

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(instance_to_dict(instance), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def load_instance_json(path: str | Path) -> MESPPRCInstance:
    """Load a solver-ready MESPPRC instance from a JSON snapshot file."""

    path = Path(path)
    return instance_from_dict(json.loads(path.read_text(encoding="utf-8")))


def canonical_instance_json(data: Mapping[str, Any]) -> str:
    """Return the canonical JSON representation used for database checksums."""

    return json.dumps(data, ensure_ascii=True, separators=(",", ":"), sort_keys=True)


def _list_value(data: Mapping[str, Any], key: str) -> list[Any]:
    value = data.get(key)
    if not isinstance(value, list):
        raise ValueError(f"Instance field {key!r} must be a list.")
    return value


def _float_list(value: object, field_name: str) -> list[float]:
    if not isinstance(value, list):
        raise ValueError(f"Instance field {field_name!r} must be a list.")
    return [float(item) for item in value]


def _int_list(value: object, field_name: str) -> list[int]:
    if not isinstance(value, list):
        raise ValueError(f"Instance field {field_name!r} must be a list.")
    return [int(item) for item in value]


def _node_type(value: object) -> NodeType:
    if isinstance(value, str):
        try:
            return NodeType[value]
        except KeyError as exc:
            raise ValueError(f"Unknown node_type {value!r}.") from exc
    return NodeType(int(value))
