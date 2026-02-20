#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace uboot::hardening {

class CriticalList {
public:
  // Checks if a service or task name is in the critical list.
  // Returns true if critical.
  static bool IsCriticalService(const std::string &serviceName);
  static bool IsCriticalTask(const std::string &taskName);

  // formatting helper
  static std::string GetWarningMessage(const std::string &componentName);

private:
  static const std::unordered_set<std::string> CriticalServices;
  static const std::unordered_set<std::string> CriticalTasks;
};

} // namespace uboot::hardening
