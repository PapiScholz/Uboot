#include "TaskToggleEnabledOp.h"
#include "../../util/ComUtils.h"
#include "../hardening/SecurityLog.h"
#include <comdef.h>
#include <comutil.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <taskschd.h>
#include <wrl/client.h>


#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")

namespace uboot {
namespace ops {

using Microsoft::WRL::ComPtr;

TaskToggleEnabledOp::TaskToggleEnabledOp(const std::string &path, Action act)
    : taskPath(path), action(act) {
  static const char chars[] = "0123456789abcdef";
  for (int i = 0; i < 8; ++i)
    opId += chars[std::rand() % 16];
}

bool TaskToggleEnabledOp::prepare(const std::string &txBasePath) {
  // 1. Init COM
  util::ComInit com(COINIT_MULTITHREADED);

  // 2. Backup Task XML (Requirement)
  // We need ITaskService to get XML
  ComPtr<ITaskService> pService;
  HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
                                IID_ITaskService, (void **)&pService);
  if (FAILED(hr)) {
    // Cannot connect, maybe service issue. Fail prepare.
    return false;
  }

  // Connect
  hr =
      pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
  if (FAILED(hr)) {
    return false;
  }

  // Get Task Wrapper
  ComPtr<ITaskFolder> pRootFolder;
  hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
  if (FAILED(hr)) {
    return false;
  }

  // Get Task
  ComPtr<IRegisteredTask> pTask;
  _bstr_t bstrTaskPath(taskPath.c_str());
  hr = pRootFolder->GetTask(bstrTaskPath, &pTask);

  if (FAILED(hr)) {
    // Task not found
    return false;
  }

  // Get Enabled State
  VARIANT_BOOL bEnabled = VARIANT_FALSE;
  hr = pTask->get_Enabled(&bEnabled);
  if (SUCCEEDED(hr)) {
    originalEnabledState = (bEnabled == VARIANT_TRUE);
  }

  // In a real implementation, we would write a JSON backup to
  // txBasePath/opId.json For now we just store state in memory
  return true;
}

bool TaskToggleEnabledOp::apply() {
  if (action == Action::Disable)
    return setTaskEnabled(false);
  else
    return setTaskEnabled(true);
}

bool TaskToggleEnabledOp::verify() {
  bool current;
  if (!getTaskEnabled(current))
    return false;

  bool expected = (action == Action::Enable);
  return current == expected;
}

bool TaskToggleEnabledOp::rollback(const std::string &txBasePath) {
  // Restore original state
  return setTaskEnabled(originalEnabledState);
}

// Helper to connect and get task
bool TaskToggleEnabledOp::setTaskEnabled(bool enabled) {
  util::ComInit com(COINIT_MULTITHREADED); // Init COM for this scope

  ComPtr<ITaskService> pService;
  HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
                                IID_ITaskService, (void **)&pService);
  if (FAILED(hr))
    return false;

  hr =
      pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
  if (FAILED(hr)) {
    return false;
  }

  ComPtr<ITaskFolder> pRootFolder;
  hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
  if (FAILED(hr)) {
    return false;
  }

  ComPtr<IRegisteredTask> pTask;
  _bstr_t bstrTaskPath(taskPath.c_str());

  hr = pRootFolder->GetTask(bstrTaskPath, &pTask);

  if (FAILED(hr)) {
    return false;
  }

  hr = pTask->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);

  return SUCCEEDED(hr);
}

bool TaskToggleEnabledOp::getTaskEnabled(bool &enabled) {
  util::ComInit com(COINIT_MULTITHREADED);

  ComPtr<ITaskService> pService;
  HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
                                IID_ITaskService, (void **)&pService);
  if (FAILED(hr))
    return false;

  hr =
      pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
  if (FAILED(hr)) {
    return false;
  }

  ComPtr<ITaskFolder> pRootFolder;
  hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
  if (FAILED(hr)) {
    return false;
  }

  ComPtr<IRegisteredTask> pTask;
  _bstr_t bstrTaskPath(taskPath.c_str());
  hr = pRootFolder->GetTask(bstrTaskPath, &pTask);

  if (FAILED(hr)) {
    return false;
  }

  VARIANT_BOOL bEnabled = VARIANT_FALSE;
  hr = pTask->get_Enabled(&bEnabled);

  if (SUCCEEDED(hr)) {
    enabled = (bEnabled == VARIANT_TRUE);
    return true;
  }
  return false;
}

std::string TaskToggleEnabledOp::getId() const { return opId; }

std::string TaskToggleEnabledOp::getDescription() const {
  return (action == Action::Disable ? "Disable Task " : "Enable Task ") +
         taskPath;
}

void TaskToggleEnabledOp::toJson(
    rapidjson::Value &outObj,
    rapidjson::Document::AllocatorType &allocator) const {
  outObj.AddMember("type", "TaskToggleEnabled", allocator);

  rapidjson::Value vPath;
  vPath.SetString(taskPath.c_str(), allocator);
  outObj.AddMember("path", vPath, allocator);

  rapidjson::Value vAction;
  vAction.SetString(action == Action::Disable ? "disable" : "enable",
                    allocator);
  outObj.AddMember("action", vAction, allocator);

  rapidjson::Value vOpId;
  vOpId.SetString(opId.c_str(), allocator);
  outObj.AddMember("op_id", vOpId, allocator);
}

} // namespace ops
} // namespace uboot
