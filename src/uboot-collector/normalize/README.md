# CommandNormalizer y LnkResolver - Guía de Uso

## Descripción General

Este módulo proporciona dos componentes principales para el procesamiento robusto de comandos en Windows:

### 1. **CommandNormalizer** (`normalize/CommandNormalizer.h`)

Pipeline de normalización multi-etapa que transforma comandos brutos en rutas de ejecutables canónicos con argumentos separados.

#### Características:

- **Sanitización de espacios**: Convierte espacios especiales (NBSP, tabulaciones, CR/LF) en espacios normales
- **Expansión de variables de entorno**: Resuelve `%USERPROFILE%`, `%SystemRoot%`, etc. usando `ExpandEnvironmentStringsW`
- **Parsing tolerante**: Maneja comillas mal cerradas, rutas sin comillas, comillas inteligentes (curly quotes)
- **Canonicalización de rutas**: Resuelve rutas relativas, expande formatos cortos (8.3), normaliza separadores usando `PathCchCanonicalizeEx` y `GetFullPathNameW`
- **Wrapper peeling (multi-pass, máx 3 iteraciones)**:
  - `cmd.exe /c` - Extrae el comando real
  - `powershell.exe -Command` - Extrae el comando PS
  - `powershell.exe -EncodedCommand` - Extrae base64 (sin decodificar/ejecutar)
  - `wscript.exe` / `cscript.exe` - Extrae el script
  - `rundll32.exe` - Extrae el DLL
  - `mshta.exe` - Extrae el HTML
  - `regsvr32.exe` - Extrae el DLL a registrar

#### API:

```cpp
struct NormalizationResult {
    std::string resolvedPath;    // Ruta ejecutable canónica (UTF-8)
    std::string arguments;       // Argumentos separados (UTF-8)
    std::string originalCommand; // Comando original para referencia
    bool isComplete;             // true si resolución es confiable
    std::string resolveNotes;    // Notas del proceso (UTF-8)
};

NormalizationResult result = CommandNormalizer::Normalize(
    "cmd.exe /c %PROGRAMFILES%\\app.exe arg1 arg2",
    "C:\\Users\\user"  // workingDir (opcional)
);

// result.resolvedPath = "C:\\Program Files\\app.exe"
// result.arguments = "arg1 arg2"
// result.isComplete = true
// result.resolveNotes = "Sanitized... Expanded... Peeled: cmd.exe /c wrapper..."
```

#### Manejo de Edge Cases:

| Caso | Comportamiento |
|------|----------------|
| Comillas mal cerradas | Busca hasta espacio o fin de línea |
| Rutas con espacios sin comillas | Toma hasta primer espacio (parcial) |
| Comillas inteligentes ("") | Se tratan como comillas normales |
| Rutas relativas | Se resuelven vs. workingDir |
| Variables de entorno | Se expanden; si fallan, se preservan |
| Archivo no existe | isComplete = false, pero se retorna el path |

---

### 2. **LnkResolver** (`resolve/LnkResolver.h`)

Resolver de accesos directos de Windows (.lnk) usando interfaces COM estándar.

#### Características:

- **Interfaz COM IShellLinkW**: Lee propiedades de archivos .lnk
- **Extrae**:
  - Ruta del ejecutable destino
  - Argumentos
  - Directorio de trabajo
- **Manejo de errores robusto**: No lanza excepciones; HRESULT en notas
- **Inicialización COM**: Asumeque `CoInitializeEx` ya está llamado (típicamente por el recolector)

#### API:

```cpp
struct LnkResolutionResult {
    std::string targetPath;          // Ruta destino (UTF-8)
    std::string arguments;           // Argumentos (UTF-8)
    std::string workingDirectory;    // Directorio trabajo (UTF-8)
    bool isResolved;                 // true si extracción exitosa
    std::string resolveNotes;        // Descripción (UTF-8)
};

LnkResolutionResult result = LnkResolver::Resolve(
    "C:\\Users\\user\\Desktop\\MyApp.lnk"
);

if (result.isResolved) {
    std::string targetPath = result.targetPath;
    std::string args = result.arguments;
    // use targetPath and args
}
```

---

## Ejemplo de Integración

Uso combinado para normalizar un comando potencialmente envuelto y resoluciones .lnk:

```cpp
#include "normalize/CommandNormalizer.h"
#include "resolve/LnkResolver.h"
#include <iostream>

int main() {
    // Ejemplo 1: Normalizar comando con wrappers
    std::string rawCmd = "cmd.exe /c powershell.exe -Command \"C:\\Program Files\\tool.exe\" -arg";
    NormalizationResult norm = CommandNormalizer::Normalize(rawCmd, "C:\\temp");
    
    std::cout << "Resolved: " << norm.resolvedPath << "\n";
    std::cout << "Args: " << norm.arguments << "\n";
    std::cout << "Complete: " << (norm.isComplete ? "true" : "false") << "\n";
    std::cout << "Notes: " << norm.resolveNotes << "\n";
    
    // Ejemplo 2: Resolver un .lnk
    std::string lnkPath = "C:\\Users\\user\\Desktop\\App.lnk";
    LnkResolutionResult lnk = LnkResolver::Resolve(lnkPath);
    
    if (lnk.isResolved) {
        std::cout << "Target: " << lnk.targetPath << "\n";
        std::cout << "Arguments: " << lnk.arguments << "\n";
        std::cout << "Working Dir: " << lnk.workingDirectory << "\n";
    } else {
        std::cout << "Resolution failed: " << lnk.resolveNotes << "\n";
    }
    
    return 0;
}
```

---

## Notas de Implementación

### Codificación

- **Procesamiento interno**: UTF-16 (requisito de APIs Win32)
- **Entrada/Salida**: UTF-8 (compatible con JSON, portabilidad)
- Conversión transparente en interfaces

### COM (LnkResolver)

```cpp
// Si se llama desde contexto sin COM inicializado:
CoInitializeEx(nullptr, COINIT_MULTITHREADED);

LnkResolutionResult result = LnkResolver::Resolve(lnkPath);

CoUninitialize();
```

### Compilación

El `CMakeLists.txt` incluye las dependencias necesarias:
- `pathcch.lib` para `PathCchCanonicalizeEx`
- `shlwapi.lib` para `PathIsAbsoluteW`, `PathCombineW`
- `ole32.lib` y `oleaut32.lib` para COM
- `shell32.lib` para `IShellLinkW`

### Restricciones de Seguridad

- **NO ejecuta comandos** - Solo parsea, expande y resuelve
- **NO decodifica/ejecuta base64** - Si hay `-EncodedCommand`, extrae el token para auditoría
- **Sin dependencias externas** - Solo Win32/COM

### Fragmentos defensivos

Ejemplos de heurística comentada:

```cpp
// TryPeelSingleWrapper() - Detección de wrapper cmd.exe
if (exeName == L"cmd.exe" || exeName == L"cmd") {
    if (arguments.find(L"/c") == 0 || arguments.find(L"/C") == 0) {
        // Heurística: /c es seguido de un (potencial) comando
        // Se busca desde posición después de "/c"
        ...
    }
}

// PowerShell -EncodedCommand
if (encPos != std::wstring::npos) {
    // No decodificamos base64 - solo extraemos el token para auditoría
    // (La decodificación sería responsabilidad del análisis)
    outArguments = args.substr(cmdStart, cmdEnd - cmdStart);
    return false; // Termina peeling aquí
}
```

---

## Testing

### Casos de Prueba Recomendados

```python
test_cases = [
    # Caso | Entrada | Esperado (resolvedPath, arguments)
    ("Simple exe", "C:\\app.exe arg1", ("C:\\app.exe", "arg1")),
    ("Quoted path", '"C:\\Program Files\\app.exe" arg1', ("C:\\Program Files\\app.exe", "arg1")),
    ("cmd.exe wrapper", "cmd.exe /c C:\\app.exe arg", ("C:\\app.exe", "arg")),
    ("PowerShell -Command", "powershell.exe -Command \"C:\\app.exe\" arg", ("C:\\app.exe", "arg")),
    ("Nested wrappers", "cmd.exe /c powershell.exe -Command app.exe", ("app.exe", "")),
    ("Bad quotes", '"C:\\path\\app.exe arg1', ("C:\\path\\app.exe", "arg1")),  # Tolerante
    ("Env var", "%SYSTEMROOT%\\System32\\cmd.exe", ("C:\\Windows\\System32\\cmd.exe", "")),
    ("Relative path", ".\\app.exe arg", ("C:\\expected\\full\\path\\app.exe", "arg")),
    ("Spaces no quotes", "C:\\Program Files\\app.exe arg", ("C:\\Program", "")),  # Parcial
]
```

---

## Referencias

- [IShellLinkW Documentation](https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ishelllinkw)
- [PathCchCanonicalizeEx](https://docs.microsoft.com/en-us/windows/win32/api/pathcch/nf-pathcch-pathcchcanonicalizeex)
- [ExpandEnvironmentStringsW](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-expandenvironmentstringsw)
