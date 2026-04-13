"""Remediation: transaction planning and application."""
import json
import subprocess
from pathlib import Path
from typing import Optional, List, Dict, Any
from dataclasses import dataclass, field
from enum import Enum


class TxAction(str, Enum):
    """Transaction action types."""
    DISABLE = "disable"
    REMOVE = "remove"
    RENAME = "rename"
    ISOLATE = "isolate"


class TxMode(str, Enum):
    """Transaction operation modes."""
    PLAN = "plan"
    APPLY = "apply"
    UNDO = "undo"
    LIST = "list"


@dataclass
class Operation:
    """A single corrective operation."""
    op_type: str  # e.g., "RegistryRenameValue", "ServiceStartType", "TaskToggleEnabled"
    target: str
    action: str
    op_id: str = ""
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class TransactionPlan:
    """Plan for remediating entries."""
    tx_id: str
    entry_ids: List[str]
    operations: List[Operation] = field(default_factory=list)
    reason: str = ""
    executed: bool = False


class Remediation:
    """Orchestrates remediation via transaction system."""

    def __init__(self, uboot_core_exe: Optional[Path] = None):
        """
        Initialize remediation orchestrator.
        
        Args:
            uboot_core_exe: Path to uboot-core.exe binary.
        """
        self.uboot_core_exe = uboot_core_exe or Path("uboot-core.exe")

    def plan(
        self,
        entry_ids: List[str],
        reason: str = "User-initiated remediation",
        timeout: int = 60
    ) -> TransactionPlan:
        """
        Create a remediation plan (dry-run).
        
        Args:
            entry_ids: List of entry IDs to remediate
            reason: Description of remediation reason
            timeout: subprocess timeout in seconds
            
        Returns:
            TransactionPlan with operations to apply
        """
        cmd = [
            str(self.uboot_core_exe), "tx", "plan",
            "--entry-ids", ",".join(entry_ids),
            "--reason", reason
        ]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False
        )

        if result.returncode != 0:
            raise RuntimeError(
                f"uboot-core.exe tx plan failed: {result.stderr}"
            )

        data = json.loads(result.stdout)
        return self._parse_plan(data)

    def apply(
        self,
        tx_id: str,
        timeout: int = 60
    ) -> bool:
        """
        Apply a remediation plan.
        
        Args:
            tx_id: Transaction ID to apply
            timeout: subprocess timeout in seconds
            
        Returns:
            True if applied successfully
        """
        cmd = [
            str(self.uboot_core_exe), "tx", "apply",
            "--tx-id", tx_id
        ]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False
        )

        return result.returncode == 0

    def undo(
        self,
        tx_id: str,
        timeout: int = 60
    ) -> bool:
        """
        Undo a remediation plan.
        
        Args:
            tx_id: Transaction ID to undo
            timeout: subprocess timeout in seconds
            
        Returns:
            True if undo successful
        """
        cmd = [
            str(self.uboot_core_exe), "tx", "undo",
            "--tx-id", tx_id
        ]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False
        )

        return result.returncode == 0

    def list_transactions(self, timeout: int = 60) -> List[TransactionPlan]:
        """
        List all recorded transactions.
        
        Args:
            timeout: subprocess timeout in seconds
            
        Returns:
            List of TransactionPlan objects
        """
        cmd = [str(self.uboot_core_exe), "tx", "list"]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False
        )

        if result.returncode != 0:
            return []

        data = json.loads(result.stdout)
        return [self._parse_plan(tx_data) for tx_data in data.get("transactions", [])]

    @staticmethod
    def _parse_plan(data: dict) -> TransactionPlan:
        """Parse transaction plan from JSON."""
        plan = TransactionPlan(
            tx_id=data.get("tx_id", ""),
            entry_ids=data.get("entry_ids", []),
            reason=data.get("reason", ""),
            executed=data.get("executed", False)
        )

        for op_json in data.get("operations", []):
            op = Operation(
                op_type=op_json.get("type", ""),
                target=op_json.get("target", ""),
                action=op_json.get("action", ""),
                op_id=op_json.get("op_id", ""),
                metadata=op_json.get("metadata", {})
            )
            plan.operations.append(op)

        return plan
