#pragma once

#include <string>
#include <optional>
#include <Windows.h>

namespace uboot {
namespace evidence {

/// <summary>
/// Version information extracted from PE file resources.
/// All fields are optional as not all PE files have version info.
/// </summary>
struct VersionInfo {
    std::optional<std::wstring> companyName;
    std::optional<std::wstring> productName;
    std::optional<std::wstring> productVersion;
    std::optional<std::wstring> fileVersion;
    std::optional<std::wstring> fileDescription;
    std::optional<std::wstring> originalFilename;
    std::optional<std::wstring> legalCopyright;
    std::optional<std::wstring> internalName;
    
    // Fixed version info (binary version)
    std::optional<uint64_t> fileVersionBinary;    // high 32 bits: major.minor, low 32 bits: build.revision
    std::optional<uint64_t> productVersionBinary;
    
    // Error state
    std::optional<DWORD> win32Error;
    
    bool HasAnyInfo() const {
        return companyName.has_value() || productName.has_value() || 
               productVersion.has_value() || fileVersion.has_value();
    }
};

/// <summary>
/// Extracts version information from PE files.
/// Uses Win32 version API - deterministic and offline.
/// </summary>
class VersionInfoExtractor {
public:
    /// <summary>
    /// Extract version info from a PE file.
    /// </summary>
    /// <param name="filePath">Full path to PE file</param>
    /// <returns>VersionInfo with populated fields or error</returns>
    static VersionInfo Extract(const std::wstring& filePath) noexcept;
    
private:
    VersionInfoExtractor() = delete;
    
    static std::optional<std::wstring> QueryStringValue(
        const void* versionData,
        const std::wstring& subBlock
    ) noexcept;
};

} // namespace evidence
} // namespace uboot
