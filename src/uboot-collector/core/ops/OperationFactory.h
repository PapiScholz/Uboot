#pragma once

#include "../tx/IOperation.h"
#include "rapidjson/document.h"
#include <memory>

namespace uboot::ops {

class OperationFactory {
public:
  static std::unique_ptr<IOperation>
  CreatefromJson(const rapidjson::Value &obj);
};

} // namespace uboot::ops
