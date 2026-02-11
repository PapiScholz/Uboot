# Build script for uboot-collector
# Builds the C++ collector for Windows x64

param(
    [string]$Configuration = "Release",
    [switch]$Clean,
    [switch]$Run
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $scriptDir "build"
$binDir = Join-Path $buildDir "bin"

Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  uboot-collector Build Script" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Clean build directory if requested
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Path $buildDir -Recurse -Force
    Write-Host "✓ Build directory cleaned" -ForegroundColor Green
    Write-Host ""
}

# Create build directory
if (-not (Test-Path $buildDir)) {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Configure with CMake
Write-Host "Configuring project with CMake..." -ForegroundColor Yellow
Push-Location $buildDir
try {
    cmake .. -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
    Write-Host "✓ Configuration complete" -ForegroundColor Green
    Write-Host ""
    
    # Build
    Write-Host "Building $Configuration configuration..." -ForegroundColor Yellow
    cmake --build . --config $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    Write-Host "✓ Build complete" -ForegroundColor Green
    Write-Host ""
    
    # Show output location
    $exePath = Join-Path $binDir "$Configuration\uboot-collector.exe"
    if (Test-Path $exePath) {
        $fileInfo = Get-Item $exePath
        Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "  Build Output" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "  Executable: $exePath" -ForegroundColor White
        Write-Host "  Size:       $($fileInfo.Length) bytes" -ForegroundColor White
        Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host ""
        
        # Run if requested
        if ($Run) {
            Write-Host "Running uboot-collector..." -ForegroundColor Yellow
            Write-Host ""
            & $exePath --help
            Write-Host ""
        }
    } else {
        Write-Host "Warning: Executable not found at expected location" -ForegroundColor Red
    }
}
finally {
    Pop-Location
}

Write-Host "Build script completed successfully!" -ForegroundColor Green
