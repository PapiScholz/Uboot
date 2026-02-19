#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace uboot {
namespace evidence {

/// <summary>
/// Types of factual issues that can be detected.
/// </summary>
enum class EvidenceIssueType {
  ResultTargetMissing,      // Referenced file does not exist
  UnquotedPathWithSpaces,   // Path with spaces not quoted
  RelativePath,             // Relative path detected
  ExtensionNotExecutable,   // File extension is not typically executable
  EmptyCommand,             // Empty or whitespace-only command
  NetworkPath,              // UNC path detected
  EnvironmentVarUnresolved, // %VAR% not resolved
  WritableByNonAdmin,       // File is writeable by non-admin users
  InvalidArguments,         // Malformed argument structure
};

/// <summary>
/// A detected factual issue.
/// </summary>
struct EvidenceIssue {
  EvidenceIssueType type;
  std::wstring description;
  std::wstring affectedValue;
};

/// <summary>
/// Result of misconfiguration checks.
/// </summary>
struct MisconfigCheckResult {
  std::vector<EvidenceIssue> findings;

  bool HasFindings() const { return !findings.empty(); }
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
  static MisconfigCheckResult Check(const std::wstring &commandOrPath,
                                    const std::wstring &imagePath) noexcept;

  /// <summary>
  /// Check if a file path has writeable permissions for non-admin users.
  /// </summary>
  static bool IsWriteableByUsers(const std::wstring &filePath) noexcept;

private:
  MisconfigChecker() = delete;

  static void CheckPathQuoting(const std::wstring &commandOrPath,
                               std::vector<EvidenceIssue> &findings) noexcept;

  static void CheckRelativePath(const std::wstring &imagePath,
                                std::vector<EvidenceIssue> &findings) noexcept;

  static void CheckExtension(const std::wstring &imagePath,
                             std::vector<EvidenceIssue> &findings) noexcept;

  static void CheckNetworkPath(const std::wstring &imagePath,
                               std::vector<EvidenceIssue> &findings) noexcept;

  static void
  CheckEnvironmentVars(const std::wstring &commandOrPath,
                       std::vector<EvidenceIssue> &findings) noexcept;

  static void
  CheckFileWritePerms(const std::wstring &imagePath,
                      std::vector<EvidenceIssue> &findings) noexcept;

  static bool HasSpaces(const std::wstring &str) noexcept;
  static bool IsQuoted(const std::wstring &str) noexcept;
  static bool IsAbsolutePath(const std::wstring &path) noexcept;
  static bool IsNetworkPath(const std::wstring &path) noexcept;
  static std::wstring GetFileExtension(const std::wstring &path) noexcept;
};

} // namespace evidence
} // namespace uboot
