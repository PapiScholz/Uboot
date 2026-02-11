# 🗂️ Mapa Visual de Archivos

## Árbol de Directorios

```
uboot/
└── src/
    └── uboot-collector/                    [ROOT]
        │
        ├─ 🔵 normalize/                    [NORMALIZACIÓN]
        │  ├─ CommandNormalizer.h           [API: 120 líneas]
        │  ├─ CommandNormalizer.cpp         [IMPL: 850+ líneas]
        │  └─ README.md                     [GUÍA DETALLADA]
        │
        ├─ 🟢 resolve/                      [RESOLUCIÓN .LNK]
        │  ├─ LnkResolver.h                 [API COM: 50 líneas]
        │  └─ LnkResolver.cpp               [IMPL: 140+ líneas]
        │
        ├─ 🟣 util/                         [UTILIDADES]
        │  ├─ CliArgs.h|cpp                 [ANTERIOR]
        │  ├─ CommandResolver.h             [API: 45 líneas] ← NUEVO
        │  └─ CommandResolver.cpp           [IMPL: 70+ líneas] ← NUEVO
        │
        ├─ 🟠 collectors/                   [COLLECTORS EXISTENTES]
        │  ├─ ICollector.h
        │  ├─ RunRegistryCollector.h        [← INTEGRAR AQUÍ]
        │  ├─ StartupFolderCollector.h      [← INTEGRAR AQUÍ]
        │  ├─ ServicesCollector.h           [← INTEGRAR AQUÍ]
        │  └─ ScheduledTasksCollector.h     [← INTEGRAR AQUÍ]
        │
        ├─ 🟡 model/                        [ESTRUCTURAS]
        │  ├─ Entry.h
        │  └─ CollectorError.h
        │
        ├─ 📚 Documentación
        │  ├─ QUICK_START.md                [✨ LEE ESTO PRIMERO]
        │  ├─ COMPLETION_SUMMARY.md         [RESUMEN ESTE ARCHIVO]
        │  ├─ normalize/README.md           [GUÍA API COMPLETA]
        │  ├─ ARCHITECTURE.md               [PATRONES INTERNOS]
        │  ├─ COMPILATION.md                [COMPILACIÓN]
        │  ├─ IMPLEMENTATION_SUMMARY.md     [RESUMEN EJECUTIVO]
        │  └─ INDEX.md                      [ÍNDICE REFERENCIAS]
        │
        ├─ 🧪 Tests & Ejemplos
        │  ├─ test_normalizer.cpp           [15 TEST CASES]
        │  └─ INTEGRATION_EXAMPLES.cpp      [6 PATRONES]
        │
        └─ ⚙️  Config
           ├─ CMakeLists.txt                 [✏️ ACTUALIZADO]
           ├─ build-collector.ps1            [BUILD SCRIPT]
           └─ .gitignore

```

---

## Flujo de Datos

```
┌──────────────────────────────────────────────────────────────┐
│  USAGE FLOW                                                  │
└──────────────────────────────────────────────────────────────┘

USER CODE (en Collector)
    │
    ├─ if (.lnk file)
    │  └─→ CommandResolver::ResolveCommand()  
    │       └─→ LnkResolver::Resolve()
    │           └─→ IShellLinkW (COM)
    │               ├─→ targetPath
    │               ├─→ arguments
    │               └─→ workingDirectory
    │
    └─ if (command string)
       └─→ CommandResolver::ResolveCommand()
           └─→ CommandNormalizer::Normalize()
               ├─→ SanitizeWhitespace()
               ├─→ ExpandEnvironmentVariables()
               ├─→ ParseCommandLine()
               ├─→ CanonicalizePath()
               ├─→ PeelWrappers()
               │   ├─→ cmd.exe detection
               │   ├─→ powershell detection
               │   ├─→ wscript detection
               │   ├─→ rundll32 detection
               │   ├─→ mshta detection
               │   ├─→ regsvr32 detection
               │   └─→ multi-pass (max 3)
               └─→ Return: NormalizationResult
                   ├─→ resolvedPath (UTF-8)
                   ├─→ arguments (UTF-8)
                   ├─→ originalCommand
                   ├─→ isComplete (bool)
                   └─→ resolveNotes (UTF-8)

OUTPUT → Entry::imagePath + Entry::arguments
```

---

## Dependencias de Compilación

```
┌─────────────────────────────────────────────────────────────┐
│  COMPILATION DEPENDENCIES                                   │
└─────────────────────────────────────────────────────────────┘

CommandNormalizer.cpp:
  ├─ windows.h          [core Win32]
  ├─ shlwapi.h          [Path functions]
  ├─ pathcch.h          [Path canonicalization]
  └─ libs: pathcch.lib, shlwapi.lib

LnkResolver.cpp:
  ├─ windows.h          [core Win32]
  ├─ shellapi.h         [Shell functions]
  ├─ shlobj.h           [Shell objects]
  └─ libs: ole32.lib, oleaut32.lib, shell32.lib

CommandResolver.cpp:
  ├─ CommandNormalizer.h
  ├─ LnkResolver.h
  └─ Entry.h

CMakeLists.txt → all libs automatically linked
```

---

## Relaciones entre Módulos

```
┌─────────────────────────────────────────────────────────────┐
│                     CommandResolver                         │
│  (Unified Interface - Automatic .lnk vs. command detection)  │
└────────────────────┬─────────────────────────────────────────┘
                     │
         ┌───────────┴───────────────────┐
         │                               │
         ▼                               ▼
┌──────────────────────┐      ┌──────────────────────┐
│ CommandNormalizer    │      │  LnkResolver (COM)   │
│ ─────────────────    │      │  ──────────────────  │
│ • Parsing            │      │ • IShellLinkW        │
│ • Canonicalization   │      │ • IPersistFile       │
│ • Wrapper peeling    │      │ • GetPath()          │
│ • Env expansion      │      │ • GetArguments()     │
└──────────────────────┘      └──────────────────────┘
         │                               │
         └───────────┬───────────────────┘
                     │
                     ▼
          ┌────────────────────────┐
          │ NormalizationResult    │
          │ ──────────────────────┤
          │ • resolvedPath (UTF-8) │
          │ • arguments (UTF-8)    │
          │ • isComplete           │
          │ • resolveNotes         │
          └────────────────────────┘
```

---

## Documentación Conectada

```
START HERE
    │
    └─→ QUICK_START.md                [5 min]
         │
         ├─→ compile & run test_normalizer.exe
         │
         ├─→ INTEGRATION_EXAMPLES.cpp   [patterns]
         │
         └─→ normalize/README.md        [detailed API]
              │
              ├─→ ARCHITECTURE.md        [internals]
              │
              └─→ COMPILATION.md         [3 options]
```

---

## Matriz de Referencia Cruzada

```
┌─────────────────────┬──────────┬──────────┬────────┬──────────┐
│ Componente          │ Header   │ Impl     │ Docs   │ Tests    │
├─────────────────────┼──────────┼──────────┼────────┼──────────┤
│ CommandNormalizer   │ .h       │ .cpp     │ README │ test_*.  │
│                     │ (120 L)  │ (850 L)  │ .md    │ cpp      │
├─────────────────────┼──────────┼──────────┼────────┼──────────┤
│ LnkResolver         │ .h       │ .cpp     │ ARCH   │ test_*.  │
│                     │ (50 L)   │ (140 L)  │ .md    │ cpp      │
├─────────────────────┼──────────┼──────────┼────────┼──────────┤
│ CommandResolver     │ .h       │ .cpp     │ IMPL   │ test_*.  │
│                     │ (45 L)   │ (70 L)   │ .md    │ cpp      │
├─────────────────────┼──────────┼──────────┼────────┼──────────┤
│ Integration         │ -        │ -        │ INTE   │ INTE     │
│ Examples            │          │          │ .cpp   │ .cpp     │
└─────────────────────┴──────────┴──────────┴────────┴──────────┘

Legend:
  .h         = Header file
  .cpp       = Implementation
  ARCH       = ARCHITECTURE.md
  IMPL       = IMPLEMENTATION_SUMMARY.md
  INTE       = INTEGRATION_EXAMPLES.cpp
  test_*.    = test_normalizer.cpp
```

---

## Timeline de Lectura Recomendado

```
⏱️  5 min   │ QUICK_START.md
           ├─ Setup básico
           ├─ API simplificada
           └─ Ejemplos mínimos

⏱️  10 min  │ test_normalizer.cpp
           ├─ Ejecutar tests
           ├─ Ver 15 casos diferentes
           └─ Verificar compilación

⏱️  15 min  │ INTEGRATION_EXAMPLES.cpp
           ├─ 6 patrones de integración
           ├─ Código template
           └─ Copy-paste ready

⏱️  20 min  │ normalize/README.md
           ├─ API detallada
           ├─ Edge cases
           └─ Ejemplos avanzados

⏱️  20 min  │ ARCHITECTURE.md
           ├─ Diagramas internos
           ├─ COM details
           └─ Patrones defensivos

        Total: ~70 minutos para dominio completo
```

---

## Checklist de Verificación

```
✅ Código Fuente
   ☑ CommandNormalizer.h          [normalize/]
   ☑ CommandNormalizer.cpp        [normalize/]
   ☑ LnkResolver.h                [resolve/]
   ☑ LnkResolver.cpp              [resolve/]
   ☑ CommandResolver.h            [util/]
   ☑ CommandResolver.cpp          [util/]

✅ Documentación
   ☑ QUICK_START.md
   ☑ normalize/README.md
   ☑ ARCHITECTURE.md
   ☑ COMPILATION.md
   ☑ IMPLEMENTATION_SUMMARY.md
   ☑ INDEX.md
   ☑ COMPLETION_SUMMARY.md

✅ Tests & Ejemplos
   ☑ test_normalizer.cpp          [15 tests]
   ☑ INTEGRATION_EXAMPLES.cpp      [6 patterns]

✅ Configuración
   ☑ CMakeLists.txt               [UPDATED]

Total: 18 archivos creados/modificados
```

---

## Navegación Rápida

```
Necesito...                          Voy a...
─────────────────────────────────────────────────────────
Empezar rápido                 →  QUICK_START.md
Entender API                   →  normalize/README.md
Ver código de integración      →  INTEGRATION_EXAMPLES.cpp
Compilar en Windows            →  COMPILATION.md
Entender funcionamiento interno →  ARCHITECTURE.md
Ver todo de una vez            →  COMPLETION_SUMMARY.md (este)
Encontrar archivo específico   →  INDEX.md
Ejecutar tests                 →  test_normalizer.cpp
Buscar patrón específico       →  grep en fuentes
```

---

## 📊 Estadísticas de Entrega

```
┌────────────────────────┬──────────┐
│ Métrica                │ Cantidad │
├────────────────────────┼──────────┤
│ Archivos de código     │    6     │
│ Líneas de código       │  1200+   │
│ Archivos de docs       │    8     │
│ Líneas de docs         │  2000+   │
│ Archivos de test       │    2     │
│ Test cases             │   15     │
│ Ejemplos de integración│    6     │
│ Wrappers soportados    │    7     │
│ Edge cases manejados   │    8+    │
│ Commits (recomendado)  │    1     │
└────────────────────────┴──────────┘

TOTAL ENTREGA: ~3200 líneas, 18 archivos
```

---

**Mapa generado:** Febrero 10, 2026  
**Para visualizar:** Abrir en Markdown viewer o VS Code
