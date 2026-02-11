# uboot-collector

Windows-only C++ CLI tool for collecting boot/startup entry information.

## Architecture

```
uboot-collector/
├── main.cpp              # Entry point with exit code handling
├── model/                # Data models
│   ├── Entry.h          # Boot entry structure
│   └── CollectorError.h # Error encapsulation
├── collectors/          # Collector implementations
│   ├── ICollector.h     # Base interface
│   ├── RunRegistryCollector.h
│   ├── ServicesCollector.h
│   ├── StartupFolderCollector.h
│   └── ScheduledTasksCollector.h
├── runner/              # Orchestration
│   ├── CollectorRunner.h
│   └── CollectorRunner.cpp
├── json/                # JSON serialization
│   ├── JsonWriter.h
│   └── JsonWriter.cpp
├── util/                # Utilities
│   ├── CliArgs.h
│   └── CliArgs.cpp
├── normalize/           # (Reserved for future use)
└── resolve/             # (Reserved for future use)
```

## Features

- **Modular collectors**: Each data source is a separate collector
- **Deterministic JSON**: Same input always produces identical output
- **UTF-8 support**: Proper encoding via RapidJSON
- **Error handling**: No exceptions to main, all errors encapsulated
- **Exit codes**:
  - `0` - Success
  - `2` - Invalid arguments
  - `3` - Fatal initialization error
  - `4` - No sources executed

## Building

### Requirements

- Windows 10/11 (x64)
- Visual Studio 2019+ with C++ desktop development
- CMake 3.15+
- Git (for RapidJSON dependency)

### Build Instructions

```powershell
# Using build script
.\build-collector.ps1

# Or manually
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Build Output

Binary: `build/bin/uboot-collector.exe`

## Usage

```bash
# Collect all sources to stdout
uboot-collector.exe

# Collect specific source to file
uboot-collector.exe --source RunRegistry --out snapshot.json

# Collect multiple sources with pretty output
uboot-collector.exe -s Services -s StartupFolder --pretty --out output.json

# All sources with schema version
uboot-collector.exe --source all --schema-version 2.0 -o snapshot.json
```

## JSON Output Format

```json
{
  "schemaVersion": "1.0",
  "timestamp": "2026-02-10T12:34:56Z",
  "entries": [
    {
      "source": "RunRegistry",
      "scope": "Machine",
      "key": "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run\\Example",
      "location": "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
      "arguments": "",
      "imagePath": "C:\\Program Files\\Example\\app.exe",
      "keyName": "Example",
      "displayName": "Example Application"
    }
  ],
  "errors": []
}
```

## Current Status

**Phase 1: Skeleton (Complete)**
- ✅ Compilable structure
- ✅ CLI argument parsing
- ✅ JSON writer with deterministic output
- ✅ CollectorRunner framework
- ✅ Stub collectors (return empty)
- ✅ Exit code handling

**Phase 2: Implementation (Pending)**
- ⏳ RunRegistry collector
- ⏳ Services collector
- ⏳ StartupFolder collector
- ⏳ ScheduledTasks collector
- ⏳ Path normalization
- ⏳ Path resolution

**Future Enhancements**
- Command normalization
- Additional data sources
- Performance optimizations

## Notes

- No hashing or signature verification (by design)
- No malware detection (collection only)
- All errors are captured, never thrown to main
- JSON property order is stable for deterministic output
