#pragma once

#include "../ISignalEvaluator.h"
#include <string>

namespace uboot {
namespace scoring {
namespace evaluators {

class ObfuscatedCommandEvaluator : public ISignalEvaluator {
public:
  std::string GetSupportedSignalId() const override {
    return "obfuscated_command";
  }

  std::optional<SignalEvidence>
  Evaluate(const Entry &entry, const evidence::EntryEvidence & /*evidence*/,
           const Signal &signalConfig) override {
    // Simple heuristic: caret injection
    if (entry.arguments.find('^') != std::string::npos) {
      SignalEvidence ev;
      ev.signalId = signalConfig.id;
      ev.description = signalConfig.description;
      ev.weight = signalConfig.weight;
      ev.evidence = "Carat injection detected in arguments";
      return ev;
    }
    return std::nullopt;
  }
};

} // namespace evaluators
} // namespace scoring
} // namespace uboot
