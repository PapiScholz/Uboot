#!/usr/bin/env pwsh
# Build script for AutorunsMVP

param(
    [string]$Configuration = "Release"
)

Write-Host "Building Uboot ($Configuration)..." -ForegroundColor Cyan

# Restore dependencies
Write-Host "`nRestoring NuGet packages..." -ForegroundColor Yellow
dotnet restore Uboot.sln
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to restore packages"
    exit 1
}

# Build solution
Write-Host "`nBuilding solution..." -ForegroundColor Yellow
dotnet build Uboot.sln -c $Configuration --no-restore
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}

# Run tests
Write-Host "`nRunning tests..." -ForegroundColor Yellow
dotnet test tests/Uboot.Tests/Uboot.Tests.csproj -c $Configuration --no-build --verbosity normal
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Tests failed or had errors"
}

Write-Host "`n✓ Build complete!" -ForegroundColor Green
Write-Host "`nArtifacts:" -ForegroundColor Cyan
Write-Host "  GUI: src\Uboot.App\bin\$Configuration\net8.0\Uboot.App.exe"
Write-Host "  CLI: src\Uboot.Cli\bin\$Configuration\net8.0\Uboot.Cli.exe"
