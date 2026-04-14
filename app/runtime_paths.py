"""Runtime path helpers for source and packaged executions."""
from __future__ import annotations

import os
import sys
from pathlib import Path


def is_frozen() -> bool:
    """Return True when running from a bundled executable (e.g., PyInstaller)."""
    return bool(getattr(sys, "frozen", False))


def app_root() -> Path:
    """Return the application root directory."""
    if is_frozen():
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent.parent


def user_data_dir() -> Path:
    """Return the per-user writable application data directory."""
    localappdata = os.environ.get("LOCALAPPDATA", "").strip()
    if localappdata:
        return Path(localappdata) / "Uboot"
    return Path.home() / ".uboot"
