#pragma once

#include "../tx/IOperation.h"
#include <comdef.h>
#include <string>
#include <taskschd.h>
#include <windows.h>


namespace uboot {
namespace ops {

class TaskToggleEnabledOp : public IOperation {
public:
  enum class Action { Disable, Enable };

  TaskToggleEnabledOp(const std::string &taskPath, Action action);

  bool prepare(const std::string &txBasePath) override;
  bool apply() override;
  bool verify() override;
  bool rollback(const std::string &txBasePath) override;

  std::string getId() const override;
  std::string getDescription() const override;
  void toJson(rapidjson::Value &outObj,
              rapidjson::Document::AllocatorType &allocator) const override;

private:
  std::string taskPath; // e.g., "\Microsoft\Windows\SomeTask"
  Action action;
  std::string opId;
  bool originalEnabledState = false;

  // COM Helper
  bool setTaskEnabled(bool enabled);
  bool getTaskEnabled(bool &enabled);
};

} // namespace ops
} // namespace uboot
