# AGENTS

## Lecciones de integración y validación (post-Fase 4)

- Antes de depurar la GUI, validar que los subcomandos invocados existen y cumplen el contrato esperado.
- Confirmar que los datos mostrados en la UI no sean placeholders antes de depurar su flujo.
- Mantener `.gitignore` actualizado y limpiar artefactos generados antes de validar entregables o marcar hitos.
- Verificar el entorno Python y dependencias antes de depurar errores de importación o tipado.
- Marcar los hitos del roadmap apenas se validen, no dejarlo para el final, para facilitar el tracking y evitar confusión.
- En features de producto, no sostener presets UX múltiples si uno de ellos no entrega una calidad claramente aceptable; simplificar la interfaz antes que mantener modos engañosos.
- Cada vez que se complete y valide una task o step del roadmap en esta sesión, actualizar `ROADMAP.md` en el mismo turno y marcarlo como completado; si quedó parcial, desdoblarlo o anotarlo explícitamente en vez de dejar un estado ambiguo.
- No marcar una task del roadmap como completada si falta una dependencia operativa crítica para usarla de verdad (por ejemplo runtime, modelo, binario externo, credenciales o instalación requerida). En esos casos, dejarla pendiente o dividirla en substeps explícitos.
- Para integración LLM en Windows con releases actuales de `llama.cpp`, preferir `llama-completion.exe` para invocación no interactiva; `llama-cli.exe` puede caer a modo chat y no aceptar bien `--no-conversation`.
- Para validación operativa del advisor local, probar con un GGUF realmente liviano (`Q4_K_M`) antes de concluir que el flujo falla por lógica; `Q8_0` puede invalidar el sizing y falsear timeouts.


## Execution Checklist for This Repo

- Validate runtime policy first: before deep debugging, run `build-vs18/bin/Release/uboot-core.exe --help` (or equivalent) to detect App Control/WDAC blocking early.
- For remediation CLI, remember argparse global options must go before subcommand: `... --pretty plan ...` not `... plan ... --pretty`.
- Use the correct core binary path after CMake build: `build-vs18/bin/Release/uboot-core.exe`.
- Keep VS Code tasks aligned with local toolchain: use `build-vs18` for CMake and PowerShell-safe Python task commands with `.venv` fallback.
- Scanner mapping must support current core fields (`key`/`displayName`/`imagePath`/`arguments`) in addition to contract fields (`entry_id`/`name`/`command`) to avoid empty GUI rows.
- Keep plan dry-run behavior non-destructive: plan must return data with `executed: false` and must not apply changes.
- Propagate operation result to process exit code in Python wrappers: if `applied`/`undone` is false, return non-zero.
- Before deleting ignored files, always run a dry-run cleanup first (`git clean -ndX`) and only remove clearly regenerable artifacts.

## Fast Gate Validation (Phase 3 Remediation)

1. Build core in Release.
2. Run remediation plan command with one entry id.
3. Confirm JSON includes `tx_id`, `operations`, and `executed: false`.
4. Run apply/undo in elevated shell for happy-path verification.
5. Confirm list includes the transaction id.
