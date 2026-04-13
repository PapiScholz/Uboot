"""Snapshot persistence, history listing, and change tracking."""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Sequence

from .scanner import Entry, ScanResult


@dataclass
class Snapshot:
    """A recorded state of the system at a point in time."""

    timestamp: str
    entries: List[Entry] = field(default_factory=list)
    entry_count: int = 0
    metadata: Dict = field(default_factory=dict)


@dataclass
class SnapshotSummary:
    """Compact metadata used for timeline/history selection."""

    timestamp: str
    label: str
    entry_count: int
    error_count: int
    path: Path


@dataclass
class SnapshotEntryChange:
    """A before/after change for the same entry identity."""

    entry_before: Entry
    entry_after: Entry
    changed_fields: List[str] = field(default_factory=list)


@dataclass
class SnapshotDiff:
    """Difference between two snapshots."""

    timestamp_current: str
    timestamp_previous: str
    new_entries: List[Entry] = field(default_factory=list)
    removed_entries: List[Entry] = field(default_factory=list)
    changed_entries: List[SnapshotEntryChange] = field(default_factory=list)
    unchanged_entry_count: int = 0


class SnapshotManager:
    """Manage snapshot persistence and change detection."""

    def __init__(self, snapshot_dir: Optional[Path] = None):
        self.snapshot_dir = Path(snapshot_dir or "snapshots")
        self.snapshot_dir.mkdir(exist_ok=True, parents=True)

    def save(self, scan_result: ScanResult, label: str = "") -> Snapshot:
        """Save a scan result as a snapshot."""
        timestamp = datetime.now(timezone.utc).isoformat()
        snapshot = Snapshot(
            timestamp=timestamp,
            entries=scan_result.entries,
            entry_count=len(scan_result.entries),
            metadata={
                "label": label,
                "schema_version": scan_result.schema_version,
                "error_count": len(scan_result.errors),
            },
        )

        filename = (
            f"snapshot_{timestamp.replace(':', '-')}_{label}.json"
            if label
            else f"snapshot_{timestamp.replace(':', '-')}.json"
        )
        filepath = self.snapshot_dir / filename
        filepath.write_text(
            json.dumps(self._snapshot_to_dict(snapshot), indent=2),
            encoding="utf-8",
        )
        return snapshot

    def list_snapshots(self) -> List[SnapshotSummary]:
        """List snapshots newest first with timeline-friendly metadata."""
        summaries: List[SnapshotSummary] = []
        for path in sorted(self.snapshot_dir.glob("snapshot_*.json"), reverse=True):
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except (json.JSONDecodeError, OSError):
                continue

            metadata = data.get("metadata", {})
            summaries.append(
                SnapshotSummary(
                    timestamp=str(data.get("timestamp", "")),
                    label=str(metadata.get("label", "")),
                    entry_count=int(data.get("entry_count", 0)),
                    error_count=int(metadata.get("error_count", 0)),
                    path=path,
                )
            )
        return summaries

    def load_latest(self) -> Optional[Snapshot]:
        """Load the most recent snapshot, if any."""
        snapshots = self.list_snapshots()
        if not snapshots:
            return None
        return self.load_snapshot(snapshots[0].path)

    def load_snapshot(self, path: Path) -> Snapshot:
        """Load a snapshot from disk."""
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        return self._dict_to_snapshot(data)

    def diff(
        self,
        current_snapshot: Snapshot,
        previous_snapshot: Optional[Snapshot] = None,
    ) -> SnapshotDiff:
        """Compare two snapshots and identify new, removed, and changed entries."""
        if previous_snapshot is None:
            snapshots = self.list_snapshots()
            if len(snapshots) < 2:
                return SnapshotDiff(
                    timestamp_current=current_snapshot.timestamp,
                    timestamp_previous="none",
                    new_entries=current_snapshot.entries,
                    unchanged_entry_count=0,
                )
            previous_snapshot = self.load_snapshot(snapshots[1].path)

        current_map = {self._entry_identity_key(entry): entry for entry in current_snapshot.entries}
        previous_map = {self._entry_identity_key(entry): entry for entry in previous_snapshot.entries}

        current_keys = set(current_map.keys())
        previous_keys = set(previous_map.keys())

        new_keys = current_keys - previous_keys
        removed_keys = previous_keys - current_keys
        shared_keys = current_keys & previous_keys

        diff = SnapshotDiff(
            timestamp_current=current_snapshot.timestamp,
            timestamp_previous=previous_snapshot.timestamp,
            new_entries=[current_map[key] for key in sorted(new_keys)],
            removed_entries=[previous_map[key] for key in sorted(removed_keys)],
        )

        unchanged_count = 0
        for key in sorted(shared_keys):
            entry_current = current_map[key]
            entry_previous = previous_map[key]
            changed_fields = self._changed_fields(entry_previous, entry_current)
            if changed_fields:
                diff.changed_entries.append(
                    SnapshotEntryChange(
                        entry_before=entry_previous,
                        entry_after=entry_current,
                        changed_fields=changed_fields,
                    )
                )
            else:
                unchanged_count += 1

        diff.unchanged_entry_count = unchanged_count
        return diff

    @staticmethod
    def _entry_identity_key(entry: Entry) -> str:
        """Generate a stable identity key for diffing snapshots."""
        if entry.entry_id:
            return entry.entry_id
        return "|".join([entry.source, entry.name, entry.image_path or "", entry.status])

    @staticmethod
    def _changed_fields(entry_before: Entry, entry_after: Entry) -> List[str]:
        """Return the list of meaningful changed fields for the same identity."""
        changed: List[str] = []
        if entry_before.name != entry_after.name:
            changed.append("name")
        if entry_before.command != entry_after.command:
            changed.append("command")
        if entry_before.image_path != entry_after.image_path:
            changed.append("image_path")
        if entry_before.source != entry_after.source:
            changed.append("source")
        if entry_before.status != entry_after.status:
            changed.append("status")
        if entry_before.metadata != entry_after.metadata:
            changed.append("metadata")
        return changed

    @staticmethod
    def _snapshot_to_dict(snapshot: Snapshot) -> dict:
        """Convert a Snapshot into a JSON-serializable structure."""
        return {
            "timestamp": snapshot.timestamp,
            "entry_count": snapshot.entry_count,
            "metadata": snapshot.metadata,
            "entries": [
                {
                    "entry_id": entry.entry_id,
                    "name": entry.name,
                    "command": entry.command,
                    "image_path": entry.image_path,
                    "source": entry.source,
                    "status": entry.status,
                    "metadata": entry.metadata,
                }
                for entry in snapshot.entries
            ],
        }

    @staticmethod
    def _dict_to_snapshot(data: dict) -> Snapshot:
        """Convert a JSON structure into a Snapshot."""
        entries = [
            Entry(
                entry_id=entry.get("entry_id", ""),
                name=entry.get("name", ""),
                command=entry.get("command", ""),
                image_path=entry.get("image_path", ""),
                source=entry.get("source", ""),
                status=entry.get("status", "unknown"),
                metadata=entry.get("metadata", {}),
            )
            for entry in data.get("entries", [])
        ]

        return Snapshot(
            timestamp=data.get("timestamp", ""),
            entries=entries,
            entry_count=data.get("entry_count", len(entries)),
            metadata=data.get("metadata", {}),
        )
