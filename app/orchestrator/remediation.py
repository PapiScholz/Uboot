"""Remediation: transaction planning and application."""
import argparse
import json
import subprocess
import sys
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

    # Sources that map to registry-rename operations
    _REGISTRY_SOURCES = frozenset({"RunRegistry", "Winlogon", "IfeoDebugger", "WmiPersistence"})
    # Sources supported by C++ tx operations
    _SUPPORTED_SOURCES = _REGISTRY_SOURCES | {"Services", "ScheduledTasks"}

    def plan(
        self,
        entries: List[Any],
        reason: str = "User-initiated remediation",
        timeout: int = 60
    ) -> TransactionPlan:
        """
        Create a remediation plan (dry-run).

        Args:
            entries: Objects with .entry_id, .source, .location, .key attributes
            reason: Description of remediation reason
            timeout: subprocess timeout in seconds

        Returns:
            TransactionPlan with operations to apply
        """
        entry_ids = [getattr(e, "entry_id", "") for e in entries]

        op_specs = []
        for e in entries:
            src = getattr(e, "source", "") or ""
            meta = getattr(e, "metadata", {}) or {}
            loc = (meta.get("location", "") or "").replace("|", "/")
            key = (meta.get("key", "") or "").replace("|", "/")
            if src in self._SUPPORTED_SOURCES:
                op_specs.append(f"{src}|{loc}|{key}|disable")

        cmd = [
            str(self.uboot_core_exe), "tx", "plan",
            "--entry-ids", ",".join(entry_ids),
            "--reason", reason,
        ]
        for spec in op_specs:
            cmd += ["--op-spec", spec]

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


def _plan_to_dict(plan: TransactionPlan) -> Dict[str, Any]:
    """Convert TransactionPlan dataclass to JSON-serializable dict."""
    return {
        "tx_id": plan.tx_id,
        "entry_ids": plan.entry_ids,
        "reason": plan.reason,
        "executed": plan.executed,
        "operations": [
            {
                "type": op.op_type,
                "target": op.target,
                "action": op.action,
                "op_id": op.op_id,
                "metadata": op.metadata,
            }
            for op in plan.operations
        ],
    }


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Uboot remediation orchestrator")
    parser.add_argument(
        "--core-exe",
        default="uboot-core.exe",
        help="Path to uboot-core.exe (default: uboot-core.exe)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=60,
        help="Subprocess timeout in seconds (default: 60)",
    )
    parser.add_argument(
        "--pretty",
        action="store_true",
        help="Pretty-print JSON output",
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    plan_parser = subparsers.add_parser("plan", help="Create dry-run remediation plan")
    plan_parser.add_argument(
        "--entry-id",
        action="append",
        default=[],
        help="Single entry id (repeatable)",
    )
    plan_parser.add_argument(
        "--entry-ids",
        default="",
        help="Comma-separated entry ids",
    )
    plan_parser.add_argument(
        "--reason",
        default="User-initiated remediation",
        help="Reason for remediation plan",
    )

    apply_parser = subparsers.add_parser("apply", help="Apply transaction by tx-id")
    apply_parser.add_argument("--tx-id", required=True, help="Transaction ID")

    undo_parser = subparsers.add_parser("undo", help="Undo transaction by tx-id")
    undo_parser.add_argument("--tx-id", required=True, help="Transaction ID")

    subparsers.add_parser("list", help="List transactions")

    return parser


def _parse_entry_ids(single_ids: List[str], csv_ids: str) -> List[str]:
    entry_ids = [e.strip() for e in single_ids if e.strip()]
    if csv_ids:
        entry_ids.extend([e.strip() for e in csv_ids.split(",") if e.strip()])
    return entry_ids


class _EntryStub:
    """Minimal entry object for CLI usage where only the ID is known."""
    def __init__(self, entry_id: str) -> None:
        self.entry_id = entry_id
        self.source = ""
        self.metadata: Dict[str, Any] = {}


def main(argv: Optional[List[str]] = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    remediator = Remediation(uboot_core_exe=Path(args.core_exe))

    try:
        if args.command == "plan":
            entry_ids = _parse_entry_ids(args.entry_id, args.entry_ids)
            if not entry_ids:
                parser.error("plan requires at least one --entry-id or --entry-ids value")

            plan = remediator.plan(
                entries=[_EntryStub(eid) for eid in entry_ids],
                reason=args.reason,
                timeout=args.timeout,
            )
            output: Dict[str, Any] = _plan_to_dict(plan)
            exit_code = 0
        elif args.command == "apply":
            applied = remediator.apply(args.tx_id, timeout=args.timeout)
            output = {
                "tx_id": args.tx_id,
                "applied": applied,
            }
            exit_code = 0 if applied else 1
        elif args.command == "undo":
            undone = remediator.undo(args.tx_id, timeout=args.timeout)
            output = {
                "tx_id": args.tx_id,
                "undone": undone,
            }
            exit_code = 0 if undone else 1
        elif args.command == "list":
            output = {
                "transactions": [
                    _plan_to_dict(plan) for plan in remediator.list_transactions(timeout=args.timeout)
                ]
            }
            exit_code = 0
        else:
            parser.error(f"unsupported command: {args.command}")
            return 2

        print(json.dumps(output, indent=2 if args.pretty else None))
        return exit_code
    except FileNotFoundError:
        print(
            f"uboot-core.exe not found at: {args.core_exe}. "
            "Build core first or provide --core-exe with a valid path.",
            file=sys.stderr,
        )
        return 1
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired:
        print("Operation timed out", file=sys.stderr)
        return 1
    except json.JSONDecodeError as exc:
        print(f"Invalid JSON from uboot-core.exe: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
