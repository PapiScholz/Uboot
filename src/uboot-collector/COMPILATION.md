# Guía de Compilación - CommandNormalizer y LnkResolver

## Opción 1: Compilación Manual MSVC (Sin CMake)

Si CMake no está instalado, puedes compilar directamente con MSVC:

### Paso 1: Abrir Visual Studio Developer Command Prompt

```batch
# En Windows 11/10:
# Busca "Developer Command Prompt for VS 2022" en el Start Menu
```

### Paso 2: Navegar al directorio

```batch
cd C:\Users\Ezequiel\Documents\GitHub\Uboot\src\uboot-collector
```

### Paso 3: Compilar archivos objeto

```batch
cl /EHsc /std:c++17 /I. /TC normalize\CommandNormalizer.cpp ^
   /Fo.\obj\CommandNormalizer.obj

cl /EHsc /std:c++17 /I. /TC resolve\LnkResolver.cpp ^
   /Fo.\obj\LnkResolver.obj

cl /EHsc /std:c++17 /I. /TC util\CommandResolver.cpp ^
   /Fo.\obj\CommandResolver.obj
```

Opciones de compilación explicadas:
- `/EHsc` - Exception handling
- `/std:c++17` - C++ 17 standard
- `/I.` - Include directory (current directory)
- `/TC` - Compile as C++
- `/Fo` - Output object file

### Paso 4: Crear biblioteca estática (opcional)

```batch
lib /out:uboot-normalize.lib ^
    obj\CommandNormalizer.obj ^
    obj\LnkResolver.obj ^
    obj\CommandResolver.obj
```

### Paso 5: Compilar test

```batch
cl /EHsc /std:c++17 /I. test_normalizer.cpp ^
   obj\CommandNormalizer.obj obj\LnkResolver.obj obj\CommandResolver.obj ^
   /link pathcch.lib shlwapi.lib shell32.lib ole32.lib ^
   /out:test_normalizer.exe
```

### Paso 6: Ejecutar test

```batch
test_normalizer.exe
```

---

## Opción 2: Con CMake (Recomendado)

### Instalar CMake

```batch
# Con Chocolatey:
choco install cmake

# O descargar desde https://cmake.org/download/
```

### Compilar

```powershell
cd "C:\Users\Ezequiel\Documents\GitHub\Uboot\src\uboot-collector"
mkdir build -ErrorAction SilentlyContinue
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

El ejecutable estará en: `build\bin\Release\uboot-collector.exe`

---

## Opción 3: MSBuild directo

Si tienes Visual Studio sin CMake, puedes crear un .vcxproj:

```bash
# [Crear archivo uboot-collector.vcxproj - ver plantilla abajo]

msbuild uboot-collector.vcxproj /p:Configuration=Release /p:Platform=x64
```

---

## Integrando en Collectors Existentes

### Ejemplo: RunRegistryCollector Integration

En `collectors/RunRegistryCollector.h`:

```cpp
#pragma once

#include "../util/CommandResolver.h"
#include "../model/Entry.h"
#include "../model/CollectorError.h"
#include "../collectors/ICollector.h"
#include <string>
#include <vector>

namespace uboot {

class RunRegistryCollector : public ICollector {
public:
    std::string GetName() const override { return "RunRegistry"; }
    CollectorResult Collect() override;

private:
    void CollectFromRegistry(
        HKEY rootKey,
        const std::string& scope,
        std::vector<Entry>& entries,
        std::vector<CollectorError>& errors
    );
};

} // namespace uboot
```

Implementación del Collect() actualizada:

```cpp
CollectorResult RunRegistryCollector::Collect() {
    CollectorResult result;
    
    // ... código existente para abrir registry ...
    
    // Para cada entrada en registry:
    std::string rawCommand = RegQueryValueString(key, "ImagePath");
    
    // NUEVO: Normalizar el comando
    NormalizationResult normalized = CommandResolver::ResolveCommand(
        rawCommand,
        "C:\\Windows\\System32"  // working dir típico
    );
    
    // Crear Entry con datos normalizados
    Entry entry;
    entry.source = "RunRegistry";
    entry.scope = scope;
    entry.key = subKeyName;
    entry.location = regPath;
    entry.imagePath = normalized.resolvedPath;      // ← NUEVO
    entry.arguments = normalized.arguments;         // ← NUEVO
    
    // Preservar notas de resolución si fallo parcial
    if (!normalized.isComplete) {
        entry.description = normalized.resolveNotes;
    }
    
    result.entries.push_back(entry);
    
    return result;
}
```

### Ejemplo: StartupFolderCollector Integration

```cpp
// En StartupFolderCollector::Collect()
CollectorResult StartupFolderCollector::Collect() {
    CollectorResult result;
    std::wstring startupPath = GetUserStartupFolder();
    
    // Buscar archivos en carpeta
    WIN32_FIND_DATAW findData;
    HANDLE findHandle = FindFirstFileW((startupPath + L"\\*").c_str(), &findData);
    
    if (findHandle != INVALID_HANDLE_VALUE) {
        do {
            if (wcschr(findData.cFileName, L'.') == nullptr) continue;
            
            std::string fileName = ConvertWideToUtf8(findData.cFileName);
            std::string filePath = ConvertWideToUtf8(startupPath + L"\\" + findData.cFileName);
            
            // NUEVO: Resolver comando (incluyendo .lnk)
            NormalizationResult resolved = CommandResolver::ResolveCommand(filePath);
            
            Entry entry;
            entry.source = "StartupFolder";
            entry.scope = "User";
            entry.key = fileName;
            entry.location = filePath;
            entry.imagePath = resolved.resolvedPath;      // ← NUEVO
            entry.arguments = resolved.arguments;         // ← NUEVO
            
            result.entries.push_back(entry);
            
        } while (FindNextFileW(findHandle, &findData));
        
        FindClose(findHandle);
    }
    
    return result;
}
```

---

## Dependencias de Compilación

### Libraries Requeridas

```cmake
# Automaticamente vinculadas por CMakeLists.txt:
target_link_libraries(
    advapi32    # Registry API
    shell32     # Shell lightweight utilities
    ole32       # COM
    oleaut32    # COM Automation  
    shlwapi     # PathXxx functions
    pathcch     # Path canonicalization
)
```

### Versiones Mínimas

- **Windows**: 7 SP1 o superior
- **Visual Studio**: 2015 o superior (C++17 required)
- **CMake**: 3.15+ (opcional)

---

## Verificar Compilación

Después de compilar, verifica:

```batch
# Ver símbolos exportados (objetos COM)
dumpbin /exports uboot-collector.exe | findstr "Shell"

# Ejecutar test
test_normalizer.exe

# Buscar "PASSED" - todo debe pasar
```

---

## Troubleshooting

### Error: "pathcch.lib not found"

Solution: Asegúrate de tener Windows 8+ SDK. Instala:
```batch
choco install windows-sdk-10-version-2004-all
```

O usa fallback:
```cpp
// En CommandNormalizer.cpp, comentar PathCchCanonicalizeEx
// y usar solo GetFullPathNameW como fallback
```

### Error: "ole32.lib not found"

Solution: Link directamente:
```batch
cl ... /link "C:\Program Files (x86)\Windows Kits\10\Lib\...\ole32.lib"
```

### Error: CoCreateInstance falla

Solution: Asegurar que COM esté inicializado:
```cpp
CoInitializeEx(nullptr, COINIT_MULTITHREADED);
// ... usar LnkResolver ...
CoUninitialize();
```

---

## Performance

Benchmark esperado (single pass):

- CommandNormalizer (simple): ~0.1ms
- CommandNormalizer (with 3 wrapper peel passes): ~0.3ms
- LnkResolver (resolve .lnk): ~5-10ms (I/O bound)

Para 1000 entradas: ~5.5 segundos total (aceptable)

---

## Próximos Pasos

1. ✅ Compilar CommandNormalizer + LnkResolver
2. ✅ Integrar en RunRegistryCollector
3. ✅ Integrar en StartupFolderCollector
4. ⏸️ Integrar en ServicesCollector (servicios no tienen .lnk)
5. ⏸️ Integrar en ScheduledTasksCollector
6. Test con datos reales
7. Validar con análisis Uboot.Analysis
