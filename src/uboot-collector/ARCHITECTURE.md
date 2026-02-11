# Arquitectura: CommandNormalizer y LnkResolver

## Diagrama de Flujo

```
┌─────────────────────────────────────────────────────────────┐
│  CommandResolver::ResolveCommand(rawCommand, workingDir)    │
└────────────────────┬────────────────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         │                       │
    ¿.lnk file?              ¿Direct command?
         │                       │
         ▼                       ▼
   LnkResolver              CommandNormalizer
   .Resolve()               .Normalize()
         │                       │
    [COM INtf]         [UTF-16 pipeline]
    - IShellLinkW             │
    - IPersistFile       1. SanitizeWhitespace
                        2. ExpandEnvironmentVariables
    Extrae:            3. ParseCommandLine
    - targetPath       4. CanonicalizePath
    - arguments        5. PeelWrappers
    - workingDir           (multi-pass)
         │                       │
         └───────────┬───────────┘
                     │
                     ▼
          ┌──────────────────────┐
          │ NormalizationResult  │
          ├──────────────────────┤
          │ resolvedPath (UTF-8) │
          │ arguments (UTF-8)    │
          │ originalCommand      │
          │ isComplete (bool)    │
          │ resolveNotes (UTF-8) │
          └──────────────────────┘
```

## Garantías de Seguridad

### 1. **No ejecución de código**
- ✓ Solo parsing y resolución
- ✓ No se construyen comandos ejecutables
- ✓ Base64 en PowerShell -EncodedCommand solo se extrae, nunca se decodifica

### 2. **Manejo de edge cases defensivo**
- ✓ Comillas mal cerradas: se intenta parsear hasta espacio
- ✓ Rutas sin comillas con espacios: resultado parcial + `isComplete=false`
- ✓ Variables no expandibles: se preserva original
- ✓ Archivos no existentes: `isComplete=false` + notas

### 3. **Conversión de codificación controlada**
- ✓ Entrada: UTF-8 (compatible con JSON, C#)
- ✓ Procesamiento: UTF-16 (APIs Win32)
- ✓ Salida: UTF-8 (compatible hacia adelante)

## Wrapper Peeling - Detalles Técnicos

### Wrappers Soportados

| Wrapper | Flags/Opciones | Lógica de Peeling |
|---------|----------------|-------------------|
| `cmd.exe` | `/c`, `/k` | Extrae comando después de flag |
| `powershell.exe` | `-Command`, `-EncodedCommand` | -Command: parsea; -EncodedCommand: preserva base64 |
| `wscript.exe` | ninguno | Primer argumento = script |
| `cscript.exe` | ninguno | Primer argumento = script |
| `rundll32.exe` | ninguno | Primer argumento = DLL |
| `mshta.exe` | ninguno | Primer argumento = HTML/HTA |
| `regsvr32.exe` | `/s`, `/i:<string>` | Argumento sin flag = DLL |

### Multi-pass (máximo 3 iteraciones)

```cpp
for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
    if (TryPeelSingleWrapper(...)) {
        // Peeled successfully, loop continues
        executable = newExecutable;
        arguments = newArguments;
    } else {
        // No wrapper or terminal point (e.g., -EncodedCommand)
        break;
    }
}
```

**Ejemplo**: 
```
Input:  "cmd.exe /c powershell.exe -Command notepad.exe"
Iter 1: Peeled cmd.exe → executable="powershell.exe", args="-Command notepad.exe"
Iter 2: Peeled powershell -Command → executable="notepad.exe", args=""
Iter 3: No wrapper found → STOP
Result: executable="notepad.exe"
```

## Path Canonicalization

### Proceso de Canonicalización

```cpp
std::wstring CanonicalizePath(const std::wstring& path, const std::wstring& workingDir) {
    // 1. Si es relativa + tenemos workingDir, combinar
    if (!PathIsAbsoluteW(path) && !workingDir.empty()) {
        PathCombineW(combined, workingDir, path);
        path = combined;
    }
    
    // 2. Intentar PathCchCanonicalizeEx (Windows 8+)
    // Resuelve: 8.3 paths, .. y ., symlinks, etc.
    
    // 3. Fallback: GetFullPathNameW
}
```

**Casos resueltos**:
- `C:\app.exe` → `C:\app.exe`
- `c:\APP.EXE` → `C:\APP.EXE` (normaliza case)
- `C:\Program~ File\app.exe` → `C:\Program Files\app.exe`
- `..\app.exe` + workingDir=`C:\temp` → `C:\app.exe`
- `\\?\UNC\server\share\app.exe` → resuelto correctamente

## LnkResolver - Detalles COM

### Inicialización (Caller Responsibility)

```cpp
// El usuario debe inicializar COM si no está ya inicializado:
CoInitializeEx(nullptr, COINIT_MULTITHREADED);

LnkResolutionResult result = LnkResolver::Resolve("path.lnk");

CoUninitialize();
```

### Secuencia COM

```cpp
1. CoCreateInstance(CLSID_ShellLink, ..., IID_IShellLinkW)
   → Crea interfaz IShellLinkW

2. QueryInterface(IID_IPersistFile)
   → Obtiene iPersistFile para carga

3. IPersistFile::Load(path, STGM_READ)
   → Carga archivo .lnk

4. IShellLinkW::Resolve(nullptr, SLR_NO_UI | SLR_UPDATE)
   → Intenta resolver broken links (no interfere)

5. IShellLinkW::GetPath(buffer, MAX_PATH, &wfd, SLGP_RAWPATH)
6. IShellLinkW::GetArguments(buffer, MAX_PATH)
7. IShellLinkW::GetWorkingDirectory(buffer, MAX_PATH)
   → Extrae propiedades

8. Cleanup: Release() para ambas interfaces
```

### Códigos de Error (HRESULT)

| HRESULT | Significado | Notas |
|---------|-------------|-------|
| `S_OK` | Éxito | |
| `S_FALSE` | Parcial | (raro) |
| `E_FAIL` | Fallo genérico | Archivo corrupto, acceso denegado |
| `HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)` | Archivo no existe | |
| `E_INVALIDARG` | Argumento inválido | Nombre de archivo nulo |

## Encoding Strategy

### UTF-8 ↔ UTF-16 Conversión

```cpp
// UTF-8 → UTF-16
std::wstring Utf8ToUtf16(const std::string& utf8) {
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring utf16(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &utf16[0], size);
    return utf16;
}

// UTF-16 → UTF-8
std::string Utf16ToUtf8(const std::wstring& utf16) {
    int size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], size, nullptr, nullptr);
    return utf8;
}
```

**Por qué UTF-8 externo**:
- JSON usa UTF-8
- C# (Uboot.Analysis) espera UTF-8
- Portabilidad entre sistemas
- Reducción de tamaño (ASCII susbset)

**Por qué UTF-16 interno**:
- Requisito de Win32 (GetFullPathNameW, ExpandEnvironmentStringsW, etc.)
- Manejo correcto de caracteres especiales de Windows
- APIs COM (IShellLinkW) usan WCHAR

## Integración con Collectors

### Patrón de Uso en RunRegistryCollector

```cpp
// En RunRegistryCollector::Collect()
HKEY key;
if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    std::string command = ReadRegValue(key, "ImagePath");
    
    // Aquí integrar normalización:
    NormalizationResult normalized = CommandResolver::ResolveCommand(
        command,
        "C:\\Windows\\System32"  // working dir típico
    );
    
    Entry entry("RunRegistry", "Machine", regKey, regPath, 
                normalized.arguments, normalized.resolvedPath);
    entries.push_back(entry);
}
```

### Patrón de Uso en StartupFolderCollector

```cpp
// En StartupFolderCollector::Collect()
// Encontrar .lnk en folder de startup
if (IsLnkFile(filePath)) {
    NormalizationResult resolved = CommandResolver::ResolveCommand(filePath);
    Entry entry("StartupFolder", "User", filename, filePath,
                resolved.arguments, resolved.resolvedPath);
    entries.push_back(entry);
}
```

## Testing Strategy

### Nivel 1: Unit Tests
- Test individual `CommandNormalizer` pipeline steps
- Test individual wrapper peeling
- Test path canonicalization

### Nivel 2: Integration Tests  
- Test `CommandResolver` con .lnk + normalization
- Test múltiples wrappers anidados
- Test con archivos reales de sistema

### Nivel 3: Regression Tests
Conjunto de comandos del mundo real:
```
- Registry run keys existentes
- Shortcuts en Desktop
- Service executables
- Task Scheduler entries
```

## Limitaciones Conocidas

### 1. PowerShell -EncodedCommand
- ✗ No decodificamos base64 (por seguridad)
- ✓ Extraemos token para auditoría
- → Análisis posterior debe decodificar si lo necesita

### 2. Rutas con espacios sin comillas
```
Input:  "C:\Program Files\app.exe arg1"
Output: resolvedPath = "C:\Program"
        arguments = "Files\app.exe arg1"
        isComplete = false
```
→ Esperado; heurística conservadora

### 3. Broken .lnk files
- Si el target no existe, `IShellLinkW::Resolve()` falla
- Pero aún extraemos el path almacenado (puede ser viejo)
- → `isResolved=false`, pero se retorna `targetPath` de todas formas

### 4. Variables de entorno no estándar
- Solo se expanden variables que `ExpandEnvironmentStringsW` conoce
- Variables personalizadas (p.ej., `%CUSTOM%`) se preservan como-está
- → Notas indican si hubo expansión

## Futuros Mejoras (No Incluidas)

- [ ] Resolver UNC paths en wrappers
- [ ] Detectar AutoPlay scripts
- [ ] Resolver comilla inteligentes (U+2018-U+201D) a comillas normales antes de parsear
- [ ] Integración con NSIS/Installers parsing
- [ ] Detección de obfuscación de comandos
