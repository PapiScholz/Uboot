# Compilación y Validación del Módulo Evidence

## Requisitos Previos

### Software necesario:
- Visual Studio 2022 (v143 toolset o superior)
- Windows SDK 10.0.22621.0 o superior
- CMake 3.15+
- PowerShell 5.1+

### Verificar versión de Windows SDK:
```powershell
reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10
```

## Compilación

### Opción 1: Usando el script de build existente

```powershell
cd C:\Users\Ezequiel\Documents\GitHub\Uboot\src\uboot-collector
.\build-collector.ps1
```

### Opción 2: Usando CMake directamente

```powershell
# Crear carpeta de build
cd C:\Users\Ezequiel\Documents\GitHub\Uboot\src\uboot-collector
mkdir build -Force
cd build

# Configurar con CMake
cmake .. -G "Visual Studio 17 2022" -A x64

# Compilar
cmake --build . --config Release

# O en Debug para debugging
cmake --build . --config Debug
```

### Opción 3: Desde Visual Studio

1. Abrir `uboot-collector` como carpeta CMake
2. Seleccionar configuración: `x64-Release` o `x64-Debug`
3. Build → Build All

## Verificar compilación sin warnings

MSVC debe compilar con `/W4` sin warnings:

```powershell
cd build
cmake --build . --config Release -- /p:WarningLevel=4 /p:TreatWarningsAsErrors=false
```

**Expectativa:** Compilación exitosa sin warnings.

## Testing del Módulo Evidence

### Test básico - Analizar notepad.exe

```powershell
cd build\bin\Release
.\uboot-collector.exe  # Ver si incluye evidencia en output
```

### Test manual con archivo de ejemplo

Crear un archivo de test simple:

```powershell
# Analizar archivo del sistema
$testFile = "C:\Windows\System32\notepad.exe"

# Si compilaste example_usage.cpp como ejecutable separado:
.\evidence_example.exe $testFile
```

### Validación de comportamiento determinístico

```powershell
# Ejecutar dos veces sobre el mismo archivo
.\uboot-collector.exe > output1.json
.\uboot-collector.exe > output2.json

# Los hashes deben ser idénticos
Compare-Object (Get-Content output1.json) (Get-Content output2.json)
# Expectativa: Sin diferencias (excepto timestamps)
```

## Validación de Compatibilidad

### Windows 10 22H2 x64
```powershell
# Verificar que NO usa APIs de Win11
dumpbin /IMPORTS build\bin\Release\uboot-collector.exe | Select-String "api-ms-win"
```

### Verificar librerías linkadas
```powershell
dumpbin /DEPENDENTS build\bin\Release\uboot-collector.exe

# Debe incluir:
# - bcrypt.dll
# - wintrust.dll
# - crypt32.dll
# - version.dll
```

## Validación de Flags Authenticode

Para verificar que se usan los flags correctos (offline-first):

```cpp
// En authenticode.cpp, verificar:
fdwRevocationChecks = WTD_REVOKE_NONE;           // ✓
dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;      // ✓
```

Búsqueda rápida:
```powershell
Select-String "WTD_REVOKE_NONE" .\evidence\authenticode.cpp
Select-String "WTD_CACHE_ONLY_URL_RETRIEVAL" .\evidence\authenticode.cpp
```

## Tests Específicos

### 1. Hash Determinístico
```powershell
$file = "C:\Windows\System32\kernel32.dll"

# Ejecutar dos veces
$hash1 = (Get-FileHash $file -Algorithm SHA256).Hash
# El módulo evidence debe devolver el mismo hash lowercase
```

### 2. Authenticode Offline
```powershell
# Desconectar red o bloquear con firewall
# Ejecutar collector
# Debe funcionar sin timeouts ni errores de red
```

### 3. Misconfig Detection
```powershell
# Crear entry con path sin comillas con espacios
# Debe detectar: PathContainsSpacesNoQuotes
```

## Debugging

### Con Visual Studio Debugger:

1. Abrir `uboot-collector` en VS
2. Set breakpoint en `EntryEvidenceCollector::Collect()`
3. F5 para debug
4. Inspeccionar valores de `EntryEvidence`

### Con WinDbg:

```powershell
windbg -o build\bin\Debug\uboot-collector.exe
```

## Verificación de Errores Comunes

### Error: "cannot open source file bcrypt.h"
**Solución:** Instalar Windows SDK completo

### Error: LNK1104 cannot open bcrypt.lib
**Solución:** Verificar que CMakeLists.txt incluya:
```cmake
target_link_libraries(uboot-collector PRIVATE bcrypt)
```

### Error: No se encuentra WinVerifyTrust
**Solución:** Agregar:
```cmake
target_link_libraries(uboot-collector PRIVATE wintrust)
```

## Validación en Win10 vs Win11

### En Windows 10 22H2:
```powershell
# Compilar
cmake --build . --config Release

# Ejecutar
.\build\bin\Release\uboot-collector.exe

# Guardar output
.\build\bin\Release\uboot-collector.exe > win10_output.json
```

### En Windows 11:
```powershell
# Mismo procedimiento
.\build\bin\Release\uboot-collector.exe > win11_output.json

# Comparar certificados y trust status - deben ser iguales
# (excepto si los stores locales difieren)
```

## Profiling de Performance

### Timing de cada componente:

```cpp
// En entry_evidence.cpp, agregar timing:
auto start = std::chrono::high_resolution_clock::now();
evidence.fileMetadata = FileProbe::Probe(filePath);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
```

**Expectativas:**
- FileProbe: < 1ms
- Hashing: Variable (depende de tamaño, ~10-100ms para archivos típicos)
- VersionInfo: < 5ms
- Authenticode: 10-50ms (sin red)
- MisconfigChecks: < 1ms

## Checklist Pre-Release

- [ ] Compila sin warnings con /W4
- [ ] Funciona en Win10 22H2 x64
- [ ] Funciona en Win11 x64
- [ ] No hace llamadas de red
- [ ] Hashes son determinísticos
- [ ] CERT_TRUST flags se preservan
- [ ] WTD_REVOKE_NONE está configurado
- [ ] WTD_CACHE_ONLY_URL_RETRIEVAL está configurado
- [ ] No usa std::filesystem
- [ ] Maneja errores sin exceptions

## Recursos

- [BCrypt CNG API](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/)
- [WinVerifyTrust](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust)
- [Certificate Chain Verification](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certgetcertificatechain)
