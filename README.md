# Uboot

**Ultra-Conservative Autoruns Scanner** with auditable evidence and Unknown-first classification.

## 🎯 Purpose

Uboot is a minimalist, professional-grade autorun persistence scanner for Windows. It focuses on **conservative analysis** and **auditable evidence** rather than making definitive malware claims.

### Key Principles

- **Unknown-first**: Without evidence, classification defaults to Unknown
- **Conservative confidence**: Never claims 100% certainty (capped at 0.95)
- **Offline by default**: All online features (VirusTotal, WHOIS, DNS) are behind toggles
- **Admin required**: Full access needed for comprehensive scanning
- **Auditable snapshots**: JSON.gz format with full diff capabilities

## 📋 Requirements

- **OS**: Windows 10 22H2 (build 19045) or later
- **Privileges**: Administrator rights (REQUIRED, not optional)
- **.NET Runtime**: .NET 8.0
- **Optional**: VirusTotal API key for online lookups (set `VIRUSTOTAL_API_KEY` env var)

## 🚀 Quick Start

### GUI Application

```powershell
# Run with admin privileges
.\Uboot.App.exe
```

### CLI (Headless)

```powershell
# Scan and save snapshot
.\Uboot.Cli.exe scan --name "clean_baseline" --baseline

# Scan and export to JSON
.\Uboot.Cli.exe scan --name "current_scan" --output "scan.json"

# Scan and export to Markdown report
.\Uboot.Cli.exe scan --output "report.md"
```

## 🔍 Features

### Data Sources (MVP)

- ✅ Run/RunOnce Registry (HKCU/HKLM, 32/64-bit)
- ✅ Startup Folders (User/Common) with .lnk resolution
- ✅ Windows Services (userland, no drivers)
- 🚧 Scheduled Tasks (planned)
- 🚧 WMI Event Consumers (planned)
- 🚧 Winlogon Shell/Userinit (planned)
- 🚧 IFEO Debuggers (planned)

### Analysis Engine

**Heuristic Rules** (ultra-conservative):
- `FILE_NOT_FOUND`: Referenced binary doesn't exist
- `UNSIGNED_BINARY`: No digital signature (outside trusted paths)
- `INVALID_SIGNATURE`: Signature present but invalid
- `RARE_LOCATION`: Binary in uncommon directories
- `LOLBIN_USAGE`: Uses living-off-the-land binaries
- `SUSPICIOUS_ARGUMENT`: Command-line patterns
- `SNAPSHOT_DIFF_ADDED`: New entry since baseline
- `TRUSTED_PUBLISHER`: Signed by Microsoft (negative weight)
- `TRUSTED_LOCATION`: In System32/Program Files (negative weight)

**Classifications**:
- `Benign`: Strong evidence of legitimacy
- `LikelyBenign`: Moderate evidence of legitimacy
- `Unknown`: Insufficient evidence (default)
- `Suspicious`: Moderate indicators
- `NeedsReview`: Strong indicators requiring manual review

**Confidence**: 0.0 - 0.95 (never 1.00)

### Hashing & Signatures

- **Always computed**: MD5, SHA1, SHA256
- **Optional**: SHA512 (toggle in UI/CLI)
- **Authenticode**: Offline validation only (no CRL/OCSP checks)

### Snapshots & Baselines

Snapshots are stored in `%ProgramData%\Uboot\snapshots\` as compressed JSON:

```
snapshots/
  baseline_clean.json.gz           # Your baseline
  baseline_<name>.json.gz          # Named baselines
  scan_20260210_1430.json.gz       # Scan snapshots
  diff_clean_vs_20260210_1430.json.gz  # Diff results
```

**Workflow**:

1. **Create baseline** (clean system state):
   ```powershell
   # GUI: Scan → "Create Baseline"
   # CLI:
   .\AutorunsMvp.Cli.exe scan --name "baseline_clean" --baseline
   ```

2. **Perform scans** (later):
   ```powershell
   .\AutorunsMvp.Cli.exe scan --name "incident_20260215"
   ```

3. **Compare** (GUI shows diff automatically, or manual diff):
   - Added entries (new since baseline)
   - Removed entries
   - Modified entries (changed fields)

### Export Formats

- **JSON**: Full data, machine-readable
- **CSV**: Tabular format for analysis
- **Markdown**: Human-readable reports for IR/pentest documentation

### Online Features (Toggle OFF by default)

**⚠️ OFFLINE BY DEFAULT** - Enable explicitly in UI:

- **VirusTotal**: Hash/URL/Domain reputation lookup (requires API key)
- **WHOIS**: Domain registration info (TCP/43)
- **DNS**: A/AAAA record resolution

Set API key:
```powershell
$env:VIRUSTOTAL_API_KEY = "your-api-key-here"
```

## 🛠️ Development

### Build

```powershell
# Build all projects
.\scripts\build.ps1

# Or manually
dotnet build AutorunsMVP.sln -c Release
```

### Test

```powershell
dotnet test
```

### Packaging (Parcel - Windows Installer)

**Prerequisites**:
```powershell
dotnet tool install --global AvaloniaUI.Parcel.Windows
```

**Package**:
```powershell
.\scripts\package.ps1
```

This generates an NSIS installer in `dist/`.

## 📁 Project Structure

```
Uboot/
├── src/
│   ├── Uboot.Core/           # Models, interfaces, services
│   ├── Uboot.Collectors.Windows/  # Windows-specific collectors
│   ├── Uboot.Analysis/       # Heuristic analysis engine
│   ├── Uboot.Online/         # VirusTotal, WHOIS, DNS
│   ├── Uboot.App/            # Avalonia GUI (MVVM)
│   └── Uboot.Cli/            # Headless CLI
├── tests/
│   └── Uboot.Tests/          # Unit tests (xUnit)
├── scripts/
│   ├── build.ps1                   # Build script
│   ├── scan.ps1                    # Quick scan wrapper
│   └── package.ps1                 # Parcel packaging
├── .vscode/
│   └── mcp.json                    # MCP server configuration
├── Uboot.sln
├── LICENSE                         # MIT License
├── .editorconfig
├── .gitignore
└── README.md
```

## 🔐 Security & Privacy

- **No telemetry**: Zero data sent anywhere (unless online mode explicitly enabled)
- **Offline-first**: All analysis runs locally
- **Admin required**: No "read-only mode" - transparency about requirements
- **Open source**: Auditable code (MIT License)

## ⚠️ Limitations (MVP)

- Windows-only (10 22H2+)
- English/Spanish UI (more languages possible)
- Limited .lnk resolution (full COM implementation pending)
- No driver/kernel-mode persistence detection (userland only)
- Snapshot diffs are basic (field-level, not semantic)

## 🤖 MCP Integration

This project includes `.vscode/mcp.json` for Model Context Protocol servers (agent tooling):

- `@modelcontextprotocol/server-filesystem`: Workspace file access
- `@modelcontextprotocol/server-github`: GitHub integration
- `@modelcontextprotocol/server-sequential-thinking`: Planning/reasoning
- `@modelcontextprotocol/server-memory`: Context retention

**Setup**:
1. Open VS Code
2. `Ctrl+Shift+P` → "MCP: Open User/Workspace Configuration"
3. Configure desired servers (requires npx)

## 📄 License

MIT License - see [LICENSE](LICENSE)

## 🙏 Credits

Built with:
- [Avalonia UI](https://avaloniaui.net/)
- [.NET 8](https://dotnet.microsoft.com/)
- [Serilog](https://serilog.net/)
- [ReactiveUI](https://www.reactiveui.net/)

---

**⚠️ DISCLAIMER**: This tool provides **analysis and evidence**, NOT definitive "malware detection". Always perform manual review for critical decisions. Classifications are heuristic-based and conservative by design.
