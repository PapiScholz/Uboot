"""Backward-compatible wrapper for new CI checks."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    checks = [
        ROOT / "scripts" / "ci" / "check-version-consistency.py",
        ROOT / "scripts" / "ci" / "check-release-naming.py",
    ]
    for check in checks:
        result = subprocess.run([sys.executable, str(check)], check=False)
        if result.returncode != 0:
            return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
