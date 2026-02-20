#pragma once

#include <string>

namespace uboot::hardening {

class PolicyInspector {
public:
  // Check if registry key path is under a policy-protected area
  static bool IsPolicyKey(const std::string &keyPath);

  // Check if it's a security provider or defender key
  static bool IsSecurityKey(const std::string &keyPath);
};

} // namespace uboot::hardening
