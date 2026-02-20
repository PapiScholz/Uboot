#include "OperationFactory.h"
#include "RegistryRenameValueOp.h"
#include "ServiceStartTypeOp.h"
#include "TaskToggleEnabledOp.h"
#include <iostream>

namespace uboot::ops {

using namespace uboot::ops; // Namespace of concrete ops

std::unique_ptr<IOperation>
OperationFactory::CreatefromJson(const rapidjson::Value &obj) {
  if (!obj.HasMember("type") || !obj["type"].IsString()) {
    return nullptr;
  }

  std::string type = obj["type"].GetString();

  if (type == "RegistryRenameValue") {
    std::string keyPath = "";
    std::string valueName = "";
    if (obj.HasMember("target") && obj["target"].IsString()) {
      // target might be full path "HKLM\\...\\ValueName"
      // My implementation of toJson stores "target" as combined string?
      // Let's check toJson impl in RegistryRenameValueOp.cpp
      // It stores "key" AND "valueName".
      if (obj.HasMember("key") && obj["key"].IsString())
        keyPath = obj["key"].GetString();
      if (obj.HasMember("valueName") && obj["valueName"].IsString())
        valueName = obj["valueName"].GetString();
    }

    std::string actionStr = "";
    if (obj.HasMember("action") && obj["action"].IsString())
      actionStr = obj["action"].GetString();

    RegistryRenameValueOp::Action action =
        (actionStr == "disable") ? RegistryRenameValueOp::Action::Disable
                                 : RegistryRenameValueOp::Action::Enable;
    return std::make_unique<RegistryRenameValueOp>(keyPath, valueName, action);
  } else if (type == "ServiceStartType") {
    std::string serviceName = "";
    if (obj.HasMember("serviceName") && obj["serviceName"].IsString())
      serviceName = obj["serviceName"].GetString();

    std::string actionStr = "";
    if (obj.HasMember("action") && obj["action"].IsString())
      actionStr = obj["action"].GetString();

    ServiceStartTypeOp::Action action =
        (actionStr == "disable") ? ServiceStartTypeOp::Action::Disable
                                 : ServiceStartTypeOp::Action::Enable;
    return std::make_unique<ServiceStartTypeOp>(serviceName, action);
  } else if (type == "TaskToggleEnabled") {
    std::string taskPath = "";
    if (obj.HasMember("taskPath") && obj["taskPath"].IsString())
      taskPath = obj["taskPath"].GetString();

    std::string actionStr = "";
    if (obj.HasMember("action") && obj["action"].IsString())
      actionStr = obj["action"].GetString();

    TaskToggleEnabledOp::Action action =
        (actionStr == "disable") ? TaskToggleEnabledOp::Action::Disable
                                 : TaskToggleEnabledOp::Action::Enable;
    return std::make_unique<TaskToggleEnabledOp>(taskPath, action);
  }

  return nullptr;
}

} // namespace uboot::ops
