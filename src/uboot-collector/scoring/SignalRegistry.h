#pragma once

#include "ISignalEvaluator.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace uboot {
namespace scoring {

class SignalRegistry {
public:
  using EvaluatorPtr = std::unique_ptr<ISignalEvaluator>;
  using CreatorFunc = std::function<EvaluatorPtr()>;

  static SignalRegistry &Instance() {
    static SignalRegistry instance;
    return instance;
  }

  // Registry is immutable post-initialization and safe for concurrent reads.
  void Register(const std::string &signalId, CreatorFunc creator) {
    m_creators[signalId] = creator;
  }

  EvaluatorPtr Create(const std::string &signalId) {
    auto it = m_creators.find(signalId);
    if (it != m_creators.end()) {
      return it->second();
    }
    return nullptr;
  }

private:
  SignalRegistry() = default;
  std::map<std::string, CreatorFunc> m_creators;
};

} // namespace scoring
} // namespace uboot
