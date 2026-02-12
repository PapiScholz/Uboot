#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace uboot {
namespace evidence {

/// <summary>
/// Types of misconfigurations that can be detected.
/// </summary>
enum class MisconfigType {
    FileNotFound,               // Referenced file does not exist
    PathContainsSpacesNoQuotes, // Path with spaces not quoted (injection risk)
    RelativePath,               // Relative path (DLL hijacking risk)
    SuspiciousExtension,        // Non-executable extension (.txt, .dat, etc.)
    EmptyCommand,               // Empty or whitespace-only command
    NetworkPath,                // UNC path (\\server\share)
    EnvironmentVarUnresolved,   // %VAR% not resolved
    WriteableByUsers,           // File is writeable by non-admin users
    InvalidArguments,           // Malformed argument structure
};

/// <summary>
/// A detected misconfiguration.
/// </summary>
struct MisconfigFinding {
    MisconfigType type;
    std::wstring description;
    std::wstring affectedValue;  // The problematic path/command/argument
};

/// <summary>
/// Result of misconfiguration checks.
/// </summary>
struct MisconfigCheckResult {
    std::vector<MisconfigFinding> findings;
    
    bool HasFindings() const { return !findings.empty(); }
    bool HasCriticalFindings() const;
};

/// <summary>
/// Detects common misconfigurations in persistence entries.
/// Purely deterministic analysis based on static rules.
/// No heuristics, no scoring.
/// </summary>
class MisconfigChecker {
public:
    /// <summary>
    /// Check a command/path for misconfigurations.
    /// </summary>
    /// <param name="commandOrPath">Full command line or path</param>
    /// <param name="imagePath">Resolved image path</param>
    /// <returns>MisconfigCheckResult with all findings</returns>
    static MisconfigCheckResult Check(
        const std::wstring& commandOrPath,
        const std::wstring& imagePath
    ) noexcept;
    
    /// <summary>
    /// Check if a file path has writeable permissions for non-admin users.
    /// </summary>
    static bool IsWriteableByUsers(const std::wstring& filePath) noexcept;
    
private:
    MisconfigChecker() = delete;
    
    static void CheckPathQuoting(
        const std::wstring& commandOrPath,
        std::vector<MisconfigFinding>& findings
    ) noexcept;
    
    static void CheckRelativePath(
        const std::wstring& imagePath,
        std::vector<MisconfigFinding>& findings
    ) noexcept;
    
    static void CheckExtension(
        const std::wstring& imagePath,
        std::vector<MisconfigFinding>& findings
    ) noexcept;
    
    static void CheckNetworkPath(
        const std::wstring& imagePath,
        std::vector<MisconfigFinding>& findings
    ) noexcept;
    
    static void CheckEnvironmentVars(
        const std::wstring& commandOrPath,
        std::vector<MisconfigFinding>& findings
    ) noexcept;
    
    static void CheckFileWritePerms(
        const std::wstring& imagePath,
        std::vector<MisconfigFinding>& findings
    ) noexcept;
    
    static bool HasSpaces(const std::wstring& str) noexcept;
    static bool IsQuoted(const std::wstring& str) noexcept;
    static bool IsAbsolutePath(const std::wstring& path) noexcept;
    static bool IsNetworkPath(const std::wstring& path) noexcept;
    static std::wstring GetFileExtension(const std::wstring& path) noexcept;
};

} // namespace evidence
} // namespace uboot
