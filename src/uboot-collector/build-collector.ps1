# Build script for uboot-collector

param(
    [string]$Configuration = "Release",
    [switch]$Clean,
    [switch]$Run
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $scriptDir "build"
$binDir = Join-Path $buildDir "bin"

Write-Host "-----------------------------------------------" -ForegroundColor Cyan
Write-Host "  uboot-collector Build Script" -ForegroundColor Cyan
Write-Host "-----------------------------------------------" -ForegroundColor Cyan
Write-Host ""

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Path $buildDir -Recurse -Force
    Write-Host "Done cleaning." -ForegroundColor Green
    Write-Host ""
}

if (-not (Test-Path $buildDir)) {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Write-Host "Configuring project with CMake..." -ForegroundColor Yellow
Push-Location $buildDir
try {
    cmake ..
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    Write-Host "Configuration complete" -ForegroundColor Green
    Write-Host ""

    Write-Host "Building $Configuration..." -ForegroundColor Yellow
    cmake --build . --config $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    Write-Host "Build complete" -ForegroundColor Green
    Write-Host ""

    $exePath = Join-Path $binDir "$Configuration\uboot-collector.exe"
    if (Test-Path $exePath) {
        Write-Host "-----------------------------------------------" -ForegroundColor Cyan
        Write-Host "  Build Output" -ForegroundColor Cyan
        Write-Host "-----------------------------------------------" -ForegroundColor Cyan
        Write-Host "  Executable: $exePath" -ForegroundColor White
        Write-Host "-----------------------------------------------" -ForegroundColor Cyan
        Write-Host ""

        if ($Run) {
            Write-Host "Running uboot-collector..." -ForegroundColor Yellow
            & $exePath --help
        }
    }
    else {
        Write-Host "Warning: Executable not found" -ForegroundColor Red
    }
}
finally {
    Pop-Location
}

Write-Host "Script completed." -ForegroundColor Green
