#pragma once

#include "ICollector.h"

namespace uboot {

/// <summary>
/// Collects entries from WMI Event Filter persistence.
/// Enumerates WMI classes in root\subscription:
/// - __EventFilter (event filter definitions)
/// - CommandLineEventConsumer (consumers that execute commands)
/// - __FilterToConsumerBinding (bindings between filters and consumers)
/// </summary>
class WmiPersistenceCollector : public ICollector {
public:
    std::string GetName() const override {
        return "WmiPersistence";
    }
    
    CollectorResult Collect() override;

private:
    /// <summary>
    /// Connect to WMI and enumerate __EventFilter instances.
    /// </summary>
    void EnumerateWmiFilters(
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
};

} // namespace uboot
