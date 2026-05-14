from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator, Mapping

from .instance import MESPPRCInstance
from .instance_io import (
    canonical_instance_json,
    instance_from_dict,
)

DATABASE_SCHEMA = "mespprc.instance_database.v1"
DATABASE_SCHEMA_VERSION = 1
DATABASE_ROOT = Path(__file__).resolve().parent / "instance_db"
MANIFEST_PATH = DATABASE_ROOT / "manifest.json"


@dataclass(frozen=True, slots=True)
class DatabaseInstanceRecord:
    """One manifest entry for a stored MESPPRC benchmark instance."""

    instance_id: str
    path: str
    checksum_sha256: str
    num_customers: int
    num_arcs: int
    route_count: int
    difficulty: str
    tags: tuple[str, ...]
    generator_config: Mapping[str, Any]
    metrics: Mapping[str, Any]

    @property
    def id(self) -> str:
        return self.instance_id


def list_database_instances(
    *,
    difficulty: str | None = None,
    tag: str | None = None,
    max_customers: int | None = None,
    manifest_path: str | Path | None = None,
) -> list[DatabaseInstanceRecord]:
    """
    Return manifest records for bundled MESPPRC benchmark instances.

    Filters are optional and intentionally simple so tests can select compact
    fixture subsets without knowing the generator sweep details.
    """

    records = _load_manifest_records(_manifest_path(manifest_path))
    if difficulty is not None:
        records = [record for record in records if record.difficulty == difficulty]
    if tag is not None:
        records = [record for record in records if tag in record.tags]
    if max_customers is not None:
        records = [
            record for record in records if record.num_customers <= max_customers
        ]
    return records


def load_database_instance(
    instance_id: str,
    *,
    validate_checksum: bool = True,
    manifest_path: str | Path | None = None,
) -> MESPPRCInstance:
    """Load one bundled instance by manifest id."""

    manifest = _manifest_path(manifest_path)
    records = {record.instance_id: record for record in _load_manifest_records(manifest)}
    try:
        record = records[instance_id]
    except KeyError as exc:
        known = ", ".join(sorted(records))
        raise KeyError(
            f"Unknown MESPPRC database instance {instance_id!r}. Known: {known}"
        ) from exc

    data = _load_instance_data(_resolve_instance_path(manifest.parent, record))
    if validate_checksum:
        observed = _checksum_data(data)
        if observed != record.checksum_sha256:
            raise ValueError(
                f"Checksum mismatch for {record.instance_id}: "
                f"manifest={record.checksum_sha256}, observed={observed}."
            )
    return instance_from_dict(data)


def iter_database_instances(
    *,
    difficulty: str | None = None,
    tag: str | None = None,
    max_customers: int | None = None,
    validate_checksum: bool = True,
    manifest_path: str | Path | None = None,
) -> Iterator[tuple[DatabaseInstanceRecord, MESPPRCInstance]]:
    """Yield `(record, instance)` pairs from the bundled database."""

    manifest = _manifest_path(manifest_path)
    for record in list_database_instances(
        difficulty=difficulty,
        tag=tag,
        max_customers=max_customers,
        manifest_path=manifest,
    ):
        yield record, load_database_instance(
            record.instance_id,
            validate_checksum=validate_checksum,
            manifest_path=manifest,
        )


def validate_database(
    *,
    manifest_path: str | Path | None = None,
) -> list[DatabaseInstanceRecord]:
    """
    Validate manifest shape, checksums, and JSON round-trip for every instance.

    Returns the validated manifest records so callers can also assert database
    size or inspect metadata.
    """

    manifest = _manifest_path(manifest_path)
    records = _load_manifest_records(manifest)
    seen_ids: set[str] = set()
    for record in records:
        if record.instance_id in seen_ids:
            raise ValueError(f"Duplicate instance id in manifest: {record.instance_id}")
        seen_ids.add(record.instance_id)

        data = _load_instance_data(_resolve_instance_path(manifest.parent, record))
        observed = _checksum_data(data)
        if observed != record.checksum_sha256:
            raise ValueError(
                f"Checksum mismatch for {record.instance_id}: "
                f"manifest={record.checksum_sha256}, observed={observed}."
            )
        instance = instance_from_dict(data)
        if len(instance.customers()) != record.num_customers:
            raise ValueError(
                f"Customer count mismatch for {record.instance_id}: "
                f"manifest={record.num_customers}, observed={len(instance.customers())}."
            )
        if len(instance.arcs) != record.num_arcs:
            raise ValueError(
                f"Arc count mismatch for {record.instance_id}: "
                f"manifest={record.num_arcs}, observed={len(instance.arcs)}."
            )
    return records


def _load_manifest_records(path: Path) -> list[DatabaseInstanceRecord]:
    if not path.exists():
        raise FileNotFoundError(path)
    raw = json.loads(path.read_text(encoding="utf-8"))
    if raw.get("schema") != DATABASE_SCHEMA:
        raise ValueError(
            f"Unsupported MESPPRC database schema: {raw.get('schema')!r}."
        )
    if raw.get("schema_version") != DATABASE_SCHEMA_VERSION:
        raise ValueError(
            "Unsupported MESPPRC database schema_version: "
            f"{raw.get('schema_version')!r}."
        )
    entries = raw.get("instances")
    if not isinstance(entries, list):
        raise ValueError("Database manifest field 'instances' must be a list.")
    return [_record_from_dict(entry) for entry in entries]


def _record_from_dict(entry: object) -> DatabaseInstanceRecord:
    if not isinstance(entry, Mapping):
        raise ValueError("Each database manifest entry must be an object.")
    tags = entry.get("tags", [])
    if not isinstance(tags, list):
        raise ValueError("Manifest entry field 'tags' must be a list.")
    generator_config = entry.get("generator_config", {})
    if not isinstance(generator_config, Mapping):
        raise ValueError("Manifest entry field 'generator_config' must be an object.")
    metrics = entry.get("metrics", {})
    if not isinstance(metrics, Mapping):
        raise ValueError("Manifest entry field 'metrics' must be an object.")
    return DatabaseInstanceRecord(
        instance_id=str(entry["id"]),
        path=str(entry["path"]),
        checksum_sha256=str(entry["checksum_sha256"]),
        num_customers=int(entry["num_customers"]),
        num_arcs=int(entry["num_arcs"]),
        route_count=int(entry["route_count"]),
        difficulty=str(entry["difficulty"]),
        tags=tuple(str(tag) for tag in tags),
        generator_config=dict(generator_config),
        metrics=dict(metrics),
    )


def _manifest_path(path: str | Path | None) -> Path:
    return MANIFEST_PATH if path is None else Path(path)


def _resolve_instance_path(database_root: Path, record: DatabaseInstanceRecord) -> Path:
    relative = Path(record.path)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError(
            f"Instance path for {record.instance_id} must be relative to the database."
        )
    resolved = (database_root / relative).resolve()
    root = database_root.resolve()
    if root not in resolved.parents and resolved != root:
        raise ValueError(
            f"Instance path for {record.instance_id} escapes the database root."
        )
    return resolved


def _load_instance_data(path: Path) -> Mapping[str, Any]:
    if not path.exists():
        raise FileNotFoundError(path)
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, Mapping):
        raise ValueError(f"Instance JSON at {path} must contain an object.")
    return data


def _checksum_data(data: Mapping[str, Any]) -> str:
    payload = canonical_instance_json(data).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()
