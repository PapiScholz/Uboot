#pragma once

#include "../collectors/ICollector.h"
#include <vector>
#include <string>
#include <memory>

namespace uboot {

/// <summary>
/// Orchestrates execution of multiple collectors.
/// </summary>
class CollectorRunner {
public:
    CollectorRunner();
    ~CollectorRunner();
    
    /// <summary>
    /// Execute specified collectors.
    /// </summary>
    /// <param name="sourceNames">List of collector names to run ("all" = run all)</param>
    /// <param name="outEntries">Output: collected entries</param>
    /// <param name="outErrors">Output: collection errors</param>
    /// <returns>Number of collectors that successfully executed</returns>
    int Run(
        const std::vector<std::string>& sourceNames,
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
    
private:
    std::vector<std::unique_ptr<ICollector>> collectors_;
    
    void InitializeCollectors();
    bool ShouldRun(const std::string& collectorName, const std::vector<std::string>& requestedSources);
};

} // namespace uboot
