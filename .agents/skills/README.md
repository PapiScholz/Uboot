# Uboot AI Skills

This directory contains custom AI skills optimized for Uboot development.

## Custom Skills

### 🔒 windows-security-analyzer
**Expert in Windows persistence mechanisms, registry analysis, and autorun threat detection**

Use when:
- Analyzing Windows persistence mechanisms (Run keys, Services, Scheduled Tasks, WMI, Winlogon, IFEO)
- Implementing autorun detection and enumeration
- Understanding Windows threat vectors and attack surfaces
- Working with Windows Security APIs (Authenticode, WinTrust, CryptoAPI)

### 🔗 cpp-csharp-interop
**Best practices for C++/CLI and C#/.NET interop in Windows applications**

Use when:
- Integrating C++ libraries with .NET applications
- Calling Windows APIs from C#
- Designing hybrid native/managed architectures
- Marshaling complex data structures between C++ and C#

### ⚙️ uboot-collector-development
**Development guidelines for creating Uboot autorun collectors**

Use when:
- Implementing new persistence collectors (Scheduled Tasks, WMI, Winlogon, IFEO)
- Extending existing collectors with new features
- Adding evidence collection capabilities
- Debugging collector issues

### 🏗️ cmake-cpp-best-practices
**Modern CMake and C++17 best practices**

Use when:
- Setting up new C++ projects with CMake
- Configuring C++17/20 compilation settings
- Managing dependencies (vcpkg, FetchContent, find_package)
- Structuring multi-target CMake projects

## Installation

Custom skills are automatically loaded from this directory by GitHub Copilot and other AI agents.

To install additional standard skills:

```powershell
# Run the installation script
.\scripts\install-skills.ps1

# Or install manually
npx claude-code-templates@latest --skill=development/code-reviewer --yes
npx skills add vercel-labs/agent-skills
```

## Skill Discovery

Skills are discovered in these locations:
- `.agents/skills/` (project-specific, this directory)
- `~/.agents/skills/` (global, user-wide)
- `~/.copilot/skills/` (GitHub Copilot)
- `~/.claude/skills/` (Claude Code)

## Managing Skills

```powershell
# List installed skills
npx skills list

# Find skills
npx skills find cmake

# Update all skills
npx skills update

# Remove a skill
npx skills remove skill-name
```

## Creating New Skills

To create a new custom skill for Uboot:

1. Create a new directory under `.agents/skills/`
2. Add a `SKILL.md` file with YAML frontmatter:

```markdown
---
name: my-skill
description: What this skill does and when to use it
---

# My Skill

Instructions for the agent...
```

3. Commit the skill to the repository

## References

- [Agent Skills Specification](https://agentskills.io/)
- [Skills Directory](https://skills.sh/)
- [GitHub Copilot Skills Documentation](https://docs.github.com/en/copilot/concepts/agents/about-agent-skills)
