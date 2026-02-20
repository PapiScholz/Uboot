#include "ScoringEngine.h"
#include "SignalRegistry.h"
#include "evaluators/InvalidSignatureEvaluator.h"
#include "evaluators/LolbinEvaluator.h"
#include "evaluators/MissingTargetEvaluator.h"
#include "evaluators/ObfuscatedCommandEvaluator.h"
#include "evaluators/RelativePathEvaluator.h"
#include "evaluators/System32LegitEvaluator.h"
#include "evaluators/UnsignedBinaryEvaluator.h"
#include "evaluators/UserWritableEvaluator.h"
#include "strategies/WeightedSumStrategy.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace uboot {
namespace scoring {

#include <mutex>

ScoringEngine::ScoringEngine(ScoringConfig config)
    : m_config(std::move(config)) {

  // Register default evaluators (Concurrent safety: Run once)
  static std::once_flag registryFlag;
  std::call_once(registryFlag, [this]() {
    auto &registry = SignalRegistry::Instance();

    registry.Register("unsigned_binary", []() {
      return std::make_unique<evaluators::UnsignedBinaryEvaluator>();
    });
    registry.Register("invalid_signature", []() {
      return std::make_unique<evaluators::InvalidSignatureEvaluator>();
    });
    registry.Register("missing_target", []() {
      return std::make_unique<evaluators::MissingTargetEvaluator>();
    });
    registry.Register("user_writable_location", []() {
      return std::make_unique<evaluators::UserWritableEvaluator>();
    });

    // Capture lolbins for lambda - uses config from the first initialized
    // engine
    std::vector<std::string> lolbins = m_config.lolbins;
    registry.Register("lolbin_autorun", [lolbins]() {
      return std::make_unique<evaluators::LolbinEvaluator>(lolbins);
    });

    registry.Register("obfuscated_command", []() {
      return std::make_unique<evaluators::ObfuscatedCommandEvaluator>();
    });
    registry.Register("relative_path", []() {
      return std::make_unique<evaluators::RelativePathEvaluator>();
    });
    registry.Register("system32_legit_signed", []() {
      return std::make_unique<evaluators::System32LegitEvaluator>();
    });
  });

  InitializeStrategy();
  InitializeEvaluators();
}

void ScoringEngine::InitializeStrategy() {
  if (m_config.model == "weighted_sum") {
    m_strategy = std::make_unique<strategies::WeightedSumStrategy>();
  } else {
    m_strategy = std::make_unique<strategies::WeightedSumStrategy>();
  }
}

void ScoringEngine::InitializeEvaluators() {
  auto &registry = SignalRegistry::Instance();

  for (const auto &[id, signal] : m_config.signals) {
    auto evaluator = registry.Create(id);
    if (evaluator) {
      m_activeEvaluators.push_back(std::move(evaluator));
    }
  }
}

ScoreResult ScoringEngine::Evaluate(const Entry &entry,
                                    const evidence::EntryEvidence &evidence) {
  std::vector<SignalEvidence> triggeredSignals;

  for (const auto &evaluator : m_activeEvaluators) {
    try {
      std::string id = evaluator->GetSupportedSignalId();
      const auto &signalConfig = m_config.signals.at(id);

      auto result = evaluator->Evaluate(entry, evidence, signalConfig);
      if (result.has_value()) {
        triggeredSignals.push_back(result.value());
      }
    } catch (...) {
      // Log error
    }
  }

  // 1. Deterministic Output: Sort signals by ID
  std::sort(triggeredSignals.begin(), triggeredSignals.end(),
            [](const SignalEvidence &a, const SignalEvidence &b) {
              return a.signalId < b.signalId;
            });

  ScoreResult result = m_strategy->Calculate(triggeredSignals, m_config);

  // 3. Negative Score Clamping
  result.totalScore = std::max(0.0, result.totalScore);

  result.modelVersion = m_config.version;
  if (evidence.sha256Hex.has_value()) {
    result.fileHash = evidence.sha256Hex.value();
  }

  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
  result.timestamp = ss.str();

  return result;
}

} // namespace scoring
} // namespace uboot
