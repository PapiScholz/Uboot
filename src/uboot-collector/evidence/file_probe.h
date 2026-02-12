#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <Windows.h>

namespace uboot {
namespace evidence {

/// <summary>
/// Basic file metadata obtained through Win32 API.
/// Deterministic and offline-first.
/// </summary>
struct FileMetadata {
    bool exists = false;
    uint64_t fileSize = 0;
    FILETIME creationTime = {};
    FILETIME lastWriteTime = {};
    DWORD attributes = 0;
    
    // Error state
    std::optional<DWORD> win32Error;
};

/// <summary>
/// Probes a file for basic metadata without executing or heuristics.
/// </summary>
class FileProbe {
public:
    /// <summary>
    /// Probe a file at the given path.
    /// Uses only Win32 API for deterministic results across Win10/Win11.
    /// </summary>
    /// <param name="path">Full path to the file</param>
    /// <returns>FileMetadata with all fields populated</returns>
    static FileMetadata Probe(const std::wstring& path) noexcept;
    
    /// <summary>
    /// Check if a file exists without full metadata.
    /// </summary>
    static bool Exists(const std::wstring& path) noexcept;
    
    /// <summary>
    /// Get file size only.
    /// </summary>
    static std::optional<uint64_t> GetFileSize(const std::wstring& path) noexcept;

private:
    FileProbe() = delete;
};

} // namespace evidence
} // namespace uboot
