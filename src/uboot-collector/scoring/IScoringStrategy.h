#pragma once

#include "Types.h"
#include <vector>

namespace uboot {
namespace scoring {

/// <summary>
/// Interface for combining signal weights into a final result.
/// </summary>
class IScoringStrategy {
public:
  virtual ~IScoringStrategy() = default;

  /// <summary>
  /// Calculate the final score based on triggered signals.
  /// </summary>
  /// <param name="triggeredSignals">List of signals that matched</param>
  /// <param name="config">Scoring configuration (for thresholds/setup)</param>
  /// <returns>Populated ScoreResult (excluding metadata like
  /// timestamp)</returns>
  virtual ScoreResult
  Calculate(const std::vector<SignalEvidence> &triggeredSignals,
            const ScoringConfig &config) = 0;
};

} // namespace scoring
} // namespace uboot
