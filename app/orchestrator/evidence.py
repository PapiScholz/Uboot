"""Evidence: detailed forensic data for a specific entry."""
import json
import subprocess
from pathlib import Path
from typing import Optional, Dict, Any
from dataclasses import dataclass


@dataclass
class EntryEvidence:
    """Detailed forensic evidence for a single entry."""
    entry_id: str
    file_info: Dict[str, Any] = None
    authenticode: Dict[str, Any] = None
    version_info: Dict[str, Any] = None
    misconfig_checks: Dict[str, Any] = None
    raw_evidence: Dict[str, Any] = None


class Evidence:
    """Orchestrates evidence gathering via uboot-core.exe."""

    def __init__(self, uboot_core_exe: Optional[Path] = None):
        """
        Initialize evidence gatherer.
        
        Args:
            uboot_core_exe: Path to uboot-core.exe binary. If None, searches PATH.
        """
        self.uboot_core_exe = uboot_core_exe or Path("uboot-core.exe")

    def get_evidence(
        self,
        entry_id: str,
        timeout: int = 60,
        pretty_print: bool = True
    ) -> EntryEvidence:
        """
        Gather detailed evidence for a specific entry.
        
        Args:
            entry_id: Unique identifier of the entry
            timeout: subprocess timeout in seconds
            pretty_print: Request pretty-printed JSON from uboot-core.exe
            
        Returns:
            EntryEvidence with forensic data
            
        Raises:
            FileNotFoundError: If uboot-core.exe not found
            subprocess.TimeoutExpired: If command times out
            json.JSONDecodeError: If output is invalid JSON
        """
        cmd = [str(self.uboot_core_exe), "evidence", "--entry-id", entry_id]
        
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

        return self._parse_evidence(entry_id, data)

    @staticmethod
    def _parse_evidence(entry_id: str, data: dict) -> EntryEvidence:
        """Parse uboot-core.exe evidence JSON."""
        evidence = EntryEvidence(entry_id=entry_id)
        
        evidence.file_info = data.get("file_info")
        evidence.authenticode = data.get("authenticode")
        evidence.version_info = data.get("version_info")
        evidence.misconfig_checks = data.get("misconfig_checks")
        evidence.raw_evidence = data
        
        return evidence
