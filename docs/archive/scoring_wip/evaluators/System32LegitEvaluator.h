#pragma once

#include "../ISignalEvaluator.h"
#include <algorithm>
#include <cwctype>
#include <string>


namespace uboot {
namespace scoring {
namespace evaluators {

class System32LegitEvaluator : public ISignalEvaluator {
public:
  std::string GetSupportedSignalId() const override {
    return "system32_legit_signed";
  }

  std::optional<SignalEvidence>
  Evaluate(const Entry & /*entry*/, const evidence::EntryEvidence &evidence,
           const Signal &signalConfig) override {
    std::wstring path = ToLowerW(evidence.imagePath);
    if (path.find(L"c:\\windows\\system32\\") == 0) {
      if (evidence.authenticode.IsSuccess()) {
        bool isMicrosoft = false;
        if (evidence.authenticode.signerSubject.has_value()) {
          std::wstring subject =
              ToLowerW(evidence.authenticode.signerSubject.value());
          if (subject.find(L"microsoft") != std::wstring::npos) {
            isMicrosoft = true;
          }
        }

        if (isMicrosoft) {
          SignalEvidence ev;
          ev.signalId = signalConfig.id;
          ev.description = signalConfig.description;
          ev.weight = signalConfig.weight;
          ev.evidence = "Microsoft signed binary in System32";
          return ev;
        }
      }
    }
    return std::nullopt;
  }

private:
  static std::wstring ToLowerW(const std::wstring &str) {
    std::wstring lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    return lower;
  }
};

} // namespace evaluators
} // namespace scoring
} // namespace uboot
