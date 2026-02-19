#pragma once

#include "../ISignalEvaluator.h"
#include <string>

namespace uboot {
namespace scoring {
namespace evaluators {

class UserWritableEvaluator : public ISignalEvaluator {
public:
  std::string GetSupportedSignalId() const override {
    return "user_writable_location";
  }

  std::optional<SignalEvidence>
  Evaluate(const Entry & /*entry*/, const evidence::EntryEvidence &evidence,
           const Signal &signalConfig) override {
    for (const auto &issue : evidence.misconfigs.findings) {
      if (issue.type == evidence::EvidenceIssueType::WritableByNonAdmin) {
        SignalEvidence ev;
        ev.signalId = signalConfig.id;
        ev.description = signalConfig.description;
        ev.weight = signalConfig.weight;
        ev.evidence = "File is writable by non-admin users";
        return ev;
      }
    }
    return std::nullopt;
  }
};

} // namespace evaluators
} // namespace scoring
} // namespace uboot
