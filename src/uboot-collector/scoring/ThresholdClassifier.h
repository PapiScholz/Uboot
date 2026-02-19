#pragma once

#include "Types.h"
#include <string>
#include <vector>

namespace uboot {
namespace scoring {

class ThresholdClassifier {
public:
  static std::string Classify(double score,
                              const std::vector<Threshold> &thresholds) {
    for (const auto &t : thresholds) {
      if (score >= t.minScore) {
        return t.label;
      }
    }
    return "Unknown";
  }
};

} // namespace scoring
} // namespace uboot
