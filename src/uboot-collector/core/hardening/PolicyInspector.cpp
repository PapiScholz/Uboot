#include "PolicyInspector.h"
#include <algorithm>
#include <cctype>

namespace uboot::hardening {

bool PolicyInspector::IsPolicyKey(const std::string &keyPath) {
  std::string lowerPath = keyPath;
  std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lowerPath.find("software\\policies") != std::string::npos)
    return true;
  if (lowerPath.find(
          "software\\microsoft\\windows\\currentversion\\policies") !=
      std::string::npos)
    return true;

  return false;
}

bool PolicyInspector::IsSecurityKey(const std::string &keyPath) {
  std::string lowerPath = keyPath;
  std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lowerPath.find("windows defender") != std::string::npos)
    return true;
  if (lowerPath.find("microsoft\\windows\\currentversion\\run") !=
      std::string::npos) {
    // We'd ideally check digital signature of binary here,
    // but strictly path-based check for "Run" isn't enough to say it's security
    // critical unless we match known security vendors. Requirement says:
    // "\CurrentVersion\Run (HKLM) con firma Microsoft" This function just
    // checks path potentially. Signature check needs Entry context.
  }
  return false;
}

} // namespace uboot::hardening
