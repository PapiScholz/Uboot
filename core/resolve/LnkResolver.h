#pragma once

#include <string>

namespace uboot {

/// <summary>
/// Result of .lnk shortcut resolution.
/// Contains the target executable path and arguments.
/// </summary>
struct LnkResolutionResult {
    std::string targetPath;      // Resolved target executable path (UTF-8)
    std::string arguments;       // Arguments from the shortcut (UTF-8)
    std::string workingDirectory; // Working directory from the shortcut (UTF-8)
    bool isResolved;             // true if resolution was successful
    std::string resolveNotes;    // Explanation of what happened (UTF-8)
};

/// <summary>
/// Resolver for Windows shortcut files (.lnk).
/// Uses IShellLinkW and IPersistFile COM interfaces.
/// 
/// Handles:
/// - Reading target path and arguments from .lnk files
/// - Extracting working directory
/// - Error handling without throwing exceptions
/// 
/// All results are UTF-8 encoded.
/// </summary>
class LnkResolver {
public:
    /// <summary>
    /// Resolve a .lnk shortcut file to its target.
    /// </summary>
    /// <param name="lnkFilePath">Full path to the .lnk file</param>
    /// <returns>Resolution result with target path and arguments</returns>
    static LnkResolutionResult Resolve(const std::string& lnkFilePath);
    
private:
    static std::string Utf16ToUtf8(const std::wstring& utf16);
    static std::wstring Utf8ToUtf16(const std::string& utf8);
};

} // namespace uboot
