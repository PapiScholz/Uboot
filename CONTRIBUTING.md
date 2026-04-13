# Contributing to Uboot

## Stack

| Layer | Language | Toolchain |
|-------|----------|-----------|
| Core | C++17 | MSVC v143, CMake ≥ 3.15 |
| Orchestration / Scoring | Python 3.12+ | venv, pytest |
| GUI | Python 3.12+ / PySide6 Qt6 | — |

---

## Getting started

```powershell
git clone https://github.com/<you>/Uboot
cd Uboot

# C++ core
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Python
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install PySide6 pytest
```

---

## C++ conventions

- Standard: **C++17**, no extensions (`/std:c++17`)
- Naming: `PascalCase` for types/files, `camelCase` for members and locals
- No raw `new`/`delete` — use `std::unique_ptr` / `std::shared_ptr`
- Error returns via `std::expected` or explicit error structs; no exception-based control flow in collectors
- Every collector implements `ICollector` from `core/enumerators/ICollector.h`
- JSON output through `core/json/JsonWriter`; never write to stdout directly from collectors
- Headers use `#pragma once`

---

## Python conventions

- Type hints on all public function signatures
- Dataclasses for value objects
- `subprocess.run()` with `check=True` and explicit encoding to invoke `uboot-core.exe`
- No global mutable state
- Tests in `tests/` using pytest

---

## Pull request flow

1. Branch from `main`: `git checkout -b feat/<name>` or `fix/<name>`
2. Keep PRs focused — one concern per PR
3. If the change touches the C++/Python JSON boundary, update `docs/ARCHITECTURE.md`
4. Run the smoke tests before opening the PR (`pytest tests/`)
5. ROADMAP.md items completed by the PR should be checked off in the same commit
