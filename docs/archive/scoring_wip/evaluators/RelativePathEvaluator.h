#pragma once

#include "../ISignalEvaluator.h"
#include <string>

namespace uboot {
namespace scoring {
namespace evaluators {

class RelativePathEvaluator : public ISignalEvaluator {
public:
  std::string GetSupportedSignalId() const override { return "relative_path"; }

  std::optional<SignalEvidence>
  Evaluate(const Entry & /*entry*/, const evidence::EntryEvidence &evidence,
           const Signal &signalConfig) override {
    for (const auto &issue : evidence.misconfigs.findings) {
      if (issue.type == evidence::EvidenceIssueType::RelativePath) {
        SignalEvidence ev;
        ev.signalId = signalConfig.id;
        ev.description = signalConfig.description;
        ev.weight = signalConfig.weight;
        ev.evidence = "Entry uses relative path";
        return ev;
      }
    }
    return std::nullopt;
  }
};

} // namespace evaluators
} // namespace scoring
} // namespace uboot
