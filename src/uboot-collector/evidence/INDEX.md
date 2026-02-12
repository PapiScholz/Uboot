# Evidence Module - Índice de Archivos

## 📋 Resumen
Módulo de análisis offline y determinístico de archivos para puntos de persistencia en Windows.

**Principio:** Mismo input → mismo output, sin red, sin heurísticas.

---

## 🗂 Estructura de Archivos

### Componentes Core (headers + cpp)

| Archivo | Propósito | APIs Usadas |
|---------|-----------|-------------|
| [`file_probe.h/cpp`](file_probe.h) | Metadata básica de archivos | `GetFileAttributesExW` |
| [`hashing.h/cpp`](hashing.h) | Hash SHA-256 con BCrypt | `BCrypt*` (CNG) |
| [`version_info.h/cpp`](version_info.h) | Extracción de version info PE | `GetFileVersionInfo`, `VerQueryValue` |
| [`authenticode.h/cpp`](authenticode.h) | Verificación de firma Authenticode | `WinVerifyTrust`, `CertGetCertificateChain` |
| [`misconfig_checks.h/cpp`](misconfig_checks.h) | Detección de misconfiguraciones | `GetNamedSecurityInfo`, ACL checks |
| [`entry_evidence.h/cpp`](entry_evidence.h) | Orquestador de evidencia completa | Todos los anteriores |

### Documentación

| Archivo | Contenido |
|---------|-----------|
| [`README.md`](README.md) | Documentación principal del módulo |
| [`BUILD_AND_VALIDATION.md`](BUILD_AND_VALIDATION.md) | Guía de compilación y testing |
| [`INDEX.md`](INDEX.md) | Este archivo - índice de navegación |

### Ejemplos

| Archivo | Descripción |
|---------|-------------|
| [`example_usage.cpp`](example_usage.cpp) | Programa de ejemplo mostrando uso del módulo |

---

## 🎯 Quick Start

### 1. Incluir en tu código:
```cpp
#include "evidence/entry_evidence.h"

auto evidence = uboot::evidence::EntryEvidenceCollector::Collect(entry);
```

### 2. Compilar:
```powershell
cd build
cmake --build . --config Release
```

### 3. Verificar output:
Ver [`BUILD_AND_VALIDATION.md`](BUILD_AND_VALIDATION.md) para tests.

---

## 🔍 Buscar por Funcionalidad

### Quiero obtener...

**Hash SHA-256 de un archivo:**
- Ver [`hashing.h`](hashing.h) → `Hashing::ComputeSHA256()`

**Metadata básica (tamaño, timestamps):**
- Ver [`file_probe.h`](file_probe.h) → `FileProbe::Probe()`

**Información de versión (Company, Product, etc.):**
- Ver [`version_info.h`](version_info.h) → `VersionInfoExtractor::Extract()`

**Verificación de firma Authenticode:**
- Ver [`authenticode.h`](authenticode.h) → `AuthenticodeVerifier::Verify()`

**Detectar misconfiguraciones:**
- Ver [`misconfig_checks.h`](misconfig_checks.h) → `MisconfigChecker::Check()`

**Todo lo anterior de una vez:**
- Ver [`entry_evidence.h`](entry_evidence.h) → `EntryEvidenceCollector::Collect()`

---

## 📊 Flujo de Análisis

```
Entry/FilePath
      ↓
EntryEvidenceCollector::Collect()
      ↓
  ┌───┴───┬───────┬─────────┬────────┬─────────┐
  ↓       ↓       ↓         ↓        ↓         ↓
FileProbe Hash VersionInfo AuthCode Misconfig [parallel]
  ↓       ↓       ↓         ↓        ↓         ↓
  └───┬───┴───────┴─────────┴────────┴─────────┘
      ↓
EntryEvidence (resultado completo)
```

---

## 🛡 Flags Críticos de Authenticode

**Ubicación:** [`authenticode.cpp`](authenticode.cpp) líneas ~65-70

```cpp
fdwRevocationChecks = WTD_REVOKE_NONE;           // Sin revocación
dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;      // Sin red
```

**Por qué:** Garantiza operación offline-first y determinística.

---

## 🧪 Testing

### Tests manuales:
```powershell
# Ver BUILD_AND_VALIDATION.md sección "Testing"
.\build\bin\Release\uboot-collector.exe
```

### Validar determinismo:
```powershell
# Ejecutar dos veces, comparar outputs
.\uboot-collector.exe > out1.json
.\uboot-collector.exe > out2.json
Compare-Object (Get-Content out1.json) (Get-Content out2.json)
```

---

## ⚙ Configuración CMake

**Ubicación:** [`../CMakeLists.txt`](../CMakeLists.txt)

Archivos evidence agregados en:
- `set(SOURCES ...)` - líneas con `evidence/*.cpp`
- `set(HEADERS ...)` - líneas con `evidence/*.h`
- `target_link_libraries(...)` - bcrypt, wintrust, crypt32, version

---

## 🎓 Aprender Más

### Principiante:
1. Leer [`README.md`](README.md) completo
2. Estudiar [`example_usage.cpp`](example_usage.cpp)
3. Compilar y ejecutar ejemplo

### Intermedio:
1. Explorar cada componente individual (file_probe, hashing, etc.)
2. Leer comentarios en headers
3. Experimentar con [`BUILD_AND_VALIDATION.md`](BUILD_AND_VALIDATION.md)

### Avanzado:
1. Estudiar implementación de Authenticode offline
2. Revisar manejo de errores (std::optional, NTSTATUS, HRESULT)
3. Profiling de performance

---

## 📞 Troubleshooting

### Errores de compilación:
→ Ver [`BUILD_AND_VALIDATION.md`](BUILD_AND_VALIDATION.md) sección "Verificación de Errores Comunes"

### Problemas de IntelliSense:
→ Ejecutar: `CMake: Configure` en VS Code

### APIs no encontradas:
→ Verificar Windows SDK >= 10.0.22621.0

---

## 📝 Notas de Diseño

### Por qué no se usa `std::filesystem`:
- Comportamiento puede variar entre Win10/Win11
- Preferimos Win32 API para máxima compatibilidad

### Por qué `WTD_REVOKE_NONE`:
- Offline-first: sin acceso a CRL/OCSP
- Determinístico: mismo resultado sin depender de red

### Por qué preservar CERT_TRUST flags completos:
- No inferir significado según OS
- Dejar análisis para capa superior
- Determinismo: reportar hechos, no interpretaciones

---

## 🔗 Referencias Externas

- [BCrypt (CNG) Documentation](https://learn.microsoft.com/en-us/windows/win32/seccng/cng-portal)
- [WinVerifyTrust API](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust)
- [Certificate Chain Verification](https://learn.microsoft.com/en-us/windows/win32/seccrypto/certificate-chain-verification)
- [Version Information Functions](https://learn.microsoft.com/en-us/windows/win32/menurc/version-information)

---

**Última actualización:** 2026-02-12  
**Versión módulo:** 1.0.0  
**Compatibilidad:** Windows 10 22H2 x64 - Windows 11 x64
