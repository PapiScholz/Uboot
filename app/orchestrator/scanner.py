"""Scanner: invokes uboot-core.exe and parses JSON output."""
import json
import subprocess
import sys
from pathlib import Path
from typing import List, Optional
from dataclasses import dataclass, field


@dataclass
class Entry:
    """Represents a single startup/persistence entry."""
    entry_id: str
    name: str
    command: str
    source: str
    status: str = "unknown"
    metadata: dict = field(default_factory=dict)


@dataclass
class CollectorError:
    """Represents an error from a collector."""
    collector: str
    error: str


@dataclass
class ScanResult:
    """Result of a scan operation."""
    entries: List[Entry] = field(default_factory=list)
    errors: List[CollectorError] = field(default_factory=list)
    schema_version: str = "1.1"


class Scanner:
    """Orchestrates the collection phase via uboot-core.exe."""

    def __init__(self, uboot_core_exe: Optional[Path] = None):
        """
        Initialize scanner.
        
        Args:
            uboot_core_exe: Path to uboot-core.exe binary. If None, searches PATH.
        """
        self.uboot_core_exe = uboot_core_exe or Path("uboot-core.exe")

    def scan(
        self, 
        sources: Optional[List[str]] = None,
        pretty_print: bool = True,
        timeout: int = 60
    ) -> ScanResult:
        """
        Scan for startup/persistence entries.
        
        Args:
            sources: List of sources to scan (e.g., ["registry", "scheduled-tasks"]).
                    If None or empty, defaults to "all".
            pretty_print: Request pretty-printed JSON from uboot-core.exe
            timeout: subprocess timeout in seconds
            
        Returns:
            ScanResult with entries and any collection errors
            
        Raises:
            FileNotFoundError: If uboot-core.exe not found
            subprocess.TimeoutExpired: If scan times out
            json.JSONDecodeError: If output is invalid JSON
        """
        if not sources:
            sources = ["all"]

        cmd = [str(self.uboot_core_exe), "collect"]
        
        for source in sources:
            cmd.extend(["--sources", source])
            
        if pretty_print:
            cmd.append("--pretty")

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                check=False
            )
        except FileNotFoundError as e:
            raise FileNotFoundError(
                f"uboot-core.exe not found at {self.uboot_core_exe}"
            ) from e

        if result.returncode != 0:
            raise RuntimeError(
                f"uboot-core.exe exited with code {result.returncode}.\n"
                f"stderr: {result.stderr}"
            )

        try:
            data = json.loads(result.stdout)
        except json.JSONDecodeError as e:
            raise json.JSONDecodeError(
                f"Invalid JSON from uboot-core.exe: {str(e)}",
                result.stdout,
                e.pos
            ) from None

        return self._parse_scan_result(data)

    @staticmethod
    def _parse_scan_result(data: dict) -> ScanResult:
        """Parse uboot-core.exe JSON output into ScanResult."""
        result = ScanResult(
            schema_version=data.get("schema_version", "1.1")
        )

        # Parse entries
        for entry_json in data.get("entries", []):
            entry = Entry(
                entry_id=entry_json.get("entry_id", "unknown"),
                name=entry_json.get("name", ""),
                command=entry_json.get("command", ""),
                source=entry_json.get("source", ""),
                status=entry_json.get("status", "unknown"),
                metadata=entry_json.get("metadata", {})
            )
            result.entries.append(entry)

        # Parse errors
        for error_json in data.get("errors", []):
            error = CollectorError(
                collector=error_json.get("collector", "unknown"),
                error=error_json.get("error", "")
            )
            result.errors.append(error)

        return result
