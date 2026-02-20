# Uboot UI Wireframe

## Pantalla A — "Overview"

```text
========================================================================================
| Uboot Logo  | [Search 🔎]           | [Mode: Basic/Analyst] [ES] [Load JSON] [Diff]  |
========================================================================================
| Filters: Tags [Persistence] [Microsoft] | Sort [ScoreDesc v] | [ ] Only Triggered    |
========================================================================================
| Score | Name                 | Source        | Detail Panel (Right)                  |
|-------|----------------------|---------------|---------------------------------------|
|  [85] | UpdateAssistant      | ScheduledTask | [ Summary ]  [ Evidence ] [ Expl. ]   |
|  [ 0] | OneDrive             | RunKey        |---------------------------------------|
|  [ 0] | Skype                | RunKey        | Command / Path                        |
|       |                      |               | > C:\App\Temp\updater.exe             |
|       |                      |               |                                       |
|       |                      |               | Matched Signals                       |
|       |                      |               | ⚠️ Execution from Temp Folder (+50)   |
|       |                      |               | ⚠️ Unsigned Executable (+35)          |
|       |                      |               |                                       |
|       |                      |               | Tags                                  |
|       |                      |               | [Suspicious] [Persistence]            |
========================================================================================
| 3 entries shown                                                  Host: DESKTOP-TEST  |
========================================================================================
```

## Pantalla B — "Diff (Snapshots)"

```text
========================================================================================
| < Back     | Diff Mode                       | A: Base Snapshot  -> [Load Target...] |
========================================================================================
| 🟢 1 Added      | 🔴 1 Removed     | 🔵 1 Changed                                    |
========================================================================================
| [REMOVED] | OneDrive            | eec54a55-1                                         |
|--------------------------------------------------------------------------------------|
| [CHANGED] | Skype               | abc12345-2                                         |
|           | Path: - C:\Program Files (x86)\Skype\Phone\Skype.exe /minimized          |
|           |       + C:\Program Files (x86)\Skype\Phone\Skype.exe /background         |
|--------------------------------------------------------------------------------------|
| [ADDED]   | MaliciousService    | new11111-4                                         |
========================================================================================
```
