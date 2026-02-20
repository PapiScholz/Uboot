#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>


namespace uboot {
namespace scoring {

/// <summary>
/// Represents a single detection rule or heuristic.
/// </summary>
struct Signal {
  std::string id;
  std::string description;
  double weight;
};

/// <summary>
/// Evidence for why a specific signal was triggered.
/// </summary>
struct SignalEvidence {
  std::string signalId;
  std::string description;
  double weight;
  std::string evidence; // Specific details
};

/// <summary>
/// Result of evaluation.
/// </summary>
struct ScoreResult {
  // Core scoring
  double totalScore;
  std::string value; // Classification

  // Explainability & Metadata
  std::vector<SignalEvidence> evidence;
  std::string timestamp; // Evaluation time (ISO 8601 preferred)
  std::string modelVersion;
  std::optional<std::string> fileHash; // SHA256 of the target file

  // Future proofing
  std::map<std::string, std::string> attributes; // Extensible attributes
};

struct Threshold {
  std::string label;
  double minScore;
};

struct ScoringConfig {
  std::string version;
  std::string model; // e.g. "weighted_sum"
  std::map<std::string, Signal> signals;
  std::vector<Threshold> thresholds;
  std::vector<std::string> lolbins;
};

} // namespace scoring
} // namespace uboot
