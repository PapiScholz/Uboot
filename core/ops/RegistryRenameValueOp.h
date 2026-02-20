#pragma once

#include "../tx/IOperation.h"
#include <optional>
#include <string>
#include <vector>
#include <windows.h>


namespace uboot {
namespace ops {

class RegistryRenameValueOp : public IOperation {
public:
  enum class Action {
    Disable, // Rename to ...__UBOOT_DISABLED__<id> OR Move to UbootDisabled
             // subkey
    Enable   // Rename back to original OR Move back from UbootDisabled subkey
  };

  RegistryRenameValueOp(const std::string &fullKeyPath,
                        const std::string &valueName, Action action);

  // IOperation implementation
  bool prepare(const std::string &txBasePath) override;
  bool apply() override;
  bool verify() override;
  bool rollback(const std::string &txBasePath) override;

  std::string getId() const override;
  std::string getDescription() const override;
  void toJson(rapidjson::Value &outObj,
              rapidjson::Document::AllocatorType &allocator) const override;

private:
  std::string fullKeyPath; // e.g. HKLM\Software\...\Run
  std::string valueName;
  Action action;
  std::string opId;

  // State
  std::string
      keyPath; // without hive prefix? or including? Let's say we normalize.
  HKEY rootKey;

  // For standard rename strategy:
  std::string disabledName;

  // For "Move" strategy (Run keys):
  bool useMoveMethod = false;  // True for Run/RunOnce keys
  std::string disabledKeyPath; // ...\Run\UbootDisabled

  // Helpers
  HKEY OpenRegKey(const std::string &path, REGSAM access);
  std::string getDisabledName(const std::string &txId);
  bool renameValue(const std::string &key, const std::string &oldVal,
                   const std::string &newVal);
  bool moveValue(const std::string &srcKey, const std::string &srcVal,
                 const std::string &dstKey, const std::string &dstVal);
  bool deleteValue(const std::string &key, const std::string &val);
};

} // namespace ops
} // namespace uboot
