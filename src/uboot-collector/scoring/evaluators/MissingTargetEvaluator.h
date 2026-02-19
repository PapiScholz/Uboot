#pragma once

#include "../ISignalEvaluator.h"
#include <string>

namespace uboot {
namespace scoring {
namespace evaluators {

class MissingTargetEvaluator : public ISignalEvaluator {
public:
  std::string GetSupportedSignalId() const override { return "missing_target"; }

  std::optional<SignalEvidence>
  Evaluate(const Entry & /*entry*/, const evidence::EntryEvidence &evidence,
           const Signal &signalConfig) override {
    for (const auto &issue : evidence.misconfigs.findings) {
      if (issue.type == evidence::EvidenceIssueType::ResultTargetMissing) {
        SignalEvidence ev;
        ev.signalId = signalConfig.id;
        ev.description = signalConfig.description;
        ev.weight = signalConfig.weight;
        ev.evidence = "Target file does not exist";
        return ev;
      }
    }
    return std::nullopt;
  }
};

} // namespace evaluators
} // namespace scoring
} // namespace uboot
