# Windows Distribution (Signed `setup.exe`, Offline AI Bundle)

This document defines the release flow for distributing Uboot as a signed per-machine installer with an embedded local AI component.

## Scope

- Primary format: signed `setup.exe` (Inno Setup).
- Install scope: per-machine (`Program Files\Uboot`).
- No auto-update in this phase.
- Manual update check only (Help menu).

## Contents of the installed app

The installer payload must include:

- `Uboot.exe` (PyInstaller one-folder GUI executable)
- `uboot-core.exe`
- `rules/rules_v1.json`
- `llm/runtime/llama-completion.exe` plus required runtime DLLs only
- `llm/models/qwen2.5-1.5b-instruct-q4_k_m.gguf`
- `llm/manifest.json`

`llm/runtime` must exclude non-essential executables (bench/debug/tools not used by Uboot).

## Build and package

1. Build core:

```powershell
cmake --build build-vs18 --config Release
```

2. Build payload and installer:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\build-release.ps1
```

Outputs (default):

- payload folder: `build/windows-dist/payload`
- installer folder: `build/windows-dist/installer`

## Sign artifacts

Sign all release-critical binaries with timestamp:

- `payload\uboot-core.exe`
- `payload\Uboot.exe`
- final `setup.exe`

Example:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\sign-release.ps1 `
  -PayloadDir .\build\windows-dist\payload `
  -InstallerExePath .\build\windows-dist\installer\Uboot-Setup-1.0.0.exe `
  -CertSha1 "<thumbprint>"
```

Then verify:

```powershell
Get-AuthenticodeSignature .\build\windows-dist\payload\uboot-core.exe
Get-AuthenticodeSignature .\build\windows-dist\payload\Uboot.exe
Get-AuthenticodeSignature .\build\windows-dist\installer\Uboot-Setup-1.0.0.exe
```

Expected status: `Valid` for all.

## Installer behavior (Inno Setup)

- `PrivilegesRequired=admin` (per-machine install).
- Default location: `Program Files\Uboot`.
- Adds Start Menu shortcut.
- Optional desktop shortcut (unchecked by default).
- Registers uninstall entry in Apps & Features.
- Local AI component is selectable in wizard and enabled by default.

## LLM offline-first behavior

- If AI component is installed by setup, `Better` mode is immediately available.
- Runtime/model download remains a fallback path only.
- `llm/manifest.json` is bundled and mirrored by runtime checks.

## Update policy (phase 1)

- No auto-updater.
- `Help -> Check for Updates...` shows current version and opens release URL.
- Release URL can be overridden with `UBOOT_RELEASES_URL`.

## SmartScreen / WDAC notes

- Smart App Control / SmartScreen: signing is mandatory baseline; reputation may still take time.
- WDAC / App Control (enterprise): signing alone may not be enough if policy only allows specific publishers/chains.
- Product policy: do not require users to disable Smart App Control.
