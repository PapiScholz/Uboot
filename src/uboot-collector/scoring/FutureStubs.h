#pragma once

#include <optional>
#include <string>


namespace uboot {
namespace scoring {
namespace future {

class IReputationProvider {
public:
  virtual ~IReputationProvider() = default;
  virtual int GetReputationScore(const std::string &sha256) = 0;
};

class IEntropyCalculator {
public:
  virtual ~IEntropyCalculator() = default;
  virtual double CalculateEntropy(const std::wstring &filePath) = 0;
};

class IHostBaseline {
public:
  virtual ~IHostBaseline() = default;
  virtual bool IsKnownGood(const std::string &entryKey) = 0;
};

} // namespace future
} // namespace scoring
} // namespace uboot
