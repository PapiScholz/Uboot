"""Persistent application settings for Uboot."""
from __future__ import annotations

import json
import os
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Optional


@dataclass
class AppSettings:
    """Serializable application settings."""

    llm_mode: Optional[str] = None
    llm_mode_configured: bool = False
    llm_component_status: str = "unknown"
    llm_component_error: str = ""
    llm_installed_runtime_version: str = ""
    llm_better_installed: bool = False


class SettingsStore:
    """Persist lightweight GUI settings in a JSON file."""

    def __init__(self, path: Optional[Path] = None):
        self.path = path or self._default_path()

    @staticmethod
    def _default_path() -> Path:
        """Resolve the default settings location."""
        override = os.environ.get("UBOOT_SETTINGS_PATH", "").strip()
        if override:
            return Path(override)

        appdata = os.environ.get("LOCALAPPDATA", "").strip()
        if appdata:
            return Path(appdata) / "Uboot" / "settings.json"

        return Path.home() / ".uboot" / "settings.json"

    def load(self) -> AppSettings:
        """Load settings from disk or return defaults."""
        if not self.path.exists():
            return AppSettings()

        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return AppSettings()

        raw_mode = data.get("llm_mode")
        normalized_mode = str(raw_mode).strip().lower() if raw_mode is not None else None
        if normalized_mode == "fast":
            normalized_mode = "better"

        return AppSettings(
            llm_mode=normalized_mode,
            llm_mode_configured=bool(data.get("llm_mode_configured", False)),
            llm_component_status=str(data.get("llm_component_status", "unknown")),
            llm_component_error=str(data.get("llm_component_error", "")),
            llm_installed_runtime_version=str(data.get("llm_installed_runtime_version", "")),
            llm_better_installed=bool(data.get("llm_better_installed", False)),
        )

    def save(self, settings: AppSettings) -> None:
        """Persist settings to disk."""
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(
            json.dumps(asdict(settings), indent=2),
            encoding="utf-8",
        )
