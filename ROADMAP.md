# Uboot — Roadmap

> **Tipo:** Autoruns/Sysinternals con scoring explicable, evidencia forense y remediación reversible.
> **Stack:** C++17 (core nativo) · Python 3.12+ (orquestación + scoring + IA) · PySide6 Qt6 (GUI)
> **Frontera C++/Python:** subprocess JSON

---

## Fase 0 — Cirugía del repo ✅

- [x] 0.1 Eliminar GEMINI.md
- [x] 0.1 Eliminar CONTRIBUTING.md (se rehace en 0.3)
- [x] 0.1 Eliminar SKILLS_INSTALLATION.md
- [x] 0.1 Eliminar .vscode/launch.json
- [x] 0.1 Eliminar .editorconfig (reglas C#)
- [x] 0.1 Eliminar carpeta build/ del tracking de git
- [x] 0.2 Actualizar .gitignore (build/, __pycache__, .venv, *.pdb, etc.)
- [x] 0.3 Reescribir README.md (qué es, stack, prerrequisitos, build, arquitectura, roadmap)
- [x] 0.3 Crear CONTRIBUTING.md nuevo (C++17 + Python 3.12, convenciones, PR flow)
- [x] 0.4 Limpiar CMakeLists.txt: quitar referencias a app/win32/*.cpp inexistentes
- [x] 0.5 Actualizar .vscode/tasks.json (tareas cmake + python reales)

## Fase 1 — Arquitectura y contratos ✅

- [x] 1.1 Definir esquema JSON del contrato CLI (scan, evidence, tx plan/apply/undo/list)
- [x] 1.2 Definir estructura de carpetas final del repo y aplicarla
- [x] 1.3 Crear docs/ARCHITECTURE.md con diagrama de capas y contratos

## Fase 2 — Core nativo: uboot-core.exe

- [x] 2.1 Fix BackupStore.cpp línea 28: formato timestamp `%Y%m%d%H%wbr%S` → `%Y%m%d%H%M%S`
- [x] 2.1 Fix BackupStore.cpp línea 188: substr hardcodeado → derivar del formato corregido
- [x] 2.1 Fix Transaction.cpp línea 2: ruta relativa `../../util/GlobalLock.h` → confirmar/ajustar
- [x] 2.1 Limpiar OperationFactory.cpp: quitar comentarios de análisis inacabados
- [x] 2.2 Crear cli/main.cpp: entrypoint C++ con subcomandos y salida JSON
- [x] 2.3 Actualizar CMakeLists.txt: compilar uboot-core.exe desde cli/main.cpp + SOURCES
- [x] 2.4 Archivar core/scoring/ en docs/archive/scoring_wip/
- ⏸️ **GATE: `cmake --build` produce `uboot-core.exe` sin errores** — *Bloqueado: falta instalar C++ Build Tools + CMake. Código listo. Ver notas abajo.*
- ⏸️ **GATE: `uboot-core.exe scan --sources=all` devuelve JSON válido** — *Bloqueado por compilación.*

### Notas de Fase 2
- ✅ Todos los bugs de código corregidos (BackupStore timestamp, Transaction includes, OperationFactory field names)
- ✅ cli/main.cpp y CMakeLists.txt listos
- ⚠️ **BLOCKER BUILD**: VS 2022 BuildTools sin C++ (v170). Necesita:
  - Reinstalar/agregar C++ workload a BuildTools, O
  - Instalar CMake + MSVC v143 standalone, O
  - Usar GitHub Actions CI para compilación remota

## Fase 3 — Capa Python: orquestación e inteligencia

- [x] 3.1 Scaffold del paquete Python (`app/orchestrator/`)
- [x] 3.2 `scanner.py`: invoca uboot-core.exe, parsea JSON de entradas
- [x] 3.3 `evidence.py`: invoca uboot-core.exe evidence por entry-id
- [x] 3.4 `scoring.py`: carga rules_v1.json, produce score + classification + signals + explanation
- [x] 3.5 `remediation.py`: construye plan TX, invoca tx plan/apply/undo
- [x] 3.6 `snapshot.py`: persiste scans, diff entre sesiones (new / changed / removed)
- [ ] **GATE: `python -m app.orchestrator.main` produce lista de ScoredEntry con score**
- [ ] **GATE: `remediation.py plan --entry-id=<id>` muestra plan sin aplicar**

## Fase 4 — GUI V1: visual y funcional

- [x] 4.1 `app/main.py` + ventana principal PySide6 (toolbar, tres paneles, menú)
- [x] 4.2 Panel izquierdo: source tree + entries list (Name, Score, Classification, Source, Signed)
- [x] 4.3 Panel derecho arriba: entry detail completo
- [x] 4.4 Panel derecho abajo: botones de acción según tipo de entry
- [x] 4.5 Diálogo de confirmación de remediación (stubs)
- [x] 4.6 Threading: scan en QThread, GUI sin congelarse, progress bar
- [x] 4.7 Colorear filas por clasificación
- [x] 4.8 Filtro live por nombre o fuente
- ⏸️ **GATE: flujo completo operable** — *Bloqueado por binario C++. GUI código listo.*

## Fase 5 — Calidad

- [x] 5.1 `tests/fixtures/`: JSONs de entradas clean, suspicious, malicious + fixtures.py
- [x] 5.2 Smoke tests Python: scanner + scoring verifican estructura de output (4/4 pasos ✓)
- [x] 5.3 Smoke test diffs: snapshot diff detection verificado
- [ ] 5.4 (opcional) GitHub Actions: cmake build + smoke tests en cada push — *Requiere build tools*

## Fase 6 — Post-V1

- [ ] Más persistence points (AppInit DLLs, COM hijacking, Boot execute, PowerShell profiles)
- [ ] Diff en background (watchdog notifica nuevas entradas)
- [ ] Scoring con LLM local para explicaciones en lenguaje natural
- [ ] Export forense: reporte HTML/PDF con timeline, evidencia, hashes
- [ ] Multi-scan / timeline visual
- [ ] Capacidades online opcionales (VirusTotal, WHOIS — toggle OFF por defecto)
- [ ] Suite integration: Uboot como módulo con launcher compartido
