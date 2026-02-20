#include "CollectorRunner.h"
#include "../enumerators/ICollector.h"
#include "../enumerators/IfeoDebuggerCollector.h"
#include "../enumerators/RunRegistryCollector.h"
#include "../enumerators/ScheduledTasksCollector.h"
#include "../enumerators/ServicesCollector.h"
#include "../enumerators/StartupFolderCollector.h"
#include "../enumerators/WinlogonCollector.h"
#include "../enumerators/WmiPersistenceCollector.h"
#include <algorithm>

namespace uboot {

CollectorRunner::CollectorRunner() { InitializeCollectors(); }

CollectorRunner::~CollectorRunner() = default;

void CollectorRunner::InitializeCollectors() {
  // Register all available collectors
  collectors_.push_back(std::make_unique<RunRegistryCollector>());
  collectors_.push_back(std::make_unique<ServicesCollector>());
  collectors_.push_back(std::make_unique<StartupFolderCollector>());
  collectors_.push_back(std::make_unique<ScheduledTasksCollector>());
  collectors_.push_back(std::make_unique<WmiPersistenceCollector>());
  collectors_.push_back(std::make_unique<WinlogonCollector>());
  collectors_.push_back(std::make_unique<IfeoDebuggerCollector>());
}

bool CollectorRunner::ShouldRun(
    const std::string &collectorName,
    const std::vector<std::string> &requestedSources) {
  // Check if "all" is requested
  if (std::find(requestedSources.begin(), requestedSources.end(), "all") !=
      requestedSources.end()) {
    return true;
  }

  // Check if this specific collector is requested (case-insensitive)
  for (const auto &source : requestedSources) {
    std::string lowerSource = source;
    std::transform(lowerSource.begin(), lowerSource.end(), lowerSource.begin(),
                   ::tolower);
    std::string lowerCollector = collectorName;
    std::transform(lowerCollector.begin(), lowerCollector.end(),
                   lowerCollector.begin(), ::tolower);

    if (lowerSource == lowerCollector) {
      return true;
    }
  }

  return false;
}

int CollectorRunner::Run(const std::vector<std::string> &sourceNames,
                         std::vector<Entry> &outEntries,
                         std::vector<CollectorError> &outErrors) {
  int successCount = 0;

  for (const auto &collector : collectors_) {
    std::string collectorName = collector->GetName();

    if (!ShouldRun(collectorName, sourceNames)) {
      continue; // Skip this collector
    }

    try {
      // Execute collector
      CollectorResult result = collector->Collect();

      // Merge results
      outEntries.insert(outEntries.end(), result.entries.begin(),
                        result.entries.end());
      outErrors.insert(outErrors.end(), result.errors.begin(),
                       result.errors.end());

      // Count as success if no errors occurred
      if (!result.HasErrors()) {
        successCount++;
      }
    } catch (const std::exception &e) {
      // Should never happen (collectors must not throw), but handle it
      CollectorError error(collectorName,
                           std::string("Unexpected exception: ") + e.what());
      outErrors.push_back(error);
    } catch (...) {
      // Handle unknown exceptions
      CollectorError error(collectorName, "Unknown exception occurred");
      outErrors.push_back(error);
    }
  }

  return successCount;
}

} // namespace uboot
