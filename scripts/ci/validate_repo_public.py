"""Backward-compatible wrapper for check-public-repo."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    check = ROOT / "scripts" / "ci" / "check-public-repo.py"
    result = subprocess.run([sys.executable, str(check)], check=False)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
