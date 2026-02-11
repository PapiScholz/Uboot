#pragma once

#include "ICollector.h"
#include <comdef.h>

namespace uboot {

/// <summary>
/// Collects entries from Windows Scheduled Tasks.
/// Uses Task Scheduler 2.0 COM interface:
/// - ITaskService to connect to Task Scheduler
/// - ITaskFolder to enumerate task folders recursively
/// - IRegisteredTask and ITaskDefinition to get task details
/// Only extracts ExecAction (executable actions).
/// </summary>
class ScheduledTasksCollector : public ICollector {
public:
    std::string GetName() const override {
        return "ScheduledTasks";
    }
    
    CollectorResult Collect() override;

private:
    /// <summary>
    /// Recursively enumerate task folders and collect actions.
    /// </summary>
    void EnumerateTaskFolder(
        IDispatch* folderDispatch,
        const std::string& folderPath,
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
    
    /// <summary>
    /// Extract executable actions from a task.
    /// </summary>
    void ExtractTaskActions(
        const std::string& taskName,
        const std::string& folderPath,
        IDispatch* taskDispatch,
        std::vector<Entry>& outEntries,
        std::vector<CollectorError>& outErrors
    );
    
    /// <summary>
    /// Convert COM description to UTF-8 string.
    /// </summary>
    static std::string VariantToString(const VARIANT& var);
};

} // namespace uboot
