# Contributing to Uboot

Thank you for your interest in contributing to Uboot!

## Development Setup

1. **Prerequisites**:
   - Windows 10 22H2+ (build 19045+)
   - .NET 8 SDK
   - Visual Studio 2022 or VS Code with C# DevKit
   - Administrator privileges for testing

2. **Clone and Build**:
   ```powershell
   git clone <repository-url>
   cd AutorunsMVP
   .\scripts\build.ps1
   ```

3. **Run Tests**:
   ```powershell
   dotnet test
   ```

## Architecture

- **Uboot.Core**: Domain models, interfaces, core services (platform-agnostic)
- **Uboot.Collectors.Windows**: Windows-specific collectors (registry, services, etc.)
- **Uboot.Analysis**: Heuristic analysis engine (conservative, evidence-based)
- **Uboot.Online**: Online services (VirusTotal, WHOIS, DNS) - toggle OFF by default
- **Uboot.App**: Avalonia UI (MVVM pattern)
- **Uboot.Cli**: Command-line interface

## Coding Guidelines

- **C# 12**: Use file-scoped namespaces, top-level statements where appropriate
- **Nullable**: Enable nullable reference types (`<Nullable>enable</Nullable>`)
- **Async/Await**: Use for I/O operations
- **Logging**: Use Serilog for all logging (never Console.WriteLine in libraries)
- **DI**: Use Microsoft.Extensions.DependencyInjection

## Principles

1. **Conservative**: Default to Unknown classification without evidence
2. **Offline-first**: All online features must be behind toggles (OFF by default)
3. **Evidence-based**: Never claim definitiveness - provide evidence, let users decide
4. **Privacy**: No telemetry, no data collection without explicit user consent
5. **Admin transparency**: Require admin upfront, don't degrade to "read-only"

## Adding a New Collector

1. Create class implementing `ICollector` in `Uboot.Collectors.Windows/Collectors/`
2. Return `List<Entry>` with normalized commands (use `CommandNormalizer`)
3. Compute hashes/signatures if file paths are available
4. Register in DI container (`Program.cs`)
5. Add tests in `AutorunsMvp.Tests`

## Adding Analysis Rules

Edit `Uboot.Analysis/AnalysisEngine.cs`:
1. Add new `Evidence` with unique `RuleId`
2. Assign weight (-1.0 to +1.0)
3. Provide ES/EN description
4. Update tests to cover new rule

## Pull Request Process

1. Fork the repository
2. Create feature branch (`feature/my-new-feature`)
3. Write tests for new functionality
4. Ensure all tests pass
5. Update README if needed
6. Submit PR with clear description

## Code of Conduct

- Be respectful and professional
- Focus on technical merit
- Welcome constructive feedback
- Keep security in mind (this is a security tool)

## Security

If you discover a security vulnerability, please email [maintainer email] instead of opening a public issue.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
