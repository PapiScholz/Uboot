param(
    [Parameter(Mandatory = $true)]
    [string]$PayloadDir,

    [Parameter(Mandatory = $true)]
    [string]$InstallerExePath,

    [string]$SignToolPath = "",
    [string]$CertSha1 = "",
    [string]$PfxPath = "",
    [string]$PfxPassword = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$signScript = Join-Path $scriptRoot "sign-file.ps1"

$resolvedPayload = (Resolve-Path -LiteralPath $PayloadDir).Path
$resolvedInstaller = (Resolve-Path -LiteralPath $InstallerExePath).Path

$targets = @(
    @{ Path = (Join-Path $resolvedPayload "uboot-core.exe"); Description = "Uboot core executable" },
    @{ Path = (Join-Path $resolvedPayload "Uboot.exe"); Description = "Uboot GUI executable" },
    @{ Path = $resolvedInstaller; Description = "Uboot setup installer" }
)

foreach ($target in $targets) {
    if (-not (Test-Path -LiteralPath $target.Path)) {
        throw "Signing target not found: $($target.Path)"
    }

    $signArgs = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $signScript,
        "-FilePath", $target.Path,
        "-TimestampUrl", $TimestampUrl,
        "-Description", $target.Description
    )

    if (-not [string]::IsNullOrWhiteSpace($SignToolPath)) {
        $signArgs += @("-SignToolPath", $SignToolPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($CertSha1)) {
        $signArgs += @("-CertSha1", $CertSha1)
    }
    if (-not [string]::IsNullOrWhiteSpace($PfxPath)) {
        $signArgs += @("-PfxPath", $PfxPath)
        if (-not [string]::IsNullOrWhiteSpace($PfxPassword)) {
            $signArgs += @("-PfxPassword", $PfxPassword)
        }
    }

    & powershell @signArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Signing failed for $($target.Path) with exit code $LASTEXITCODE"
    }
}

Write-Host ""
Write-Host "Release artifacts signed successfully:"
foreach ($target in $targets) {
    $sig = Get-AuthenticodeSignature -LiteralPath $target.Path
    Write-Host " - $($target.Path) => $($sig.Status)"
}
