param(
    [string]$CoreExePath = ".\build-vs18\bin\Release\uboot-core.exe",
    [string]$RuntimeDir = ".\llm\runtime",
    [string]$ModelPath = ".\llm\models\qwen2.5-1.5b-instruct-q4_k_m.gguf",
    [string]$OutputRoot = ".\build\windows-dist",
    [switch]$SkipInstaller,
    [switch]$SkipAiComponent
)

$ErrorActionPreference = "Stop"

function Resolve-IsccPath {
    $candidates = @(
        "$env:ProgramFiles(x86)\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    $found = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($found) {
        return $found.Source
    }
    throw "ISCC.exe not found. Install Inno Setup 6 or add ISCC.exe to PATH."
}

function Get-AppVersion {
    param([string]$InitFilePath)
    $content = Get-Content -LiteralPath $InitFilePath -Raw -Encoding UTF8
    $match = [regex]::Match($content, "__version__\s*=\s*""([^""]+)""")
    if (-not $match.Success) {
        throw "Could not parse app version from $InitFilePath"
    }
    return $match.Groups[1].Value
}

function Resolve-PythonPath {
    $override = $env:UBOOT_PYTHON
    if (-not [string]::IsNullOrWhiteSpace($override) -and (Test-Path -LiteralPath $override)) {
        return (Resolve-Path -LiteralPath $override).Path
    }

    $venvPython = Join-Path $repoRoot ".venv\Scripts\python.exe"
    if (Test-Path -LiteralPath $venvPython) {
        return (Resolve-Path -LiteralPath $venvPython).Path
    }

    $cmd = Get-Command python -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    throw "Python executable not found. Set UBOOT_PYTHON or create .venv."
}

function Copy-FilteredRuntime {
    param(
        [string]$SourceRuntimeDir,
        [string]$DestinationRuntimeDir
    )

    if (-not (Test-Path -LiteralPath $SourceRuntimeDir)) {
        throw "Runtime directory not found: $SourceRuntimeDir"
    }

    New-Item -ItemType Directory -Path $DestinationRuntimeDir -Force | Out-Null

    $includePatterns = @(
        "llama-completion.exe",
        "llama*.dll",
        "ggml*.dll",
        "mtmd*.dll",
        "pthread*.dll",
        "libomp*.dll",
        "vcomp*.dll",
        "vcruntime*.dll",
        "msvcp*.dll",
        "concrt*.dll"
    )

    foreach ($pattern in $includePatterns) {
        Get-ChildItem -LiteralPath $SourceRuntimeDir -Filter $pattern -File -ErrorAction SilentlyContinue |
            ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $DestinationRuntimeDir $_.Name) -Force
            }
    }

    $requiredRuntime = Join-Path $DestinationRuntimeDir "llama-completion.exe"
    if (-not (Test-Path -LiteralPath $requiredRuntime)) {
        throw "llama-completion.exe was not staged. Source runtime directory does not contain required binary."
    }
}

function Write-LlmManifest {
    param(
        [string]$ManifestPath,
        [string]$RuntimeExePath,
        [string]$ModelFilePath
    )

    $runtimeHash = (Get-FileHash -LiteralPath $RuntimeExePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $modelHash = (Get-FileHash -LiteralPath $ModelFilePath -Algorithm SHA256).Hash.ToLowerInvariant()

    $manifest = @{
        runtime = @{
            version = $env:UBOOT_LLAMA_RUNTIME_VERSION
            url = ""
            path = "llm/runtime/llama-completion.exe"
            sha256 = $runtimeHash
            installed = $true
        }
        models = @{
            better = @{
                filename = [System.IO.Path]::GetFileName($ModelFilePath)
                version = $env:UBOOT_LLM_BETTER_MODEL_VERSION
                url = ""
                path = "llm/models/$([System.IO.Path]::GetFileName($ModelFilePath))"
                installed = $true
                sha256 = $modelHash
            }
        }
    }

    if ([string]::IsNullOrWhiteSpace($manifest.runtime.version)) {
        $manifest.runtime.version = "bundled"
    }
    if ([string]::IsNullOrWhiteSpace($manifest.models.better.version)) {
        $manifest.models.better.version = "Q4_K_M_1.5B"
    }

    $manifestJson = $manifest | ConvertTo-Json -Depth 8
    Set-Content -LiteralPath $ManifestPath -Value $manifestJson -Encoding UTF8
}

$repoRoot = (Resolve-Path ".").Path
$resolvedCoreExe = (Resolve-Path -LiteralPath $CoreExePath).Path
$resolvedRuntimeDir = if ($SkipAiComponent) { $null } else { (Resolve-Path -LiteralPath $RuntimeDir).Path }
$resolvedModelPath = if ($SkipAiComponent) { $null } else { (Resolve-Path -LiteralPath $ModelPath).Path }
$resolvedOutputRoot = (New-Item -ItemType Directory -Path $OutputRoot -Force).FullName

$pyiWork = Join-Path $resolvedOutputRoot "pyinstaller-work"
$pyiDist = Join-Path $resolvedOutputRoot "pyinstaller-dist"
$payloadDir = Join-Path $resolvedOutputRoot "payload"
$installerOut = Join-Path $resolvedOutputRoot "installer"

if (Test-Path -LiteralPath $pyiWork) { Remove-Item -LiteralPath $pyiWork -Recurse -Force }
if (Test-Path -LiteralPath $pyiDist) { Remove-Item -LiteralPath $pyiDist -Recurse -Force }
if (Test-Path -LiteralPath $payloadDir) { Remove-Item -LiteralPath $payloadDir -Recurse -Force }
if (Test-Path -LiteralPath $installerOut) { Remove-Item -LiteralPath $installerOut -Recurse -Force }

New-Item -ItemType Directory -Path $pyiWork -Force | Out-Null
New-Item -ItemType Directory -Path $pyiDist -Force | Out-Null
New-Item -ItemType Directory -Path $payloadDir -Force | Out-Null
New-Item -ItemType Directory -Path $installerOut -Force | Out-Null

$appVersion = Get-AppVersion -InitFilePath (Join-Path $repoRoot "app\__init__.py")
$pythonExe = Resolve-PythonPath

Write-Host "Building GUI bundle with PyInstaller..."
& $pythonExe -m PyInstaller `
    --noconfirm `
    --clean `
    --onedir `
    --name "Uboot" `
    --distpath $pyiDist `
    --workpath $pyiWork `
    --add-data "rules\rules_v1.json;rules" `
    "app\main.py"
if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller build failed with exit code $LASTEXITCODE"
}

$bundledGuiDir = Join-Path $pyiDist "Uboot"
if (-not (Test-Path -LiteralPath $bundledGuiDir)) {
    throw "Expected PyInstaller output directory not found: $bundledGuiDir"
}

Write-Host "Staging payload directory..."
Copy-Item -Path (Join-Path $bundledGuiDir "*") -Destination $payloadDir -Recurse -Force

$payloadCoreExe = Join-Path $payloadDir "uboot-core.exe"
Copy-Item -LiteralPath $resolvedCoreExe -Destination $payloadCoreExe -Force

$payloadRulesDir = Join-Path $payloadDir "rules"
New-Item -ItemType Directory -Path $payloadRulesDir -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "rules\rules_v1.json") -Destination (Join-Path $payloadRulesDir "rules_v1.json") -Force

if (-not $SkipAiComponent) {
    Write-Host "Staging offline AI component (runtime + model)..."
    $payloadRuntimeDir = Join-Path $payloadDir "llm\runtime"
    $payloadModelsDir = Join-Path $payloadDir "llm\models"
    New-Item -ItemType Directory -Path $payloadModelsDir -Force | Out-Null
    Copy-FilteredRuntime -SourceRuntimeDir $resolvedRuntimeDir -DestinationRuntimeDir $payloadRuntimeDir
    Copy-Item -LiteralPath $resolvedModelPath -Destination (Join-Path $payloadModelsDir ([System.IO.Path]::GetFileName($resolvedModelPath))) -Force
    Write-LlmManifest `
        -ManifestPath (Join-Path $payloadDir "llm\manifest.json") `
        -RuntimeExePath (Join-Path $payloadRuntimeDir "llama-completion.exe") `
        -ModelFilePath (Join-Path $payloadModelsDir ([System.IO.Path]::GetFileName($resolvedModelPath)))
}

$guiExe = Join-Path $payloadDir "Uboot.exe"
if (-not (Test-Path -LiteralPath $guiExe)) {
    throw "Packaged GUI executable not found in payload: $guiExe"
}

if (-not $SkipInstaller) {
    Write-Host "Building signed-installer candidate with Inno Setup..."
    $isccPath = Resolve-IsccPath
    $installerScript = Join-Path $repoRoot "scripts\windows\uboot-installer.iss"
    & $isccPath `
        "/DAppVersion=$appVersion" `
        "/DSourceDir=$payloadDir" `
        "/DOutputDir=$installerOut" `
        $installerScript
    if ($LASTEXITCODE -ne 0) {
        throw "Inno Setup compilation failed with exit code $LASTEXITCODE"
    }
}

Write-Host ""
Write-Host "Distribution build complete."
Write-Host "Payload:   $payloadDir"
Write-Host "GUI EXE:   $guiExe"
Write-Host "Core EXE:  $payloadCoreExe"
if (-not $SkipInstaller) {
    Get-ChildItem -LiteralPath $installerOut -Filter "*.exe" -File | ForEach-Object {
        Write-Host "Installer: $($_.FullName)"
    }
}
