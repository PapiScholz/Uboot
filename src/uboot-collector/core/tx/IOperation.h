#pragma once

#include "rapidjson/document.h"
#include <string>
#include <string_view>


class IOperation {
public:
  virtual ~IOperation() = default;

  // Phase 1: Prepare
  // creating backups, validating preconditions
  virtual bool prepare(const std::string &txBasePath) = 0;

  // Phase 2: Apply
  // executing the change
  virtual bool apply() = 0;

  // Phase 3: Verify
  // checking if the change was applied correctly
  // MVP: "Verifying configuration applied"
  virtual bool verify() = 0;

  // Rollback/Undo
  // Reverting the change using backup or inverse operation
  virtual bool rollback(const std::string &txBasePath) = 0;

  // Metadata
  virtual std::string getId() const = 0;
  virtual std::string getDescription() const = 0;

  // Serialization for Manifest
  virtual void toJson(rapidjson::Value &outObj,
                      rapidjson::Document::AllocatorType &allocator) const = 0;
};
