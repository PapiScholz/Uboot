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
- [x] **GATE: `cmake --build` produce `uboot-core.exe` sin errores** — *Validado con VS 2026 generator en `build-vs18`.*
- [x] **GATE: `uboot-core.exe` devuelve JSON válido en modo collect** — *Validado con `uboot-core.exe --source all --pretty` (exit 0).*

### Notas de Fase 2
- ✅ Todos los bugs de código corregidos (BackupStore timestamp, Transaction includes, OperationFactory field names)
- ✅ cli/main.cpp y CMakeLists.txt listos
- ✅ Build local validado en Visual Studio Community 2026 (`Visual Studio 18 2026`)

## Fase 3 — Capa Python: orquestación e inteligencia

- [x] 3.1 Scaffold del paquete Python (`app/orchestrator/`)
- [x] 3.2 `scanner.py`: invoca uboot-core.exe, parsea JSON de entradas
- [x] 3.3 `evidence.py`: recopila evidencia forense via PowerShell (Get-AuthenticodeSignature, Get-FileHash) — modulo C++ evidence archivado en docs/archive/evidence_wip/
- [x] 3.4 `scoring.py`: carga rules_v1.json, produce score + classification + signals + explanation
- [x] 3.5 `remediation.py`: construye plan TX, invoca tx plan/apply/undo
- [x] 3.6 `snapshot.py`: persiste scans, diff entre sesiones (new / changed / removed)
- [x] **GATE: `python -m app.orchestrator.main` produce lista de ScoredEntry con score** — *Validado (331 entries, exit 0).* 
- [x] **GATE: `python app/orchestrator/remediation.py --core-exe build-vs18/bin/Release/uboot-core.exe plan --entry-id=<id>` muestra plan sin aplicar** — *Validado (JSON de plan devuelto con `executed: false`, exit 0).*

## Fase 4 — GUI V1: visual y funcional

- [x] 4.1 `app/main.py` + ventana principal PySide6 (toolbar, tres paneles, menú)
- [x] 4.2 Panel izquierdo: source tree + entries list (Name, Score, Classification, Source, Signed)
- [x] 4.3 Panel derecho arriba: entry detail completo
- [x] 4.4 Panel derecho abajo: botones de acción según tipo de entry
- [x] 4.5 Diálogo de confirmación de remediación (stubs)
- [x] 4.6 Threading: scan en QThread, GUI sin congelarse, progress bar
- [x] 4.7 Colorear filas por clasificación
- [x] 4.8 Filtro live por nombre o fuente
- [x] **GATE: flujo completo operable** — *Validado manualmente end-to-end en GUI (scan, filtros, details/evidence, remediación plan/undo y respuesta UI).* 

## Fase 5 — Calidad

- [x] 5.1 `tests/fixtures/`: JSONs de entradas clean, suspicious, malicious + fixtures.py
- [x] 5.2 Smoke tests Python: scanner + scoring verifican estructura de output (4/4 pasos ✓)
- [x] 5.3 Smoke test diffs: snapshot diff detection verificado
- [x] 5.4 GitHub Actions CI: checks + smoke tests + build CMake en Windows

## Fase 6 — Post-V1

- [ ] Más persistence points (AppInit DLLs, COM hijacking, Boot execute, PowerShell profiles)
- [ ] Diff en background (watchdog notifica nuevas entradas)
- [x] Advisor LLM local para explicación de evidencia y recomendación de remediación
- [x] Export forense: reporte HTML con timeline, evidencia, hashes y advisory
- [ ] Export forense: PDF derivado del HTML
- [x] Multi-scan / timeline visual
- [ ] Capacidades online opcionales (VirusTotal, WHOIS — toggle OFF por defecto)
- [ ] Suite integration: Uboot como módulo con launcher compartido

## Fase 7 — Distribución Windows firmada (setup.exe + AI embebida)

- [x] 7.1 Resolver rutas runtime para ejecución instalada (`Program Files`) en scanner/rules/llm
- [x] 7.2 Persistir snapshots en ruta de usuario (`LOCALAPPDATA\Uboot\snapshots`) para evitar escritura en Program Files
- [x] 7.3 Añadir `Help -> Check for Updates...` (manual check only, sin auto-update)
- [x] 7.4 Crear pipeline de release `scripts/windows/build-release.ps1` (PyInstaller `--onedir` + payload offline AI + compilación Inno Setup)
- [x] 7.5 Crear instalador Inno Setup `scripts/windows/uboot-installer.iss` (per-machine, Apps & Features, Start Menu, desktop opcional)
- [x] 7.6 Crear helper de firma de release `scripts/windows/sign-release.ps1` (core + GUI + setup, con timestamp)
- [x] 7.7 Documentar flujo completo de distribución y firma (`docs/WINDOWS_DISTRIBUTION.md`, README, SIGNING.md)
- [ ] 7.8 Validación en VM limpia: instalación, Better offline sin descarga, uninstall limpio, firma `Valid` en todos los binarios

## Fase 8 — Elegibilidad OSS para firma gestionada

- [x] 8.1 Crear CI en GitHub Actions para smoke tests + build reproducible de `uboot-core.exe` en Windows
- [x] 8.2 Agregar validación automática de identidad de release (versión consistente app/CMake/installer + naming `Uboot-Setup-*`)
- [x] 8.3 Agregar validación automática de visibilidad pública del repositorio en contexto GitHub
- [x] 8.4 Documentar política OSS de confianza y uso no engañoso (`docs/OSS_TRUST_POLICY.md`)
- [x] 8.5 Crear workflow de release reproducible (build+artifacts, sin firma automática en CI)
- [ ] 8.6 Integrar firma gestionada (SignPath o equivalente) cuando existan credenciales de org/proyecto
