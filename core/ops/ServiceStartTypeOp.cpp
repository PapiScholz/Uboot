#include "ServiceStartTypeOp.h"
#include "../hardening/CriticalList.h"
#include "../hardening/DependencyAnalyzer.h"
#include <iostream>
#include <sstream>
#include <vector>


#ifdef _WIN32
#include <windows.h>
#include <winsvc.h>
#endif

namespace uboot::ops {

ServiceStartTypeOp::ServiceStartTypeOp(const std::string &name, Action act)
    : serviceName(name), action(act) {
  // Random ID
  static const char chars[] = "0123456789abcdef";
  for (int i = 0; i < 8; ++i)
    opId += chars[rand() % 16];
}

bool ServiceStartTypeOp::prepare(const std::string &txBasePath) {
  // 1. Hardening: Critical List
  if (hardening::CriticalList::IsCriticalService(serviceName)) {
    std::cerr << hardening::CriticalList::GetWarningMessage(serviceName)
              << "\n";
    // In a real run, this should block unless overridden.
    // For MVP, we log and maybe return false if enforcement is strict.
    return false;
  }

  // 2. Hardening: Dependencies
  if (action == Action::Disable) {
    auto deps =
        hardening::DependencyAnalyzer::GetDependentServices(serviceName);
    if (!deps.empty()) {
      std::cerr << "WARNING: Service " << serviceName << " has dependents:\n";
      for (const auto &d : deps)
        std::cerr << "  - " << d << "\n";
      // Warning, not fail.
    }
  }

  // 3. Backup Config (Query current)
  return queryConfig(originalStartType, originalBinaryPath);
}

bool ServiceStartTypeOp::apply() {
#ifdef _WIN32
  DWORD target =
      (action == Action::Disable) ? SERVICE_DISABLED : originalStartType;
  if (action == Action::Enable && originalStartType == 0)
    target = SERVICE_DEMAND_START; // Default fallback
  return setStartType(target);
#else
  return true;
#endif
}

bool ServiceStartTypeOp::verify() {
#ifdef _WIN32
  int currentType;
  std::string currentPath;
  if (!queryConfig(currentType, currentPath))
    return false;

  DWORD target =
      (action == Action::Disable) ? SERVICE_DISABLED : originalStartType;
  return currentType == target;
#else
  return true;
#endif
}

bool ServiceStartTypeOp::rollback(const std::string &txBasePath) {
  return setStartType(originalStartType);
}

std::string ServiceStartTypeOp::getId() const { return opId; }

std::string ServiceStartTypeOp::getDescription() const {
  return (action == Action::Disable ? "Disable Service " : "Enable Service ") +
         serviceName;
}

void ServiceStartTypeOp::toJson(
    rapidjson::Value &outObj,
    rapidjson::Document::AllocatorType &allocator) const {
  outObj.AddMember("type", "ServiceStartType", allocator);
  outObj.AddMember("service", rapidjson::Value(serviceName.c_str(), allocator),
                   allocator);
  outObj.AddMember(
      "action",
      rapidjson::Value(action == Action::Disable ? "disable" : "enable",
                       allocator),
      allocator);
  outObj.AddMember("original_start_type", originalStartType, allocator);
  outObj.AddMember("op_id", rapidjson::Value(opId.c_str(), allocator),
                   allocator);
}

bool ServiceStartTypeOp::queryConfig(int &startType, std::string &binPath) {
#ifdef _WIN32
  SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  if (!hSCManager)
    return false;

  SC_HANDLE hService =
      OpenServiceA(hSCManager, serviceName.c_str(), SERVICE_QUERY_CONFIG);
  if (!hService) {
    CloseServiceHandle(hSCManager);
    return false;
  }

  DWORD bytesNeeded = 0;
  QueryServiceConfigA(hService, NULL, 0, &bytesNeeded);
  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    std::vector<BYTE> buffer(bytesNeeded);
    LPQUERY_SERVICE_CONFIGA config =
        reinterpret_cast<LPQUERY_SERVICE_CONFIGA>(buffer.data());
    if (QueryServiceConfigA(hService, config, bytesNeeded, &bytesNeeded)) {
      startType = config->dwStartType;
      binPath = config->lpBinaryPathName ? config->lpBinaryPathName : "";
      CloseServiceHandle(hService);
      CloseServiceHandle(hSCManager);
      return true;
    }
  }

  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);
#endif
  return false;
}

bool ServiceStartTypeOp::setStartType(int startType) {
#ifdef _WIN32
  SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  if (!hSCManager)
    return false;

  SC_HANDLE hService =
      OpenServiceA(hSCManager, serviceName.c_str(), SERVICE_CHANGE_CONFIG);
  if (!hService) {
    CloseServiceHandle(hSCManager);
    return false;
  }

  // Use A version explicitly
  BOOL res = ChangeServiceConfigA(hService, SERVICE_NO_CHANGE, startType,
                                  SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL);

  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);
  return res != 0;
#endif
  return true;
}

} // namespace uboot::ops
