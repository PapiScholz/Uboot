#pragma once

#include "ICollector.h"
#include <windows.h>

namespace uboot {

/// <summary>
/// Collects entries from Windows Services.
/// Uses Service Control Manager (SCM) APIs:
/// - EnumServicesStatusExW to enumerate services
/// - QueryServiceConfigW to get executable path and parameters
/// Only collects services in the "own process" and "shared process" modes.
/// </summary>
class ServicesCollector : public ICollector {
public:
    std::string GetName() const override {
        return "Services";
    }
    
    CollectorResult Collect() override;

private:
    /// <summary>
    /// Enumerate all services and collect their information.
    /// </summary>
    void EnumerateServices(
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
};

} // namespace uboot
