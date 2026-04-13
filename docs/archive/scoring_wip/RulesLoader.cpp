#include "RulesLoader.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>


#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

namespace uboot {
namespace scoring {

std::optional<ScoringConfig> RulesLoader::Load(const std::string &path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    std::cerr << "[Error] Failed to open rules file: " << path << std::endl;
    return std::nullopt;
  }

  rapidjson::IStreamWrapper isw(ifs);
  rapidjson::Document d;
  d.ParseStream(isw);

  if (d.HasParseError()) {
    std::cerr << "[Error] JSON parse error in rules file." << std::endl;
    return std::nullopt;
  }

  ScoringConfig config;

  // Validation: Check required top-level fields
  if (!d.HasMember("version") || !d["version"].IsString()) {
    std::cerr
        << "[Warning] Missing or invalid 'version' field. Defaulting to '1.0'."
        << std::endl;
    config.version = "1.0";
  } else {
    config.version = d["version"].GetString();
  }

  if (!d.HasMember("scoring_model") || !d["scoring_model"].IsString()) {
    std::cerr << "[Warning] Missing or invalid 'scoring_model' field. "
                 "Defaulting to 'weighted_sum'."
              << std::endl;
    config.model = "weighted_sum";
  } else {
    config.model = d["scoring_model"].GetString();
  }

  if (d.HasMember("signals") && d["signals"].IsArray()) {
    const auto &signalsArray = d["signals"];
    for (const auto &s : signalsArray.GetArray()) {
      // strict validation for signals
      if (!s.HasMember("id") || !s["id"].IsString()) {
        std::cerr << "[Error] Signal definition missing 'id'. Skipping."
                  << std::endl;
        continue;
      }
      std::string id = s["id"].GetString();

      if (!s.HasMember("description") || !s["description"].IsString()) {
        std::cerr << "[Warning] Signal '" << id << "' missing description."
                  << std::endl;
      }

      if (!s.HasMember("weight") || !s["weight"].IsDouble()) {
        std::cerr << "[Error] Signal '" << id
                  << "' missing or invalid 'weight'. Skipping." << std::endl;
        continue;
      }

      Signal signal;
      signal.id = id;
      signal.description =
          s.HasMember("description") ? s["description"].GetString() : "";
      signal.weight = s["weight"].GetDouble();
      config.signals[signal.id] = signal;
    }
  }

  if (d.HasMember("thresholds") && d["thresholds"].IsArray()) {
    const auto &thresholdsArray = d["thresholds"];
    for (const auto &t : thresholdsArray.GetArray()) {
      if (t.HasMember("label") && t["label"].IsString() &&
          t.HasMember("min_score") && t["min_score"].IsDouble()) {

        Threshold threshold;
        threshold.label = t["label"].GetString();
        threshold.minScore = t["min_score"].GetDouble();
        config.thresholds.push_back(threshold);
      }
    }
    // Ensure thresholds are sorted descending by minScore
    std::sort(config.thresholds.begin(), config.thresholds.end(),
              [](const Threshold &a, const Threshold &b) {
                return a.minScore > b.minScore;
              });
  }

  if (d.HasMember("lolbins") && d["lolbins"].IsArray()) {
    const auto &lolbinsArray = d["lolbins"];
    for (const auto &l : lolbinsArray.GetArray()) {
      if (l.IsString()) {
        config.lolbins.push_back(l.GetString());
      }
    }
  }

  return config;
}

} // namespace scoring
} // namespace uboot
