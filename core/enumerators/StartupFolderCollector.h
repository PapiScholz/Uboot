#pragma once

#include "ICollector.h"
#include <string>
#include <vector>

namespace uboot {

/// <summary>
/// Collects entries from Windows Startup folders.
/// Enumerates .lnk files and resolves them.
/// Sources:
/// - User Startup: %APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup
/// - Common Startup: %ProgramData%\Microsoft\Windows\Start Menu\Programs\Startup
/// </summary>
class StartupFolderCollector : public ICollector {
public:
    std::string GetName() const override {
        return "StartupFolder";
    }
    
    CollectorResult Collect() override;

private:
    /// <summary>
    /// Collect entries from a specific startup folder.
    /// </summary>
    void CollectFromFolder(
        const std::string& scope,
        const std::string& folderPath,
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
    
    /// <summary>
    /// Recursively enumerate all .lnk files in a directory.
    /// </summary>
    void EnumerateLnkFiles(
        const std::string& scope,
        const std::string& folderPath,
        const std::string& relativePath,
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
    
    /// <summary>
    /// Get the path to a special folder.
    /// </summary>
    static std::string GetSpecialFolderPath(const std::string& folderName);
};

} // namespace uboot
