#pragma once

#include <string>
#include <vector>

namespace uboot::hardening {

class DependencyAnalyzer {
public:
  // Returns list of dependent service names.
  // If empty, no dependents found (or check failed/not applicable).
  static std::vector<std::string>
  GetDependentServices(const std::string &serviceName);
};

} // namespace uboot::hardening
