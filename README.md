# Uboot

Windows persistence auditor with explainable scoring, forensic evidence, and reversible remediation.

Think Sysinternals Autoruns — but with a risk score on every entry, cryptographic evidence capture, and a one-click undo for any change it makes.

---

## What it does

1. **Scan** — collects all autostart entries across the most common persistence vectors (Run keys, Services, Scheduled Tasks, Startup folder, Winlogon, WMI subscriptions, IFEO debugger hijacks).
2. **Score** — assigns every entry a risk classification (clean / suspicious / malicious) with an explanation derived from rule-based signals: publisher, signing, path anomalies, known-good list.
3. **Evidence** — captures SHA-256 hash, Authenticode signature, version info, and file metadata for each entry before touching anything.
4. **Remediate** — proposes a reversible action plan, requires explicit confirmation, executes it transactionally, and writes a snapshot you can undo from.

---

## Architecture

```
┌───────────────────────────── GUI (PySide6 / Qt6) ────────────────────────────────┐
│  main.py  ·  views/  ·  widgets/                                                 │
└──────────────────────────────────┬───────────────────────────────────────────────┘
                                   │  Python calls / QThread
┌──────────────────────────────────▼───────────────────────────────────────────────┐
│  Orchestrator (Python 3.12+)                                                     │
│  scanner.py · evidence.py · scoring.py · remediation.py · snapshot.py            │
└──────────────────────────────────┬───────────────────────────────────────────────┘
                                   │  subprocess JSON  (stdout / stdin)
┌──────────────────────────────────▼───────────────────────────────────────────────┐
│  uboot-core.exe  (C++17)                                                         │
│  cli/main.cpp · enumerators/ · evidence/ · tx/ · backup/ · ops/                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

The C++ core runs as a subprocess and communicates via newline-delimited JSON on stdout.  
The Python layer orchestrates, scores, diffs, and drives the GUI.  
No COM bindings, no P/Invoke — the boundary is a plain process call.

---

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| MSVC | VS 2022 (v143) | C++17, x64 |
| CMake | ≥ 3.15 | |
| Python | ≥ 3.12 | 64-bit |
| PySide6 | ≥ 6.7 | `pip install PySide6` |

---

## Build

```powershell
# C++ core
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
# binary: build/bin/Release/uboot-core.exe

# Python dependencies
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install PySide6

# Run GUI
python -m app.main
```

---

## Project layout

```
app/
  main.py                  GUI entry point (PySide6)
  orchestrator/            Python orchestration layer
    scanner.py
    evidence.py
    scoring.py
    remediation.py
    snapshot.py
cli/
  main.cpp                 C++ entry point (JSON CLI)
core/
  enumerators/             7 Windows persistence collectors
  evidence/                SHA-256, Authenticode, version info
  backup/                  Pre-remediation snapshots
  tx/                      ACID transaction layer
  ops/                     Correctional operations (Registry, Service, Task)
  runner/                  CollectorRunner orchestration
  util/                    CLI args, locks, command resolver
  normalize/               Command path normalization
  resolve/                 .lnk resolver
  json/                    JSON writer
  hardening/               Policy inspector, security log, dependency analysis
  model/                   Shared data types (Entry, CollectorError)
docs/
  ARCHITECTURE.md
  SIGNING.md
rules/
  rules_v1.json            Signal definitions for scoring
tests/
  fixtures/                Reference JSONs (clean, suspicious, malicious)
```

---

## Status

See [ROADMAP.md](ROADMAP.md) for the current implementation plan and progress.

---

## Windows signing

`uboot-core.exe` is currently a normal Windows executable build artifact.  
For distribution, it should be Authenticode-signed to reduce Smart App Control / SmartScreen friction.

This repo includes:

- optional CMake signing target support
- PowerShell signing helper at [scripts/windows/sign-file.ps1](scripts/windows/sign-file.ps1)
- detailed signing notes at [docs/SIGNING.md](docs/SIGNING.md)

Quick validation:

```powershell
Get-AuthenticodeSignature .\build-vs18\bin\Release\uboot-core.exe | Format-List Status,SignerCertificate,TimeStamperCertificate,Path
```

---

## Optional local AI component

`LLM Assistance` can run in `Off` or `Better` mode from the GUI.

- `Off`: heuristic evidence only.
- `Better`: downloads the shared `llama.cpp` runtime plus the recommended local model on first use.

Behavior:

- The first time a missing mode is selected, Uboot shows a short hardware/benefit banner and starts the install automatically in background.
- Downloaded assets are cached under `llm/`.
- If installation or inference fails, the GUI falls back to heuristic evidence and remains usable.

Current local model choice:

- `Better` uses `Qwen/Qwen2.5-1.5B-Instruct-GGUF` in `Q4_K_M`.
- Target hardware is `6 GB minimum / 8 GB recommended`.
- `Fast` was removed because its smaller model did not meet the quality bar for remediation guidance.

Advanced overrides for development/testing are available through environment variables:

- `UBOOT_LLAMA_RUNTIME_URL`
- `UBOOT_LLAMA_RUNTIME_SHA256`
- `UBOOT_LLM_BETTER_MODEL_URL`
- `UBOOT_LLM_BETTER_MODEL_SHA256`

---

## License

See [LICENSE](LICENSE).
