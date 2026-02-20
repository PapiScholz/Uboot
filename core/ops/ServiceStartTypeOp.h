#pragma once

#include "../tx/IOperation.h"
#include <string>

namespace uboot::ops {

class ServiceStartTypeOp : public IOperation {
public:
  enum class Action {
    Disable, // StartType = DISABLED
    Enable   // StartType = Original (or DEMAND_START)
  };

  ServiceStartTypeOp(const std::string &serviceName, Action action);

  bool prepare(const std::string &txBasePath) override;
  bool apply() override;
  bool verify() override;
  bool rollback(const std::string &txBasePath) override;

  std::string getId() const override;
  std::string getDescription() const override;
  void toJson(rapidjson::Value &outObj,
              rapidjson::Document::AllocatorType &allocator) const override;

private:
  std::string serviceName;
  Action action;
  std::string opId;

  // Original configuration
  int originalStartType = 0; // SERVICE_DEMAND_START etc.
  std::string originalBinaryPath;

  // Helpers
  bool queryConfig(int &startType, std::string &binPath);
  bool setStartType(int startType);
};

} // namespace uboot::ops
