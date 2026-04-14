"""Fail when app, CMake, and Inno versions are inconsistent."""
from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _extract(pattern: str, content: str, label: str) -> str:
    match = re.search(pattern, content, re.MULTILINE)
    if not match:
        raise RuntimeError(f"Could not extract {label}.")
    return match.group(1)


def main() -> int:
    app_init = _read(ROOT / "app" / "__init__.py")
    cmake = _read(ROOT / "CMakeLists.txt")
    iss = _read(ROOT / "scripts" / "windows" / "uboot-installer.iss")

    py_version = _extract(r'__version__\s*=\s*"([^"]+)"', app_init, "Python app version")
    cmake_version = _extract(r"project\(Uboot VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", cmake, "CMake version")
    installer_version = _extract(r'#define\s+AppVersion\s+"([^"]+)"', iss, "Inno default version")

    if py_version != cmake_version:
        raise RuntimeError(f"Version mismatch: app={py_version}, cmake={cmake_version}")
    if py_version != installer_version:
        raise RuntimeError(f"Version mismatch: app={py_version}, installer={installer_version}")

    print("Version consistency check passed:")
    print(f"- app version: {py_version}")
    print(f"- cmake version: {cmake_version}")
    print(f"- installer version: {installer_version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
