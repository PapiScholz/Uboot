# Windows Signing

`uboot-core.exe` should be Authenticode-signed before distribution.  
Without signing, Windows Smart App Control / SmartScreen may block or warn on execution even if the binary is technically valid.

## Current risk

- Unsigned binaries are high-friction on modern Windows installations.
- Smart App Control and SmartScreen care about publisher trust and reputation.
- In managed environments, WDAC/App Control policies may also require an allowed signer.

Signing is the correct mitigation. Telling users to disable Smart App Control is not.

## What to sign

At minimum:

- `uboot-core.exe`

For real releases, also sign:

- the packaged GUI executable (`Uboot.exe`)
- the final installer (`setup.exe`)

## Signing helper

This repo includes:

- PowerShell helper: [scripts/windows/sign-file.ps1](../scripts/windows/sign-file.ps1)
- Release helper: [scripts/windows/sign-release.ps1](../scripts/windows/sign-release.ps1)
- Optional CMake target: `sign-uboot-core`
- Installer build flow: [docs/WINDOWS_DISTRIBUTION.md](WINDOWS_DISTRIBUTION.md)

The helper supports either:

- certificate thumbprint from the Windows certificate store
- a `.pfx` file plus password

## Manual usage

### With a certificate thumbprint

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\sign-file.ps1 `
  -FilePath .\build-vs18\bin\Release\uboot-core.exe `
  -CertSha1 "<thumbprint>" `
  -TimestampUrl "http://timestamp.digicert.com" `
  -Description "Uboot core executable"
```

### With a PFX file

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\sign-file.ps1 `
  -FilePath .\build-vs18\bin\Release\uboot-core.exe `
  -PfxPath "C:\certs\uboot-signing.pfx" `
  -PfxPassword "<password>" `
  -TimestampUrl "http://timestamp.digicert.com" `
  -Description "Uboot core executable"
```

## CMake integration

Configure build with signing enabled:

```powershell
cmake -S . -B build-vs18 -G "Visual Studio 18 2026" -A x64 `
  -DUBOOT_ENABLE_SIGNING=ON `
  -DUBOOT_SIGN_CERT_SHA1="<thumbprint>"
```

Then either:

```powershell
cmake --build build-vs18 --config Release --target sign-uboot-core
```

Or sign immediately after every build:

```powershell
cmake -S . -B build-vs18 -G "Visual Studio 18 2026" -A x64 `
  -DUBOOT_ENABLE_SIGNING=ON `
  -DUBOOT_SIGN_AFTER_BUILD=ON `
  -DUBOOT_SIGN_CERT_SHA1="<thumbprint>"
```

## Validation

Verify the resulting signature:

```powershell
Get-AuthenticodeSignature .\build-vs18\bin\Release\uboot-core.exe | Format-List Status,SignerCertificate,TimeStamperCertificate,Path
```

Expected:

- `Status: Valid`

## Smart App Control / SmartScreen / WDAC

- **Smart App Control / SmartScreen**: signing is the baseline requirement. Reputation may still take time to build for a new publisher.
- **WDAC / App Control**: signing may not be sufficient if the enterprise policy only allows specific publishers or certificate chains. In that case, the policy must explicitly allow the signer used for Uboot.

## Recommended release practice

1. Build `uboot-core.exe`
2. Build GUI payload and `setup.exe` with Inno Setup
3. Sign `uboot-core.exe`, `Uboot.exe`, and `setup.exe`
4. Verify signatures before distribution

Example end-to-end signing:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\sign-release.ps1 `
  -PayloadDir .\build\windows-dist\payload `
  -InstallerExePath .\build\windows-dist\installer\Uboot-Setup-1.0.0.exe `
  -CertSha1 "<thumbprint>"
```
