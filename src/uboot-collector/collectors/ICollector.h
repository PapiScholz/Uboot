#pragma once

#include "../model/Entry.h"
#include "../model/CollectorError.h"
#include <vector>
#include <string>

namespace uboot {

/// <summary>
/// Result of a collector execution.
/// Contains entries or errors, never throws exceptions.
/// </summary>
struct CollectorResult {
    std::vector<Entry> entries;
    std::vector<CollectorError> errors;
    
    bool HasErrors() const { return !errors.empty(); }
    bool HasEntries() const { return !entries.empty(); }
};

/// <summary>
/// Base interface for all collectors.
/// </summary>
class ICollector {
public:
    virtual ~ICollector() = default;
    
    /// <summary>
    /// Get the unique name of this collector.
    /// </summary>
    virtual std::string GetName() const = 0;
    
    /// <summary>
    /// Execute the collector and return results.
    /// Must never throw exceptions - all errors go into CollectorResult.
    /// </summary>
    virtual CollectorResult Collect() = 0;
};

} // namespace uboot
