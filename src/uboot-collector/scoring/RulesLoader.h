#pragma once

#include "../model/CollectorError.h"
#include "Types.h"
#include <optional>
#include <string>

namespace uboot {
namespace scoring {

class RulesLoader {
public:
  /// <summary>
  /// Loads scoring configuration from a JSON file.
  /// </summary>
  /// <param name="path">Path to rules.json</param>
  /// <returns>ScoringConfig if successful, nullopt otherwise</returns>
  static std::optional<ScoringConfig> Load(const std::string &path);
};

} // namespace scoring
} // namespace uboot
