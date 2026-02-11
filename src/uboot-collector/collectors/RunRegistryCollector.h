#pragma once

#include "ICollector.h"
#include <windows.h>

namespace uboot {

/// <summary>
/// Collects entries from Windows Run and RunOnce registry keys.
/// Sources:
/// - HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
/// - HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\RunOnce
/// - HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Run (32 & 64 bit)
/// - HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\RunOnce (32 & 64 bit)
/// </summary>
class RunRegistryCollector : public ICollector {
public:
    std::string GetName() const override {
        return "RunRegistry";
    }
    
    CollectorResult Collect() override;

private:
    /// <summary>
    /// Collect entries from a specific registry hive/key.
    /// </summary>
    void CollectFromKey(
        const std::string& scope,
        HKEY hive,
        const std::string& subkey,
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
    
    /// <summary>
    /// Enumerate a registry key and collect all value entries.
    /// </summary>
    void EnumerateRegistry(
        const std::string& scope,
        HKEY keyHandle,
        const std::string& keyPath,
        const std::string& source,
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
};

} // namespace uboot
