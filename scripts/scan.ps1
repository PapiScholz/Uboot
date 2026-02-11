#!/usr/bin/env pwsh
# Quick scan wrapper script

param(
    [string]$Name = "scan_$(Get-Date -Format 'yyyyMMdd_HHmm')",
    [string]$Output = "",
    [switch]$Baseline
)

$cliPath = "src\Uboot.Cli\bin\Release\net8.0\Uboot.Cli.exe"

if (-not (Test-Path $cliPath)) {
    Write-Error "CLI not found. Run .\scripts\build.ps1 first."
    exit 1
}

# Check admin
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "This script requires Administrator privileges. Please run as Admin."
    exit 1
}

Write-Host "Starting scan: $Name" -ForegroundColor Cyan

$args = @("scan", "--name", $Name)
if ($Baseline) {
    $args += "--baseline"
}
if ($Output) {
    $args += "--output", $Output
}

& $cliPath @args
