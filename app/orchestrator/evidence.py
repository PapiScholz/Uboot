"""Evidence: detailed forensic data for a specific entry."""
import json
import subprocess
from pathlib import Path
from typing import Optional, Dict, Any
from dataclasses import dataclass

from .scanner import Entry


@dataclass
class EntryEvidence:
    """Detailed forensic evidence for a single entry."""
    entry_id: str
    file_info: Dict[str, Any] = None
    authenticode: Dict[str, Any] = None
    version_info: Dict[str, Any] = None
    misconfig_checks: Dict[str, Any] = None
    raw_evidence: Dict[str, Any] = None

    def to_metadata_updates(self) -> Dict[str, Any]:
        """Map evidence fields to metadata consumed by the GUI scorer."""
        updates: Dict[str, Any] = {}

        file_info = self.file_info or {}
        if "exists" in file_info:
            updates["file_exists"] = bool(file_info["exists"])
            updates["target_exists"] = bool(file_info["exists"])

        authenticode = self.authenticode or {}
        signed = authenticode.get("signed")
        if signed is not None:
            updates["signed"] = bool(signed)

        signature_status = str(authenticode.get("status") or "").strip()
        if signature_status:
            updates["signature_status"] = signature_status
            updates["signature_valid"] = signature_status.lower() in {
                "valid",
                "ok",
                "trusted",
                "success",
            }

        signer_subject = authenticode.get("signer_subject")
        if signer_subject:
            updates["publisher"] = signer_subject

        return updates


class Evidence:
    """Orchestrates evidence gathering for a selected entry on Windows."""

    def __init__(self, uboot_core_exe: Optional[Path] = None):
        """
        Initialize evidence gatherer.
        
        Args:
            uboot_core_exe: Path to uboot-core.exe binary. If None, searches PATH.
        """
        self.uboot_core_exe = uboot_core_exe or Path("uboot-core.exe")

    def get_evidence(
        self,
        entry: Entry,
        timeout: int = 60,
    ) -> EntryEvidence:
        """
        Gather detailed evidence for a specific entry.
        
        Args:
            entry: Entry selected in the UI
            timeout: subprocess timeout in seconds
            
        Returns:
            EntryEvidence with forensic data
            
        Raises:
            RuntimeError: If no executable path can be resolved for the entry
            subprocess.TimeoutExpired: If command times out
            json.JSONDecodeError: If output is invalid JSON
        """
        image_path = (entry.image_path or "").strip()
        if not image_path:
            raise RuntimeError(f"No executable path available for entry {entry.entry_id}")

        script = r"""
$path = $args[0]
$exists = Test-Path -LiteralPath $path -PathType Leaf

if (-not $exists) {
    [pscustomobject]@{
        file_info = @{
            path = $path
            exists = $false
        }
        authenticode = @{
            signed = $false
            status = 'MissingFile'
            status_message = 'Target file does not exist'
            signer_subject = $null
            signer_issuer = $null
        }
        version_info = $null
        misconfig_checks = $null
    } | ConvertTo-Json -Depth 6 -Compress
    exit 0
}

$item = Get-Item -LiteralPath $path
$sig = Get-AuthenticodeSignature -LiteralPath $path
$version = $item.VersionInfo
$hash = Get-FileHash -LiteralPath $path -Algorithm SHA256

[pscustomobject]@{
    file_info = @{
        path = $item.FullName
        exists = $true
        size = $item.Length
        last_write_time_utc = $item.LastWriteTimeUtc.ToString('o')
    }
    authenticode = @{
        signed = ($sig.SignerCertificate -ne $null)
        status = [string]$sig.Status
        status_message = [string]$sig.StatusMessage
        signer_subject = if ($sig.SignerCertificate) { [string]$sig.SignerCertificate.Subject } else { $null }
        signer_issuer = if ($sig.SignerCertificate) { [string]$sig.SignerCertificate.Issuer } else { $null }
    }
    version_info = @{
        company_name = [string]$version.CompanyName
        file_description = [string]$version.FileDescription
        product_name = [string]$version.ProductName
        file_version = [string]$version.FileVersion
    }
    misconfig_checks = $null
    hashes = @{
        sha256 = [string]$hash.Hash
    }
} | ConvertTo-Json -Depth 6 -Compress
"""

        cmd = ["powershell.exe", "-NoProfile", "-Command", script, image_path]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                check=False
            )
        except FileNotFoundError as e:
            raise FileNotFoundError("powershell.exe not found") from e

        if result.returncode != 0:
            raise RuntimeError(
                f"evidence collection exited with code {result.returncode}.\n"
                f"stderr: {result.stderr}"
            )

        try:
            data = json.loads(result.stdout)
        except json.JSONDecodeError as e:
            raise json.JSONDecodeError(
                f"Invalid JSON from evidence collection: {str(e)}",
                result.stdout,
                e.pos
            ) from None

        return self._parse_evidence(entry.entry_id, data)

    @staticmethod
    def _parse_evidence(entry_id: str, data: dict) -> EntryEvidence:
        """Parse evidence JSON into an EntryEvidence object."""
        evidence = EntryEvidence(entry_id=entry_id)
        
        evidence.file_info = data.get("file_info")
        evidence.authenticode = data.get("authenticode")
        evidence.version_info = data.get("version_info")
        evidence.misconfig_checks = data.get("misconfig_checks")
        evidence.raw_evidence = data
        
        return evidence
