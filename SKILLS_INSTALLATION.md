# 🎯 AI Skills Installation Summary - Uboot Project

**Installation Date:** February 12, 2026  
**Project:** Uboot - Ultra-Conservative Autoruns Scanner

---

## ✅ Installation Complete

All AI skills have been successfully installed and configured for Uboot development.

## 📦 Installed Skills

### 🔒 **Custom Skills for Uboot** (4 skills)

Located in: `~\.agents\skills\`

1. **windows-security-analyzer**
   - Expert in Windows persistence mechanisms and threat detection
   - Covers: Registry, Services, Scheduled Tasks, WMI, IFEO, Winlogon
   - Use for: Security analysis, persistence detection, Windows API guidance

2. **cpp-csharp-interop**
   - C++/C# interop patterns and best practices
   - Covers: P/Invoke, COM, JSON/IPC, marshaling, memory management
   - Use for: Native/managed integration, Windows API calls from C#

3. **uboot-collector-development**
   - Guidelines for creating new Uboot collectors
   - Covers: ICollector interface, COM initialization, evidence collection
   - Use for: Implementing new persistence collectors

4. **cmake-cpp-best-practices**
   - Modern CMake 3.15+ and C++17 patterns
   - Covers: Target-centric design, dependencies, Windows libraries
   - Use for: Build configuration, project structure, optimization

### 🎨 **Standard Skills from aitmpl.com** (6 skills)

Located in: `.claude\skills\`

1. **code-reviewer** 🔴 HIGH
   - Comprehensive code review for TypeScript, JavaScript, Python, C#
   - Automated analysis and best practices

2. **git-commit-helper** 🔴 HIGH
   - Generate descriptive commit messages from git diffs
   - Essential for clean commit history

3. **senior-architect** 🟡 MEDIUM
   - Software architecture and system design guidance
   - Scalable, maintainable systems

4. **senior-backend** 🟡 MEDIUM
   - Backend development patterns
   - NodeJS, Express, Go, Python, PostgreSQL

5. **webapp-testing** 🟢 LOW
   - Playwright testing toolkit
   - Frontend functionality verification

6. **skill-creator** 🟢 LOW
   - Create new custom skills
   - Skill development guidelines

### ⚡ **Vercel Skills** (4 skills)

Located in: `.agents\skills\`

1. **vercel-react-best-practices**
   - React & Next.js performance optimization
   - 40+ rules across 8 categories
   - Useful for: Future UI work, Avalonia patterns

2. **web-design-guidelines**
   - UI/UX best practices audit
   - 100+ rules for accessibility, performance, UX
   - Useful for: Avalonia UI development

3. **vercel-composition-patterns**
   - React composition patterns
   - Scalable component architecture
   - Useful for: UI component design

4. **vercel-react-native-skills**
   - React Native best practices
   - Mobile development patterns

---

## 🎯 Skills Optimized For

- ✅ **C++ 17** (Windows APIs, CMake, RapidJSON)
- ✅ **C# 12** / .NET 8 (Avalonia, System.Text.Json)
- ✅ **Windows Security Analysis** (Persistence, Autoruns)
- ✅ **Hybrid Architecture** (C++ collectors + C# analysis)
- ✅ **Git Workflow** (Commits, code review)

---

## 📁 Skills Directory Structure

```
C:\Users\Ezequiel\
├── .agents\skills\                    # Global skills (GitHub Copilot, etc.)
│   ├── windows-security-analyzer\
│   ├── cpp-csharp-interop\
│   ├── uboot-collector-development\
│   ├── cmake-cpp-best-practices\
│   ├── vercel-react-best-practices\
│   ├── vercel-composition-patterns\
│   ├── vercel-react-native-skills\
│   └── web-design-guidelines\
│
└── Documents\GitHub\Uboot\
    ├── .agents\skills\                # Project-local skills (symlinks)
    │   └── README.md
    ├── .claude\skills\                # Claude Code skills
    │   ├── code-reviewer\
    │   ├── git-commit-helper\
    │   ├── senior-architect\
    │   ├── senior-backend\
    │   ├── webapp-testing\
    │   └── skill-creator\
    └── scripts\
        └── install-skills.ps1         # Installation script
```

---

## 🚀 Quick Commands

### List Skills
```powershell
npx skills list
```

### Find Skills
```powershell
# Interactive search
npx skills find

# Search by keyword
npx skills find security
npx skills find cmake
```

### Update Skills
```powershell
# Check for updates
npx skills check

# Update all skills
npx skills update
```

### Remove Skills
```powershell
# Interactive removal
npx skills remove

# Remove specific skill
npx skills remove skill-name
```

### Re-install Skills
```powershell
# Re-run installation script
.\scripts\install-skills.ps1

# Skip standard skills
.\scripts\install-skills.ps1 -SkipStandard

# Skip Vercel skills
.\scripts\install-skills.ps1 -SkipVercel

# List mode only
.\scripts\install-skills.ps1 -ListOnly
```

---

## 🔍 How Skills Work

Skills are automatically loaded by AI agents when:
1. **GitHub Copilot** reads from `~\.agents\skills\`
2. **Claude Code** reads from `.claude\skills\`
3. Other agents read from their respective directories

Skills provide:
- **Domain knowledge** (Windows security, C++/C# patterns)
- **Best practices** (CMake, code review)
- **Guidelines** (when to use specific approaches)
- **Code patterns** (implementation examples)

The AI agent will **automatically use** relevant skills based on:
- Your question or task
- File context (C++, C#, CMake)
- Project structure

---

## 📚 Resources

### Skill Repositories
- **aitmpl.com**: https://www.aitmpl.com/skills
- **skills.sh**: https://skills.sh/
- **Vercel Skills**: https://github.com/vercel-labs/agent-skills

### Documentation
- **Agent Skills Spec**: https://agentskills.io/
- **GitHub Copilot Skills**: https://docs.github.com/en/copilot/concepts/agents/about-agent-skills
- **Claude Code Skills**: https://code.claude.com/docs/en/skills

### Uboot-Specific
- **Custom Skills**: `.agents\skills\README.md`
- **Installation Script**: `scripts\install-skills.ps1`

---

## 💡 Tips

1. **Skills are passive**: They don't run automatically, the AI agent uses them as reference knowledge
2. **Custom skills**: Highly specific to Uboot, use these for domain-specific questions
3. **Standard skills**: General best practices, useful across all projects
4. **Update regularly**: Run `npx skills check` monthly
5. **Create more**: Use `skill-creator` skill to make new custom skills

---

## 🎓 Example Usage

**Question:** "How do I enumerate Windows services in C++?"
- AI will reference: `windows-security-analyzer` + `uboot-collector-development`

**Question:** "How to marshal a struct from C++ to C#?"
- AI will reference: `cpp-csharp-interop`

**Question:** "Configure CMake for Windows libraries"
- AI will reference: `cmake-cpp-best-practices`

**Question:** "Review my C# code for issues"
- AI will reference: `code-reviewer`

**Question:** "Generate a commit message"
- AI will reference: `git-commit-helper`

---

## ✅ Next Steps

1. ✅ Skills installed and configured
2. ✅ Custom skills created for Uboot
3. ✅ Installation script ready
4. 📚 Start using skills in your workflow
5. 🔄 Update skills periodically

**You're all set!** The AI agents now have deep knowledge about:
- Windows security mechanisms
- C++/C# hybrid development
- CMake configuration
- Code review patterns
- Git workflow

Happy coding! 🚀
