"""Test fixtures for Uboot smoke tests."""
import json
from pathlib import Path


# Clean entry fixture - legitimate Windows service
CLEAN_ENTRY = {
    "entry_id": "svc-winupdate",
    "name": "Windows Update",
    "command": "C:\\Windows\\System32\\svchost.exe -k netsvcs",
    "source": "services",
    "status": "running",
    "metadata": {
        "service_type": "win32_share_process",
        "start_type": "automatic",
        "signed": True,
        "publisher": "Microsoft Corporation"
    }
}

# Suspicious entry fixture - unknown startup
SUSPICIOUS_ENTRY = {
    "entry_id": "reg-startup-unknown",
    "name": "MysteryApp",
    "command": "C:\\Users\\Admin\\AppData\\Local\\Temp\\mystery.exe",
    "source": "registry::HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
    "status": "active",
    "metadata": {
        "created": "2026-04-12T10:30:00Z",
        "last_modified": "2026-04-12T10:30:00Z",
        "signed": False,
        "publisher": None
    }
}

# Malicious entry fixture - known trojan
MALICIOUS_ENTRY = {
    "entry_id": "svc-trojan",
    "name": "Windows Service",
    "command": "C:\\Windows\\System32\\rundll32.exe malware.dll",
    "source": "services",
    "status": "running",
    "metadata": {
        "created": "2026-04-10T03:15:00Z",
        "last_modified": "2026-04-10T03:15:00Z",
        "signed": False,
        "publisher": None,
        "file_size": 262144,
        "entropy": 7.85
    }
}

# Scan result fixture
SCAN_RESULT_FIXTURE = {
    "schema_version": "1.1",
    "entries": [CLEAN_ENTRY, SUSPICIOUS_ENTRY, MALICIOUS_ENTRY],
    "errors": [
        {
            "collector": "WmiPersistenceCollector",
            "error": "Access denied when querying WMI"
        }
    ]
}

# Transaction plan fixture
TX_PLAN_FIXTURE = {
    "tx_id": "tx-20260413-abc123",
    "entry_ids": [SUSPICIOUS_ENTRY["entry_id"], MALICIOUS_ENTRY["entry_id"]],
    "reason": "User-initiated remediation",
    "executed": False,
    "operations": [
        {
            "type": "RegistryRenameValue",
            "target": "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            "action": "disable",
            "op_id": "op-001",
            "metadata": {
                "value_name": "MysteryApp",
                "backup_location": "C:\\backups\\registry_backup_20260413.reg"
            }
        },
        {
            "type": "ServiceStartType",
            "target": "Windows Service",
            "action": "disable",
            "op_id": "op-002",
            "metadata": {
                "service_name": "TrojanService",
                "current_start_type": "auto",
                "new_start_type": "disabled"
            }
        }
    ]
}


def get_fixture_path(name: str) -> Path:
    """Get path to a fixture JSON file."""
    return Path(__file__).parent / f"{name}.json"


def save_fixtures():
    """Save fixture data to JSON files."""
    fixtures_dir = Path(__file__).parent
    
    # Save individual entry fixtures
    with open(fixtures_dir / "entry_clean.json", "w") as f:
        json.dump({"entries": [CLEAN_ENTRY]}, f, indent=2)
    
    with open(fixtures_dir / "entry_suspicious.json", "w") as f:
        json.dump({"entries": [SUSPICIOUS_ENTRY]}, f, indent=2)
    
    with open(fixtures_dir / "entry_malicious.json", "w") as f:
        json.dump({"entries": [MALICIOUS_ENTRY]}, f, indent=2)
    
    # Save full scan result
    with open(fixtures_dir / "scan_result_mixed.json", "w") as f:
        json.dump(SCAN_RESULT_FIXTURE, f, indent=2)
    
    # Save transaction plan
    with open(fixtures_dir / "tx_plan.json", "w") as f:
        json.dump(TX_PLAN_FIXTURE, f, indent=2)


if __name__ == "__main__":
    save_fixtures()
    print("✓ Fixtures saved")
