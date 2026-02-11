#!/usr/bin/env pwsh
# Packaging script using Avalonia Parcel

param(
    [string]$Configuration = "Release"
)

Write-Host "Packaging Uboot with Parcel..." -ForegroundColor Cyan

# Check if Parcel is installed
$parcelInstalled = dotnet tool list -g | Select-String "avaloniaui.parcel.windows"
if (-not $parcelInstalled) {
    Write-Host "Installing Avalonia Parcel..." -ForegroundColor Yellow
    dotnet tool install --global AvaloniaUI.Parcel.Windows
}

# Build first
Write-Host "`nBuilding project..." -ForegroundColor Yellow
dotnet build src\Uboot.App\Uboot.App.csproj -c $Configuration
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}

# Create dist directory
$distDir = "dist"
if (-not (Test-Path $distDir)) {
    New-Item -ItemType Directory -Path $distDir | Out-Null
}

# Package with Parcel
Write-Host "`nPackaging with Parcel..." -ForegroundColor Yellow
$appDir = "src\Uboot.App\bin\$Configuration\net8.0"

# Create installer using NSIS
# Note: This requires Parcel configuration - for MVP, we document the command
Write-Host "`nTo create installer, run:" -ForegroundColor Cyan
Write-Host "  parcel pack -p $appDir -o $distDir --installer nsis" -ForegroundColor White

Write-Host "`n✓ Packaging preparation complete!" -ForegroundColor Green
Write-Host "`nManual step required:" -ForegroundColor Yellow
Write-Host "  1. Install AvaloniaUI.Parcel.Windows: dotnet tool install -g AvaloniaUI.Parcel.Windows"
Write-Host "  2. Run: parcel pack -p $appDir -o $distDir --installer nsis"
