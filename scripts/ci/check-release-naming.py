"""Fail when release naming and branding conventions drift."""
from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def main() -> int:
    iss = _read(ROOT / "scripts" / "windows" / "uboot-installer.iss")
    main_py = _read(ROOT / "app" / "main.py")

    output_match = re.search(r"OutputBaseFilename=([^\r\n]+)", iss, re.MULTILINE)
    if not output_match:
        raise RuntimeError("OutputBaseFilename was not found in uboot-installer.iss")
    output_base = output_match.group(1).strip()
    expected_output = "Uboot-Setup-{#AppVersion}"
    if output_base != expected_output:
        raise RuntimeError(f"Invalid installer naming. Expected '{expected_output}', got '{output_base}'.")

    app_name_match = re.search(r'#define\s+MyAppName\s+"([^"]+)"', iss, re.MULTILINE)
    if not app_name_match:
        raise RuntimeError("MyAppName is missing in uboot-installer.iss")
    app_name = app_name_match.group(1).strip()
    if app_name != "Uboot":
        raise RuntimeError(f"Installer app name drifted. Expected 'Uboot', got '{app_name}'.")

    if "Uboot v{APP_VERSION}" not in main_py:
        raise RuntimeError("About dialog must display APP_VERSION for release consistency.")

    print("Release naming check passed:")
    print(f"- OutputBaseFilename: {output_base}")
    print(f"- AppName: {app_name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
