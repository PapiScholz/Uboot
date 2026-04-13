#pragma once

#include "../ISignalEvaluator.h"
#include <string>

namespace uboot {
namespace scoring {
namespace evaluators {

class UnsignedBinaryEvaluator : public ISignalEvaluator {
public:
  std::string GetSupportedSignalId() const override {
    return "unsigned_binary";
  }

  std::optional<SignalEvidence>
  Evaluate(const Entry & /*entry*/, const evidence::EntryEvidence &evidence,
           const Signal &signalConfig) override {
    if (evidence.fileMetadata.fileAttributes != INVALID_FILE_ATTRIBUTES &&
        !evidence.authenticode.isSigned) {
      SignalEvidence ev;
      ev.signalId = signalConfig.id; // "unsigned_binary"
      ev.description = signalConfig.description;
      ev.weight = signalConfig.weight;
      ev.evidence = "File is not signed";
      return ev;
    }
    return std::nullopt;
  }
};

} // namespace evaluators
} // namespace scoring
} // namespace uboot
