#include "CriticalList.h"
#include <algorithm>
#include <cctype>

namespace uboot::hardening {

// Helper for case-insensitive comparison (store keys as lowercase)
// For now we just lower-case input before lookup.

// List from requirements
const std::unordered_set<std::string> CriticalList::CriticalServices = {
    "windefend", "securityhealthservice",
    "sense",     "eventlog",
    "rpcss",     "dcomlaunch",
    "samss",     "plugplay",
    "schedule",  "bfe",
    "mpssvc",
    "wuauserv" // Listed as high warning, treated as critical block by default
               // per requirement 1
};

const std::unordered_set<std::string> CriticalList::CriticalTasks = {
    // Add specific critical tasks if any defined in future.
    // For now preventing obvious system tasks could be done by path check
    // elsewhere.
};

bool CriticalList::IsCriticalService(const std::string &serviceName) {
  std::string lowerName = serviceName;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  return CriticalServices.find(lowerName) != CriticalServices.end();
}

bool CriticalList::IsCriticalTask(const std::string &taskName) {
  // Placeholder
  return false;
}

std::string CriticalList::GetWarningMessage(const std::string &componentName) {
  return "CRITICAL: " + componentName +
         " is a core system component. Disabling it may cause instability or "
         "security risks.";
}

} // namespace uboot::hardening
