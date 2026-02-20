#pragma once

#include "../IScoringStrategy.h"
#include "../ThresholdClassifier.h"
#include <numeric>

namespace uboot {
namespace scoring {
namespace strategies {

class WeightedSumStrategy : public IScoringStrategy {
public:
  ScoreResult Calculate(const std::vector<SignalEvidence> &triggeredSignals,
                        const ScoringConfig &config) override {
    ScoreResult result;
    result.totalScore = 0.0;

    for (const auto &s : triggeredSignals) {
      result.totalScore += s.weight;
    }

    if (result.totalScore < 0.0) {
      result.totalScore = 0.0;
    }

    result.value =
        ThresholdClassifier::Classify(result.totalScore, config.thresholds);
    result.evidence = triggeredSignals;

    return result;
  }
};

} // namespace strategies
} // namespace scoring
} // namespace uboot
