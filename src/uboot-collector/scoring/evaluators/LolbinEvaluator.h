#pragma once

#include "../ISignalEvaluator.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace uboot {
namespace scoring {
namespace evaluators {

class LolbinEvaluator : public ISignalEvaluator {
public:
  explicit LolbinEvaluator(std::vector<std::string> lolbins)
      : m_lolbins(std::move(lolbins)) {}

  std::string GetSupportedSignalId() const override { return "lolbin_autorun"; }

  std::optional<SignalEvidence>
  Evaluate(const Entry &entry, const evidence::EntryEvidence &evidence,
           const Signal &signalConfig) override {
    if (!IsLolbin(evidence.imagePath)) {
      return std::nullopt;
    }

    bool hasSuspiciousArgs = HasSuspiciousArguments(entry.arguments);
    bool isLegitLocation = IsLegitimateLocation(evidence.imagePath);

    // Contextual policy:
    // 1. If arguments are suspicious -> Trigger (regardless of location)
    // 2. If location is NOT legitimate (e.g. running from User temp) -> Trigger
    // 3. If legitimate location AND no suspicious args -> Do NOT trigger
    if (!hasSuspiciousArgs && isLegitLocation) {
      return std::nullopt;
    }

    std::string evidenceStr = "LOLBin detected: " + ToUtf8(evidence.imagePath);
    if (!entry.arguments.empty()) {
      evidenceStr += " Arguments: " + entry.arguments;
    }

    if (hasSuspiciousArgs) {
      evidenceStr += " [Suspicious Arguments Detected]";
    }
    if (!isLegitLocation) {
      evidenceStr += " [Non-Standard Location]";
    }

    SignalEvidence ev;
    ev.signalId = signalConfig.id;
    ev.description = signalConfig.description;
    ev.weight = signalConfig.weight;
    ev.evidence = evidenceStr;
    return ev;
  }

private:
  std::vector<std::string> m_lolbins;

  static std::string ToUtf8(const std::wstring &wstr) {
    if (wstr.empty())
      return "";
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0],
                        size_needed, nullptr, nullptr);
    return strTo;
  }

  static std::string ToLower(const std::string &str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
  }

  bool IsLolbin(const std::wstring &imagePath) const {
    if (imagePath.empty())
      return false;
    std::filesystem::path path(imagePath);
    std::string filename = ToLower(path.filename().string());

    for (const auto &lolbin : m_lolbins) {
      if (ToLower(lolbin) == filename) {
        return true;
      }
    }
    return false;
  }

  bool HasSuspiciousArguments(const std::string &args) const {
    if (args.empty())
      return false;
    std::string lowerArgs = ToLower(args);

    // Indicators of possible malicious intent
    const std::vector<std::string> indicators = {
        "-enc", "/c", "/s", "http:", "https:", "base64", "-encodedcommand"};

    for (const auto &indicator : indicators) {
      if (lowerArgs.find(indicator) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  bool IsLegitimateLocation(const std::wstring &imagePath) const {
    if (imagePath.empty())
      return false;

    // Normalize path to lower case generic generic string for check
    // In a real scenario, we should resolve environment variables or use system
    // API For now, checks for standard system roots.
    std::filesystem::path path(imagePath);
    std::string pathStr = ToLower(path.string());

    // Basic heuristic: check if it starts with system root
    // Note: This is a simplification.
    return pathStr.find("c:\\windows\\system32") != std::string::npos ||
           pathStr.find("c:\\windows\\syswow64") != std::string::npos;
  }
};

} // namespace evaluators
} // namespace scoring
} // namespace uboot
