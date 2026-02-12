#pragma once

#include "../model/Entry.h"
#include "file_probe.h"
#include "hashing.h"
#include "version_info.h"
#include "authenticode.h"
#include "misconfig_checks.h"
#include <string>
#include <optional>

namespace uboot {
namespace evidence {

/// <summary>
/// Complete evidence package for a single persistence entry.
/// Aggregates all offline, deterministic analysis results.
/// </summary>
struct EntryEvidence {
    // Reference to original entry
    std::wstring source;
    std::wstring scope;
    std::wstring key;
    std::wstring imagePath;
    
    // File metadata
    FileMetadata fileMetadata;
    
    // Hashing
    std::optional<std::string> sha256Hex;
    std::optional<NTSTATUS> hashError;
    
    // Version information
    VersionInfo versionInfo;
    
    // Authenticode signature
    AuthenticodeResult authenticode;
    
    // Misconfiguration checks
    MisconfigCheckResult misconfigs;
    
    // Summary flags (derived from above)
    bool fileExists = false;
    bool isSigned = false;
    bool signatureValid = false;
    bool hasMisconfigs = false;
    bool hasCriticalMisconfigs = false;
};

/// <summary>
/// Collects comprehensive evidence for persistence entries.
/// All operations are offline-first and deterministic.
/// </summary>
class EntryEvidenceCollector {
public:
    /// <summary>
    /// Collect all evidence for a persistence entry.
    /// </summary>
    /// <param name="entry">The persistence entry to analyze</param>
    /// <returns>EntryEvidence with all fields populated</returns>
    static EntryEvidence Collect(const Entry& entry) noexcept;
    
    /// <summary>
    /// Collect evidence for a specific file path.
    /// Useful for ad-hoc analysis.
    /// </summary>
    /// <param name="filePath">Full path to file</param>
    /// <param name="commandLine">Optional command line for misconfig checks</param>
    /// <returns>EntryEvidence with all fields populated</returns>
    static EntryEvidence CollectForFile(
        const std::wstring& filePath,
        const std::wstring& commandLine = L""
    ) noexcept;
    
private:
    EntryEvidenceCollector() = delete;
    
    static std::wstring ToWideString(const std::string& str) noexcept;
    static void PopulateSummaryFlags(EntryEvidence& evidence) noexcept;
};

} // namespace evidence
} // namespace uboot
