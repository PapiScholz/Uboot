#pragma once

#include "../ISignalEvaluator.h"
#include <cstdio>
#include <string>


namespace uboot {
namespace scoring {
namespace evaluators {

class InvalidSignatureEvaluator : public ISignalEvaluator {
public:
  std::string GetSupportedSignalId() const override {
    return "invalid_signature";
  }

  std::optional<SignalEvidence>
  Evaluate(const Entry & /*entry*/, const evidence::EntryEvidence &evidence,
           const Signal &signalConfig) override {
    if (evidence.authenticode.isSigned && !evidence.authenticode.IsSuccess()) {
      std::string errorHex = "Unknown";
      if (evidence.authenticode.trustStatus.has_value()) {
        char buf[32];
        sprintf_s(buf, "0x%X",
                  (unsigned int)evidence.authenticode.trustStatus.value());
        errorHex = buf;
      }

      SignalEvidence ev;
      ev.signalId = signalConfig.id;
      ev.description = signalConfig.description;
      ev.weight = signalConfig.weight;
      ev.evidence = "Signature verification failed. Status: " + errorHex;
      return ev;
    }
    return std::nullopt;
  }
};

} // namespace evaluators
} // namespace scoring
} // namespace uboot
