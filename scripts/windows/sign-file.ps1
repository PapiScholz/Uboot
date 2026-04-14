param(
    [Parameter(Mandatory = $true)]
    [string]$FilePath,

    [string]$SignToolPath = "",
    [string]$CertSha1 = "",
    [string]$PfxPath = "",
    [string]$PfxPassword = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [string]$Description = "Uboot binary"
)

$ErrorActionPreference = "Stop"

function Resolve-SignTool {
    param([string]$ExplicitPath)

    if ($ExplicitPath -and (Test-Path -LiteralPath $ExplicitPath)) {
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $candidates = @(
        "$env:ProgramFiles(x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe",
        "$env:ProgramFiles(x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe",
        "$env:ProgramFiles(x86)\Windows Kits\10\bin\10.0.22000.0\x64\signtool.exe",
        "$env:ProgramFiles(x86)\Windows Kits\10\App Certification Kit\signtool.exe"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $found = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($found) {
        return $found.Source
    }

    throw "signtool.exe not found. Install the Windows SDK / App Certification Kit or pass -SignToolPath."
}

function Assert-CertificateInput {
    param(
        [string]$Thumbprint,
        [string]$Pfx
    )

    if ([string]::IsNullOrWhiteSpace($Thumbprint) -and [string]::IsNullOrWhiteSpace($Pfx)) {
        throw "No signing certificate configured. Provide -CertSha1 or -PfxPath."
    }
}

$resolvedFile = (Resolve-Path -LiteralPath $FilePath).Path
$resolvedSignTool = Resolve-SignTool -ExplicitPath $SignToolPath
Assert-CertificateInput -Thumbprint $CertSha1 -Pfx $PfxPath

$signArgs = @(
    "sign",
    "/fd", "SHA256",
    "/td", "SHA256",
    "/d", $Description
)

if (-not [string]::IsNullOrWhiteSpace($TimestampUrl)) {
    $signArgs += @("/tr", $TimestampUrl)
}

if (-not [string]::IsNullOrWhiteSpace($PfxPath)) {
    $resolvedPfx = (Resolve-Path -LiteralPath $PfxPath).Path
    $signArgs += @("/f", $resolvedPfx)
    if (-not [string]::IsNullOrWhiteSpace($PfxPassword)) {
        $signArgs += @("/p", $PfxPassword)
    }
} else {
    $signArgs += @("/sha1", $CertSha1)
}

$signArgs += $resolvedFile

Write-Host "Signing: $resolvedFile"
Write-Host "Using signtool: $resolvedSignTool"

& $resolvedSignTool @signArgs
if ($LASTEXITCODE -ne 0) {
    throw "signtool.exe failed with exit code $LASTEXITCODE"
}

$signature = Get-AuthenticodeSignature -LiteralPath $resolvedFile
if ($signature.Status -ne "Valid") {
    throw "Signing completed but verification status is '$($signature.Status)'."
}

Write-Host "Authenticode signature valid."
