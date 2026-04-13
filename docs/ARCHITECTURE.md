# Architecture

## Layer diagram

```
┌─────────────────────────────────────────────────────────────────┐
│  GUI — app/main.py (PySide6 Qt6)                                │
│  views/, widgets/, threads/                                     │
└───────────────────────────────┬─────────────────────────────────┘
                                │ Python calls / QThread
┌───────────────────────────────▼─────────────────────────────────┐
│  Orchestrator — app/orchestrator/                               │
│  scanner.py · evidence.py · scoring.py                          │
│  remediation.py · snapshot.py                                   │
└───────────────────────────────┬─────────────────────────────────┘
                                │ subprocess + stdout JSON
┌───────────────────────────────▼─────────────────────────────────┐
│  uboot-core.exe  (C++17)                                        │
│  cli/main.cpp → CollectorRunner → JsonWriter → stdout           │
│  core/enumerators/  core/evidence/  core/tx/  core/backup/      │
└─────────────────────────────────────────────────────────────────┘
```

---

## C++/Python boundary

The Python layer starts `uboot-core.exe` as a subprocess and reads newline-delimited JSON from its stdout. No COM bindings, no ctypes, no shared memory — just a process call with a versioned JSON contract.

```python
import subprocess, json

proc = subprocess.run(
    ["uboot-core.exe", "--source", "all", "--schema-version", "1.1"],
    capture_output=True, text=True, encoding="utf-8", check=True
)
data = json.loads(proc.stdout)
```

---

## JSON output schema (schema_version: "1.1")

### collect command (default)

```json
{
  "schema_version": "1.1",
  "generated_at": "2026-04-13T10:00:00Z",
  "entry_count": 42,
  "error_count": 0,
  "entries": [
    {
      "source":      "RunRegistry",
      "scope":       "Machine",
      "key":         "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run\\MyApp",
      "location":    "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
      "key_name":    "MyApp",
      "image_path":  "C:\\Program Files\\MyApp\\myapp.exe",
      "arguments":   "--silent",
      "display_name": null,
      "publisher":   "MyApp Inc.",
      "description": null
    }
  ],
  "errors": [
    {
      "source":  "WmiPersistenceCollector",
      "message": "Access denied"
    }
  ]
}
```

### Sources

| source value | Collector |
|---|---|
| `all` | All collectors |
| `run` | Run / RunOnce registry keys |
| `services` | Windows Services |
| `tasks` | Scheduled Tasks |
| `startup` | Startup Folder |
| `winlogon` | Winlogon values |
| `wmi` | WMI subscriptions |
| `ifeo` | IFEO debugger hijacks |

### CLI flags

```
uboot-core.exe [collect] [--source <name>] [--out <path>] [--pretty] [--schema-version <v>]
```

- `--source` can be repeated; default is `all`
- `--out` path writes to file instead of stdout
- `--pretty` enables human-readable JSON indentation

---

## Scoring contract (Python — scoring.py)

Input: one `Entry` dict from the collect output.
Output:

```json
{
  "entry_key": "HKLM\\...\\MyApp",
  "score": 72,
  "classification": "suspicious",
  "signals": [
    {"id": "unsigned_binary",   "weight": 30, "fired": true},
    {"id": "path_in_temp",      "weight": 40, "fired": false},
    {"id": "known_good_vendor", "weight": -20, "fired": false}
  ],
  "explanation": "Binary is unsigned and located outside expected system paths."
}
```

Classifications: `clean` (0–30) · `suspicious` (31–70) · `malicious` (71–100)

---

## Transaction contract (Python — remediation.py)

```
plan   → dry-run, returns list of operations with descriptions
apply  → executes the plan, returns tx_id
undo   → reverts by tx_id
list   → lists all recorded transactions
```

Each operation is reversible. A backup snapshot is written before any apply.
