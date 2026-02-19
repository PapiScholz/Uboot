#pragma once

#include "../evidence/entry_evidence.h"
#include "../model/Entry.h"
#include "Types.h"
#include <optional>
#include <string>
#include <vector>


namespace uboot {
namespace scoring {

/// <summary>
/// Interface for individual signal evaluation logic.
/// </summary>
class ISignalEvaluator {
public:
  virtual ~ISignalEvaluator() = default;

  /// <summary>
  /// Evaluate a specific signal against the entry.
  /// </summary>
  /// <param name="entry">The original entry</param>
  /// <param name="evidence">Collected evidence</param>
  /// <param name="signalConfig">Configuration for this specific signal (weight,
  /// etc)</param> <returns>SignalEvidence if triggered, nullopt
  /// otherwise</returns>
  virtual std::optional<SignalEvidence>
  Evaluate(const Entry &entry, const evidence::EntryEvidence &evidence,
           const Signal &signalConfig) = 0;

  /// <summary>
  /// The unique ID of the signal this evaluator handles.
  /// </summary>
  virtual std::string GetSupportedSignalId() const = 0;
};

} // namespace scoring
} // namespace uboot
