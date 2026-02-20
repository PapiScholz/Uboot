#include "DependencyAnalyzer.h"
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winsvc.h>
#endif

namespace uboot::hardening {

std::vector<std::string>
DependencyAnalyzer::GetDependentServices(const std::string &serviceName) {
  std::vector<std::string> dependents;

#ifdef _WIN32
  SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  if (!hSCManager)
    return dependents;

  SC_HANDLE hService = OpenServiceA(hSCManager, serviceName.c_str(),
                                    SERVICE_ENUMERATE_DEPENDENTS);
  if (!hService) {
    CloseServiceHandle(hSCManager);
    return dependents;
  }

  DWORD bytesNeeded = 0;
  DWORD count = 0;

  // First call to get buffer size
  if (!EnumDependentServicesA(hService, SERVICE_ACTIVE | SERVICE_INACTIVE, NULL,
                              0, &bytesNeeded, &count)) {
    if (GetLastError() != ERROR_MORE_DATA) {
      // Real error
      CloseServiceHandle(hService);
      return dependents;
    }
  }

  if (bytesNeeded > 0) {
    std::vector<BYTE> buffer(bytesNeeded);
    LPENUM_SERVICE_STATUSA lpDependencies =
        reinterpret_cast<LPENUM_SERVICE_STATUSA>(buffer.data());

    if (EnumDependentServicesA(hService, SERVICE_ACTIVE | SERVICE_INACTIVE,
                               lpDependencies, bytesNeeded, &bytesNeeded,
                               &count)) {
      for (DWORD i = 0; i < count; i++) {
        dependents.push_back(lpDependencies[i].lpServiceName);
      }
    }
  }

  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);
#endif

  return dependents;
}

} // namespace uboot::hardening
