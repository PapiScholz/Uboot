#!/usr/bin/env pwsh
# Script de instalación de AI Skills para el proyecto Uboot
# Instala skills personalizadas y estándar para desarrollo C++, C#, .NET, Windows Security

param(
    [switch]$SkipStandard,
    [switch]$SkipVercel,
    [switch]$ListOnly
)

Write-Host @"
╔═══════════════════════════════════════════════════════════╗
║   Uboot AI Skills Installer                              ║
║   Installing development skills for C++/C# hybrid project ║
╚═══════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# Check if npx is available
$npxPath = Get-Command npx -ErrorAction SilentlyContinue
if (-not $npxPath) {
    Write-Host "`n⚠️  npx not found. Installing Node.js is required." -ForegroundColor Yellow
    Write-Host "   Download from: https://nodejs.org/" -ForegroundColor Yellow
    Write-Host "`n   After installing Node.js, run this script again." -ForegroundColor Yellow
    exit 1
}

Write-Host "`n✓ npx found: $($npxPath.Source)" -ForegroundColor Green

# Custom Skills (already created in .agents/skills)
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "📦 CUSTOM SKILLS (Local)" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

$customSkills = @(
    "windows-security-analyzer"
    "cpp-csharp-interop"
    "uboot-collector-development"
    "cmake-cpp-best-practices"
)

foreach ($skill in $customSkills) {
    $skillPath = "$env:USERPROFILE\.agents\skills\$skill\SKILL.md"
    if (Test-Path $skillPath) {
        Write-Host "  ✓ $skill" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $skill (missing)" -ForegroundColor Red
    }
}

if ($ListOnly) {
    Write-Host "`nListing mode - no installation performed." -ForegroundColor Yellow
    exit 0
}

# Standard Skills from aitmpl.com (Claude Code Templates)
if (-not $SkipStandard) {
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "🎨 STANDARD SKILLS (aitmpl.com)" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

    $standardSkills = @(
        @{
            Name = "code-reviewer"
            Category = "development"
            Priority = "🔴 HIGH"
            Description = "Code review for C#/TypeScript/Python"
        },
        @{
            Name = "git-commit-helper"
            Category = "development"
            Priority = "🔴 HIGH"
            Description = "Generate descriptive commit messages"
        },
        @{
            Name = "senior-architect"
            Category = "development"
            Priority = "🟡 MEDIUM"
            Description = "Software architecture & system design"
        },
        @{
            Name = "senior-backend"
            Category = "development"
            Priority = "🟡 MEDIUM"
            Description = "Backend development patterns"
        },
        @{
            Name = "webapp-testing"
            Category = "development"
            Priority = "🟢 LOW"
            Description = "Playwright testing toolkit"
        },
        @{
            Name = "skill-creator"
            Category = "development"
            Priority = "🟢 LOW"
            Description = "Create custom skills"
        }
    )

    foreach ($skill in $standardSkills) {
        Write-Host "`n$($skill.Priority) Installing: $($skill.Name)" -ForegroundColor White
        Write-Host "   $($skill.Description)" -ForegroundColor DarkGray
        
        try {
            $cmd = "npx claude-code-templates@latest --skill=$($skill.Category)/$($skill.Name) --yes"
            Write-Host "   Executing: $cmd" -ForegroundColor DarkGray
            
            Invoke-Expression $cmd
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "   ✓ Installed successfully" -ForegroundColor Green
            } else {
                Write-Host "   ⚠️  Installation completed with warnings" -ForegroundColor Yellow
            }
        } catch {
            Write-Host "   ✗ Failed to install: $_" -ForegroundColor Red
        }
        
        Start-Sleep -Milliseconds 500
    }
} else {
    Write-Host "`n⏭️  Skipping standard skills (--SkipStandard)" -ForegroundColor Yellow
}

# Vercel Skills
if (-not $SkipVercel) {
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "⚡ VERCEL SKILLS (vercel-labs/agent-skills)" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    
    Write-Host "`nInstalling Vercel's curated skill collection..." -ForegroundColor White
    Write-Host "  - react-best-practices: Performance optimization" -ForegroundColor DarkGray
    Write-Host "  - web-design-guidelines: UI/UX best practices" -ForegroundColor DarkGray
    Write-Host "  - composition-patterns: React patterns" -ForegroundColor DarkGray
    
    try {
        $cmd = "npx skills add vercel-labs/agent-skills --yes"
        Write-Host "`nExecuting: $cmd" -ForegroundColor DarkGray
        
        Invoke-Expression $cmd
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "`n✓ Vercel skills installed successfully" -ForegroundColor Green
        } else {
            Write-Host "`n⚠️  Installation completed with warnings" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "`n✗ Failed to install Vercel skills: $_" -ForegroundColor Red
        Write-Host "   You can install manually with: npx skills add vercel-labs/agent-skills" -ForegroundColor Yellow
    }
} else {
    Write-Host "`n⏭️  Skipping Vercel skills (--SkipVercel)" -ForegroundColor Yellow
}

# Summary
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "📊 INSTALLATION COMPLETE" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

Write-Host "`n✅ Skills installed for Uboot development!" -ForegroundColor Green

Write-Host "`n📝 Next Steps:" -ForegroundColor Yellow
Write-Host "  1. List installed skills:" -ForegroundColor White
Write-Host "     npx skills list" -ForegroundColor DarkGray
Write-Host "`n  2. Check for updates:" -ForegroundColor White
Write-Host "     npx skills check" -ForegroundColor DarkGray
Write-Host "`n  3. Update all skills:" -ForegroundColor White
Write-Host "     npx skills update" -ForegroundColor DarkGray
Write-Host "`n  4. Find more skills:" -ForegroundColor White
Write-Host "     npx skills find security" -ForegroundColor DarkGray
Write-Host "     https://www.aitmpl.com/skills" -ForegroundColor DarkGray
Write-Host "     https://skills.sh/" -ForegroundColor DarkGray

Write-Host "`n💡 Custom Skills Location:" -ForegroundColor Cyan
Write-Host "   $env:USERPROFILE\.agents\skills\" -ForegroundColor White

Write-Host "`n🎯 Skills Optimized For:" -ForegroundColor Cyan
Write-Host "   • C++ 17 (Windows APIs, CMake)" -ForegroundColor White
Write-Host "   • C# 12 / .NET 8" -ForegroundColor White
Write-Host "   • Windows Security Analysis" -ForegroundColor White
Write-Host "   • Avalonia UI Development" -ForegroundColor White
Write-Host "   • Git Workflow & Code Review" -ForegroundColor White

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan
