#pragma once

#include "../evidence/entry_evidence.h"
#include "../model/Entry.h"
#include "IScoringStrategy.h"
#include "ISignalEvaluator.h"
#include "Types.h"
#include <memory>
#include <string>
#include <vector>


namespace uboot {
namespace scoring {

class ScoringEngine {
public:
  explicit ScoringEngine(ScoringConfig config);

  /// <summary>
  /// Evaluates an entry using the configured strategy and evaluators.
  /// </summary>
  ScoreResult Evaluate(const Entry &entry,
                       const evidence::EntryEvidence &evidence);

  const ScoringConfig &GetConfig() const { return m_config; }

private:
  ScoringConfig m_config;
  std::unique_ptr<IScoringStrategy> m_strategy;
  std::vector<std::unique_ptr<ISignalEvaluator>> m_activeEvaluators;

  void InitializeEvaluators();
  void InitializeStrategy();
};

} // namespace scoring
} // namespace uboot
