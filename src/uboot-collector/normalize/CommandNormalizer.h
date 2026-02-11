#pragma once

#include <string>
#include <optional>
#include <vector>
#include <windows.h>

namespace uboot {

/// <summary>
/// Result of command normalization.
/// Contains both the resolved executable path and arguments separately,
/// or partial data if resolution was unreliable.
/// </summary>
struct NormalizationResult {
    std::string resolvedPath;    // Canonicalized executable path (UTF-8)
    std::string arguments;       // Arguments separated from executable (UTF-8)
    std::string originalCommand; // Original command for fallback
    bool isComplete;             // true if resolution is reliable, false if partial
    std::string resolveNotes;    // Explanation of resolution process (UTF-8)
};

/// <summary>
/// Robust command normalization pipeline.
/// Handles:
/// - Whitespace sanitization (NBSP, tabs, CR/LF)
/// - Environment variable expansion
/// - Tolerant command-line parsing
/// - Executable/arguments separation
/// - Path canonicalization
/// - Wrapper peeling (cmd.exe, powershell, wscript, etc.)
/// 
/// All internal processing is UTF-16; results are UTF-8 encoded.
/// </summary>
class CommandNormalizer {
public:
    /// <summary>
    /// Normalize a raw command string.
    /// </summary>
    /// <param name="rawCommand">The command to normalize</param>
    /// <param name="workingDir">Optional working directory for relative path resolution</param>
    /// <returns>Normalization result with resolved path and arguments</returns>
    static NormalizationResult Normalize(
        const std::string& rawCommand,
        const std::string& workingDir = ""
    );

private:
    // Internal UTF-16 processing pipeline
    static std::wstring SanitizeWhitespace(const std::wstring& input);
    static std::wstring ExpandEnvironmentVariables(const std::wstring& input);
    
    /// <summary>
    /// Parse command line tolerantly, separating executable from arguments.
    /// Handles:
    /// - Quoted and unquoted paths
    /// - Paths with spaces
    /// - Mismatched quotes
    /// - Intelligent quotes (curly quotes)
    /// </summary>
    static void ParseCommandLine(
        const std::wstring& commandLine,
        std::wstring& outExecutable,
        std::wstring& outArguments
    );
    
    /// <summary>
    /// Canonicalize a path using Windows APIs.
    /// Resolves relative paths, handles UNC paths, etc.
    /// </summary>
    static std::wstring CanonicalizePath(
        const std::wstring& path,
        const std::wstring& workingDir = L""
    );
    
    /// <summary>
    /// Detect and remove wrapper executables (cmd.exe, powershell, etc.)
    /// Multiple passes (max 3 iterations) to handle nested wrappers.
    /// </summary>
    static void PeelWrappers(
        std::wstring& executable,
        std::wstring& arguments,
        std::vector<std::wstring>& notes
    );
    
    /// <summary>
    /// Check if the executable is a wrapper and extract wrapped command.
    /// Returns true if a wrapper was detected.
    /// </summary>
    static bool TryPeelSingleWrapper(
        const std::wstring& executable,
        const std::wstring& arguments,
        std::wstring& outExecutable,
        std::wstring& outArguments,
        std::wstring& outNote
    );
    
    // Helper utilities
    static bool IsQuoteCharacter(wchar_t c);
    static bool IsSmartQuote(wchar_t c);
    static std::wstring TrimWhitespace(const std::wstring& input);
    static bool IsExecutableFile(const std::wstring& path);
    static std::wstring Utf8ToUtf16(const std::string& utf8);
    static std::string Utf16ToUtf8(const std::wstring& utf16);
};

} // namespace uboot
