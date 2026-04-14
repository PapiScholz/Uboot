"""Validate release identity and version consistency for Uboot."""
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
    main_py = _read(ROOT / "app" / "main.py")
    license_text = _read(ROOT / "LICENSE")

    py_version = _extract(r'__version__\s*=\s*"([^"]+)"', app_init, "Python app version")
    cmake_version = _extract(r"project\(Uboot VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", cmake, "CMake version")
    iss_version = _extract(r'#define\s+AppVersion\s+"([^"]+)"', iss, "Inno default version")
    output_base = _extract(r"OutputBaseFilename=([^\r\n]+)", iss, "installer output basename")

    if "MIT License" not in license_text:
        raise RuntimeError("LICENSE must remain a clear OSS license (MIT expected).")
    if py_version != cmake_version:
        raise RuntimeError(f"Version mismatch: app={py_version}, cmake={cmake_version}")
    if py_version != iss_version:
        raise RuntimeError(f"Version mismatch: app={py_version}, installer={iss_version}")
    if output_base.strip() != "Uboot-Setup-{#AppVersion}":
        raise RuntimeError("Installer output basename must be 'Uboot-Setup-{#AppVersion}'.")
    if "Uboot v{APP_VERSION}" not in main_py:
        raise RuntimeError("About dialog must display APP_VERSION for release consistency.")

    print("Release identity checks passed:")
    print(f"- version: {py_version}")
    print("- product name and setup filename convention are consistent")
    print("- OSS license detected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
