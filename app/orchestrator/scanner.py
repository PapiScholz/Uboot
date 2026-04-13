"""Scanner: invokes uboot-core.exe and parses JSON output."""
import json
import subprocess
import sys
import hashlib
from pathlib import Path
from typing import List, Optional
from dataclasses import dataclass, field


@dataclass
class Entry:
    """Represents a single startup/persistence entry."""
    entry_id: str
    name: str
    command: str
    image_path: str
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
        self.uboot_core_exe = uboot_core_exe or self._resolve_uboot_core_path()

    @staticmethod
    def _resolve_uboot_core_path() -> Path:
        """Resolve uboot-core.exe from common local build locations."""
        candidates = [
            Path("uboot-core.exe"),
            Path("build-vs18/bin/Release/uboot-core.exe"),
            Path("build/bin/Release/uboot-core.exe"),
            Path("build-vs18/Release/uboot-core.exe"),
            Path("build/Release/uboot-core.exe"),
        ]

        for candidate in candidates:
            if candidate.exists():
                return candidate

        return Path("uboot-core.exe")

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

        # CLI defaults to collect mode; do not pass subcommand.
        cmd = [str(self.uboot_core_exe)]
        
        for source in sources:
            cmd.extend(["--source", source])
            
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
            source = str(entry_json.get("source", ""))

            name = Scanner._pick_first(
                entry_json,
                ["name", "displayName", "keyName", "key"],
                default="(unnamed)"
            )

            image_path = Scanner._extract_image_path(entry_json)
            command = Scanner._extract_command(entry_json)
            status = Scanner._pick_first(
                entry_json,
                ["status", "state", "startType"],
                default="unknown"
            )

            entry_id = Scanner._pick_first(entry_json, ["entry_id", "id", "guid"], default="")
            if not entry_id:
                entry_id = Scanner._build_entry_id(
                    source=source,
                    name=name,
                    command=command,
                    location=str(entry_json.get("location", "")),
                    key=str(entry_json.get("key", "")),
                    scope=str(entry_json.get("scope", "")),
                )

            metadata = dict(entry_json.get("metadata", {}))
            for key, value in entry_json.items():
                if key in {
                    "entry_id", "id", "guid", "name", "displayName", "keyName", "key",
                    "command", "imagePath", "arguments", "source", "status", "state", "startType",
                    "metadata"
                }:
                    continue
                metadata.setdefault(key, value)

            entry = Entry(
                entry_id=entry_id,
                name=name,
                command=command,
                image_path=image_path,
                source=source,
                status=status,
                metadata=metadata,
            )
            result.entries.append(entry)

        # Parse errors
        for error_json in data.get("errors", []):
            error = CollectorError(
                collector=error_json.get("collector") or error_json.get("source") or "unknown",
                error=error_json.get("error") or error_json.get("message") or ""
            )
            result.errors.append(error)

        return result

    @staticmethod
    def _pick_first(data: dict, keys: List[str], default: str = "") -> str:
        """Return the first non-empty string value from candidate keys."""
        for key in keys:
            value = data.get(key)
            if value is None:
                continue
            text = str(value).strip()
            if text:
                return text
        return default

    @staticmethod
    def _extract_command(entry_json: dict) -> str:
        """Extract executable command from known schema variants."""
        direct = Scanner._pick_first(entry_json, ["command"], default="")
        if direct:
            return direct

        image_path = Scanner._extract_image_path(entry_json)
        arguments = Scanner._pick_first(entry_json, ["arguments", "args"], default="")
        if image_path and arguments:
            return f"{image_path} {arguments}".strip()
        return image_path

    @staticmethod
    def _extract_image_path(entry_json: dict) -> str:
        """Extract resolved executable path from known schema variants."""
        return Scanner._pick_first(entry_json, ["imagePath", "path", "executablePath"], default="")

    @staticmethod
    def _build_entry_id(
        source: str,
        name: str,
        command: str,
        location: str,
        key: str,
        scope: str,
    ) -> str:
        """Generate stable entry id when core output does not provide one."""
        seed = "|".join([source, scope, location, key, name, command])
        digest = hashlib.sha1(seed.encode("utf-8", errors="ignore")).hexdigest()[:16]
        source_prefix = source.lower() if source else "entry"
        return f"{source_prefix}-{digest}"
