# Evidence Module

## Descripción

El módulo `evidence` proporciona análisis offline, determinístico y sin heurísticas de archivos asociados a puntos de persistencia en Windows.

**Principios fundamentales:**
- ✅ Offline-first: Sin acceso a red
- ✅ Determinístico: Mismo input → mismo output
- ✅ Sin heurísticas ni scoring
- ✅ Compatible Win10 22H2 - Win11 x64
- ✅ Sólo APIs estándar de Windows

## Componentes

### 1. `file_probe.h/cpp` - Metadata Básica
Obtiene información básica del archivo usando Win32 API:
- Existencia del archivo
- Tamaño
- Timestamps (creación, modificación)
- Atributos

**API usada:** `GetFileAttributesExW`

### 2. `hashing.h/cpp` - Hash SHA-256
Calcula hash SHA-256 usando BCrypt (CNG):
- Hash en formato hex (lowercase)
- Manejo de archivos grandes (buffer de 1MB)
- Códigos de error NTSTATUS preservados

**API usada:** BCrypt (CNG)

### 3. `version_info.h/cpp` - Información de Versión
Extrae metadata de recursos PE:
- CompanyName, ProductName, ProductVersion
- FileVersion (string y binario)
- FileDescription, OriginalFilename
- LegalCopyright, InternalName

**API usada:** `GetFileVersionInfo`, `VerQueryValue`

### 4. `authenticode.h/cpp` - Verificación de Firma
Verifica firmas Authenticode **offline**:
- Usa `WTD_REVOKE_NONE` (sin revocación)
- Usa `WTD_CACHE_ONLY_URL_RETRIEVAL` (sin red)
- Preserva todos los `CERT_TRUST_STATUS` flags
- Extrae cadena de certificados completa
- Timestamps de countersignature

**API usada:** `WinVerifyTrust`, `CertGetCertificateChain`

**Flags críticos:**
```cpp
fdwRevocationChecks = WTD_REVOKE_NONE;           // No revocation checks
dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;      // No network
```

### 5. `misconfig_checks.h/cpp` - Detección de Misconfiguraciones
Detecta problemas comunes sin heurísticas:
- Paths con espacios sin comillas (injection risk)
- Rutas relativas (DLL hijacking risk)
- Extensiones sospechosas (.txt, .dat, etc.)
- Rutas de red (UNC paths)
- Variables de entorno sin resolver
- Permisos de escritura para usuarios

**Checks puramente determinísticos, sin scoring.**

### 6. `entry_evidence.h/cpp` - Orquestador
Agrega todos los análisis en un único resultado:
```cpp
EntryEvidence evidence = EntryEvidenceCollector::Collect(entry);
```

## Uso

### Ejemplo básico:
```cpp
#include "evidence/entry_evidence.h"

uboot::Entry entry;
entry.imagePath = "C:\\Windows\\System32\\notepad.exe";

// Recoger evidencia completa
auto evidence = uboot::evidence::EntryEvidenceCollector::Collect(entry);

// Verificar resultados
if (evidence.fileExists) {
    std::cout << "SHA-256: " << evidence.sha256Hex.value_or("N/A") << "\n";
    std::cout << "Signed: " << (evidence.isSigned ? "Yes" : "No") << "\n";
    std::cout << "Misconfigs: " << evidence.misconfigs.findings.size() << "\n";
}
```

### Análisis de archivo individual:
```cpp
auto evidence = uboot::evidence::EntryEvidenceCollector::CollectForFile(
    L"C:\\SomeApp\\app.exe",
    L"C:\\Some App\\app.exe /arg1 /arg2"  // Command line para misconfig checks
);
```

## Compatibilidad OS

### Windows 10 22H2 x64 (mínimo)
- `#define _WIN32_WINNT 0x0A00`
- BCrypt disponible
- WinVerifyTrust disponible
- CertGetCertificateChain disponible

### Windows 11 x64
- Mismas APIs, mismo comportamiento
- **IMPORTANTE:** No se infieren diferencias según versión de OS
- Los flags `CERT_TRUST_STATUS` se reportan tal cual

## Manejo de Errores

Todos los componentes usan `std::optional` para valores que pueden fallar:

```cpp
if (evidence.sha256Hex.has_value()) {
    // Hash exitoso
} else if (evidence.hashError.has_value()) {
    // Error específico en hashError.value()
}
```

Códigos de error preservados:
- Win32: `DWORD` (`GetLastError()`)
- NTSTATUS: `NTSTATUS` (BCrypt)
- HRESULT: `LONG` (`WinVerifyTrust`)
- CERT_TRUST: `DWORD` (flags completos)

## Compilación

Requiere:
- MSVC v143+ toolset
- Windows SDK 10.0.22621.0+
- C++17

Warnings nivel /W4 habilitado.

## Librerías requeridas

```cmake
target_link_libraries(uboot-collector PRIVATE
    bcrypt      # SHA-256 hashing
    wintrust    # Authenticode
    crypt32     # Certificate chains
    version     # Version info
    advapi32    # ACL checks
)
```

## Determinismo

**Garantías:**
1. Para un archivo dado, el hash SHA-256 es siempre el mismo
2. La metadata del archivo depende solo del filesystem, no del OS
3. La verificación de firma usa solo certificados locales (no CRL/OCSP)
4. Los checks de misconfig son puramente sintácticos
5. Version info se extrae directamente de recursos PE

**No determinístico:**
- Timestamps (dependen del filesystem)
- Certificados en el store local (pueden cambiar)

## Testing

Ejemplos en `test_evidence.cpp`:
```bash
cd build
ctest -R evidence
```

## Notas de Seguridad

Este módulo **NO**:
- Ejecuta binarios
- Hace inferencias heurísticas
- Contacta servicios externos
- Usa machine learning
- Realiza scoring de riesgo

Solo reporta hechos objetivos y verificables offline.
