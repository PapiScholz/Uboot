<#
.SYNOPSIS
    Gestiona perfiles CLAUDE.md para el proyecto Uboot.

.DESCRIPTION
    Descarga (si no existen localmente) y activa un perfil CLAUDE.md
    según el tipo de tarea que vas a realizar.

.PARAMETER Profile
    Perfil a activar. Valores válidos:
      coding    - Desarrollo, code review, debugging (recomendado por defecto)
      agents    - Pipelines de automatización, multi-agent
      analysis  - Análisis de datos, investigación, reportes
      benchmark - Benchmarks de código
      v5        - Config multi-archivo estructurado (proyectos complejos)
      v6        - Ejecución one-shot, sin iteraciones (K-drona23-v6)
      v8        - Ultra-lean, mínimo de tool calls, alta eficiencia de costo (M-drona23-v8)
      base      - CLAUDE.md universal base del repo

.PARAMETER List
    Muestra los perfiles disponibles y cuál está activo.

.PARAMETER Status
    Muestra el perfil activo actual.

.EXAMPLE
    .\scripts\windows\claude-profile.ps1 coding
    .\scripts\windows\claude-profile.ps1 -Profile agents
    .\scripts\windows\claude-profile.ps1 -List
#>

param(
    [Parameter(Position = 0)]
    [ValidateSet("coding", "agents", "analysis", "benchmark", "v5", "v6", "v8", "base")]
    [string]$Profile,

    [switch]$List,
    [switch]$Status
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot    = $PSScriptRoot | Split-Path | Split-Path
$ProfilesDir = Join-Path $RepoRoot ".claude-profiles"
$TargetFile  = Join-Path $RepoRoot "CLAUDE.md"
$BaseUrl     = "https://raw.githubusercontent.com/drona23/claude-token-efficient/main"

# Mapa: nombre amigable -> ruta en repo remoto y archivo local
$ProfileMap = @{
    "coding"    = @{ Remote = "profiles/CLAUDE.coding.md";    Local = "CLAUDE.coding.md";    Desc = "Desarrollo, code review, debugging" }
    "agents"    = @{ Remote = "profiles/CLAUDE.agents.md";    Local = "CLAUDE.agents.md";    Desc = "Pipelines automatizacion, multi-agent" }
    "analysis"  = @{ Remote = "profiles/CLAUDE.analysis.md";  Local = "CLAUDE.analysis.md";  Desc = "Analisis de datos, investigacion, reportes" }
    "benchmark" = @{ Remote = "profiles/CLAUDE.benchmark.md"; Local = "CLAUDE.benchmark.md"; Desc = "Benchmarks de codigo token-efficiency" }
    "v5"        = @{ Remote = "profiles/J-drona23-v5/CLAUDE.md"; Local = "CLAUDE.v5.md"; Desc = "Multi-archivo estructurado (proyectos complejos, 50 calls)" }
    "v6"        = @{ Remote = "profiles/K-drona23-v6/CLAUDE.md"; Local = "CLAUDE.v6.md"; Desc = "One-shot execution, sin iteraciones (50 calls)" }
    "v8"        = @{ Remote = "profiles/M-drona23-v8/CLAUDE.md"; Local = "CLAUDE.v8.md"; Desc = "Ultra-lean, minimo turns, alta eficiencia (20 calls)" }
    "base"      = @{ Remote = "CLAUDE.md";                    Local = "CLAUDE.base.md";  Desc = "Universal base (cualquier proyecto)" }
}

function Get-ActiveProfileName {
    if (-not (Test-Path $TargetFile)) { return $null }
    $content = Get-Content $TargetFile -Raw -ErrorAction SilentlyContinue
    foreach ($name in $ProfileMap.Keys) {
        $localPath = Join-Path $ProfilesDir $ProfileMap[$name].Local
        if (Test-Path $localPath) {
            $cached = Get-Content $localPath -Raw -ErrorAction SilentlyContinue
            if ($content -eq $cached) { return $name }
        }
    }
    return "custom"
}

function Show-Status {
    $active = Get-ActiveProfileName
    if ($null -eq $active) {
        Write-Host "  CLAUDE.md: no encontrado en raiz del proyecto" -ForegroundColor Yellow
    } elseif ($active -eq "custom") {
        Write-Host "  CLAUDE.md: activo (contenido personalizado, no coincide con ningun perfil cacheado)" -ForegroundColor Cyan
    } else {
        $desc = $ProfileMap[$active].Desc
        Write-Host "  Perfil activo: $active  --  $desc" -ForegroundColor Green
    }
}

function Show-List {
    Write-Host ""
    Write-Host "Perfiles disponibles:" -ForegroundColor Cyan
    Write-Host "---------------------"
    $active = Get-ActiveProfileName
    foreach ($name in ($ProfileMap.Keys | Sort-Object)) {
        $localPath = Join-Path $ProfilesDir $ProfileMap[$name].Local
        $cached    = if (Test-Path $localPath) { "[cacheado]" } else { "[sin cache]" }
        $marker    = if ($name -eq $active) { " <-- ACTIVO" } else { "" }
        Write-Host ("  {0,-12} {1,-12} {2}{3}" -f $name, $cached, $ProfileMap[$name].Desc, $marker)
    }
    Write-Host ""
    Write-Host "Uso: .\scripts\windows\claude-profile.ps1 <perfil>" -ForegroundColor DarkGray
    Write-Host ""
}

function Install-Profile([string]$name) {
    $entry     = $ProfileMap[$name]
    $localPath = Join-Path $ProfilesDir $entry.Local
    $remoteUrl = "$BaseUrl/$($entry.Remote)"

    # Crear directorio de cache si no existe
    if (-not (Test-Path $ProfilesDir)) {
        New-Item -ItemType Directory -Path $ProfilesDir | Out-Null
        Write-Host "  Creado: .claude-profiles/" -ForegroundColor DarkGray
    }

    # Descargar solo si no está en cache
    if (-not (Test-Path $localPath)) {
        Write-Host "  Descargando perfil '$name' desde GitHub..." -ForegroundColor DarkGray
        try {
            Invoke-WebRequest -Uri $remoteUrl -OutFile $localPath -UseBasicParsing
            Write-Host "  Guardado en: .claude-profiles\$($entry.Local)" -ForegroundColor DarkGray
        } catch {
            Write-Error "Error descargando perfil '$name': $_"
            exit 1
        }
    } else {
        Write-Host "  Cache encontrado: .claude-profiles\$($entry.Local)" -ForegroundColor DarkGray
    }

    # Copiar como CLAUDE.md activo
    Copy-Item -Path $localPath -Destination $TargetFile -Force
    Write-Host ""
    Write-Host "  Perfil activado: $name" -ForegroundColor Green
    Write-Host "  $($entry.Desc)" -ForegroundColor White
    Write-Host ""
    Write-Host "  Primeras 5 lineas del CLAUDE.md activo:" -ForegroundColor DarkGray
    Get-Content $TargetFile -TotalCount 5 | ForEach-Object { Write-Host "  | $_" -ForegroundColor DarkGray }
    Write-Host ""
}

# --- Main ---
if ($Status) {
    Show-Status
    exit 0
}

if ($List -or (-not $Profile -and -not $Status)) {
    Show-List
    Show-Status
    exit 0
}

Install-Profile $Profile
