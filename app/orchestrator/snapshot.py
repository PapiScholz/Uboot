"""Snapshot: persist scans and track changes between sessions."""
import json
from pathlib import Path
from typing import Dict, List, Optional, Set
from dataclasses import dataclass, field
from datetime import datetime

from .scanner import Entry, ScanResult


@dataclass
class Snapshot:
    """A recorded state of the system at a point in time."""
    timestamp: str
    entries: List[Entry] = field(default_factory=list)
    entry_count: int = 0
    metadata: Dict = field(default_factory=dict)


@dataclass
class SnapshotDiff:
    """Difference between two snapshots."""
    timestamp_current: str
    timestamp_previous: str
    new_entries: List[Entry] = field(default_factory=list)
    removed_entries: List[Entry] = field(default_factory=list)
    changed_entries: List[Entry] = field(default_factory=list)
    unchanged_entry_count: int = 0


class SnapshotManager:
    """Manages snapshot persistence and change detection."""

    def __init__(self, snapshot_dir: Optional[Path] = None):
        """
        Initialize snapshot manager.
        
        Args:
            snapshot_dir: Directory to store snapshots. If None, uses ./snapshots
        """
        self.snapshot_dir = Path(snapshot_dir or "snapshots")
        self.snapshot_dir.mkdir(exist_ok=True, parents=True)

    def save(self, scan_result: ScanResult, label: str = "") -> Snapshot:
        """
        Save a scan result as a snapshot.
        
        Args:
            scan_result: Result from Scanner.scan()
            label: Optional label for the snapshot (e.g., "before-remediation")
            
        Returns:
            Snapshot object that was saved
        """
        timestamp = datetime.utcnow().isoformat()
        snapshot = Snapshot(
            timestamp=timestamp,
            entries=scan_result.entries,
            entry_count=len(scan_result.entries),
            metadata={
                "label": label,
                "schema_version": scan_result.schema_version,
                "error_count": len(scan_result.errors)
            }
        )

        # Save to file
        filename = f"snapshot_{timestamp.replace(':', '-')}_{label}.json" if label else f"snapshot_{timestamp.replace(':', '-')}.json"
        filepath = self.snapshot_dir / filename

        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(self._snapshot_to_dict(snapshot), f, indent=2)

        return snapshot

    def load_latest(self) -> Optional[Snapshot]:
        """Load the most recent snapshot."""
        snapshots = sorted(self.snapshot_dir.glob("snapshot_*.json"), reverse=True)
        if not snapshots:
            return None

        with open(snapshots[0], "r", encoding="utf-8") as f:
            data = json.load(f)
        
        return self._dict_to_snapshot(data)

    def diff(
        self,
        current_snapshot: Snapshot,
        previous_snapshot: Optional[Snapshot] = None
    ) -> SnapshotDiff:
        """
        Compare two snapshots and identify changes.
        
        Args:
            current_snapshot: The new snapshot
            previous_snapshot: The old snapshot. If None, uses load_latest()
            
        Returns:
            SnapshotDiff with change information
        """
        if previous_snapshot is None:
            # Load previous from disk (excluding current)
            snapshots = sorted(self.snapshot_dir.glob("snapshot_*.json"), reverse=True)
            if len(snapshots) < 2:
                # No previous to compare against
                return SnapshotDiff(
                    timestamp_current=current_snapshot.timestamp,
                    timestamp_previous="none",
                    new_entries=current_snapshot.entries,
                    unchanged_entry_count=0
                )

            with open(snapshots[1], "r", encoding="utf-8") as f:
                data = json.load(f)
            previous_snapshot = self._dict_to_snapshot(data)

        # Build maps for comparison
        current_map = {self._entry_key(e): e for e in current_snapshot.entries}
        previous_map = {self._entry_key(e): e for e in previous_snapshot.entries}

        current_keys = set(current_map.keys())
        previous_keys = set(previous_map.keys())

        new_keys = current_keys - previous_keys
        removed_keys = previous_keys - current_keys
        unchanged_keys = current_keys & previous_keys

        diff = SnapshotDiff(
            timestamp_current=current_snapshot.timestamp,
            timestamp_previous=previous_snapshot.timestamp,
            new_entries=[current_map[k] for k in sorted(new_keys)],
            removed_entries=[previous_map[k] for k in sorted(removed_keys)],
            unchanged_entry_count=len(unchanged_keys)
        )

        # Detect changed entries (same key, different metadata)
        for key in unchanged_keys:
            if self._entries_differ(current_map[key], previous_map[key]):
                diff.changed_entries.append(current_map[key])

        return diff

    @staticmethod
    def _entry_key(entry: Entry) -> tuple:
        """Generate unique key for an entry based on identifying attributes."""
        return (entry.source, entry.name, entry.command)

    @staticmethod
    def _entries_differ(e1: Entry, e2: Entry) -> bool:
        """Check if two entries have meaningful differences."""
        return (e1.status != e2.status or
                e1.command != e2.command or
                e1.metadata != e2.metadata)

    @staticmethod
    def _snapshot_to_dict(snapshot: Snapshot) -> dict:
        """Convert Snapshot to JSON-serializable dict."""
        return {
            "timestamp": snapshot.timestamp,
            "entry_count": snapshot.entry_count,
            "metadata": snapshot.metadata,
            "entries": [
                {
                    "entry_id": e.entry_id,
                    "name": e.name,
                    "command": e.command,
                    "source": e.source,
                    "status": e.status,
                    "metadata": e.metadata
                }
                for e in snapshot.entries
            ]
        }

    @staticmethod
    def _dict_to_snapshot(data: dict) -> Snapshot:
        """Convert JSON dict to Snapshot."""
        entries = [
            Entry(
                entry_id=e.get("entry_id", ""),
                name=e.get("name", ""),
                command=e.get("command", ""),
                source=e.get("source", ""),
                status=e.get("status", "unknown"),
                metadata=e.get("metadata", {})
            )
            for e in data.get("entries", [])
        ]

        return Snapshot(
            timestamp=data.get("timestamp", ""),
            entries=entries,
            entry_count=data.get("entry_count", 0),
            metadata=data.get("metadata", {})
        )
