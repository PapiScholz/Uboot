#include "ServicesCollector.h"
#include "../util/CommandResolver.h"
#include <windows.h>
#include <winsvc.h>
#include <algorithm>

namespace uboot {

CollectorResult ServicesCollector::Collect() {
    CollectorResult result;
    EnumerateServices(result.entries, result.errors);
    
    // Sort entries deterministically
    std::sort(result.entries.begin(), result.entries.end(), [](const Entry& a, const Entry& b) {
        if (a.key < b.key) return true;
        if (a.key > b.key) return false;
        return a.arguments < b.arguments;
    });
    
    return result;
}

void ServicesCollector::EnumerateServices(
    std::vector<Entry>& outEntries,
    std::vector<CollectorError>& outErrors
) {
    // Open connection to SCM
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scmHandle) {
        outErrors.push_back(CollectorError(GetName(), "Failed to open SCM", GetLastError()));
        return;
    }
    
    DWORD bytesNeeded = 0;
    DWORD servicesReturned = 0;
    DWORD resumeHandle = 0;
    ENUM_SERVICE_STATUS_PROCESSW* services = nullptr;
    
    // First call to get required buffer size
    if (!EnumServicesStatusExW(
        scmHandle,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,  // Only win32 services
        SERVICE_STATE_ALL,
        nullptr,
        0,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        nullptr
    )) {
        if (GetLastError() != ERROR_MORE_DATA) {
            outErrors.push_back(CollectorError(GetName(), "EnumServicesStatusEx failed in size query", GetLastError()));
            CloseServiceHandle(scmHandle);
            return;
        }
    }
    
    // Allocate buffer
    services = (ENUM_SERVICE_STATUS_PROCESSW*)LocalAlloc(LMEM_FIXED, bytesNeeded);
    if (!services) {
        outErrors.push_back(CollectorError(GetName(), "Failed to allocate buffer for services", GetLastError()));
        CloseServiceHandle(scmHandle);
        return;
    }
    
    // Enumerate services
    resumeHandle = 0;
    do {
        servicesReturned = 0;
        
        if (!EnumServicesStatusExW(
            scmHandle,
            SC_ENUM_PROCESS_INFO,
            SERVICE_WIN32,
            SERVICE_STATE_ALL,
            (LPBYTE)services,
            bytesNeeded,
            &bytesNeeded,
            &servicesReturned,
            &resumeHandle,
            nullptr
        )) {
            if (GetLastError() != ERROR_MORE_DATA && GetLastError() != ERROR_NO_MORE_ITEMS) {
                outErrors.push_back(CollectorError(GetName(), "EnumServicesStatusEx failed", GetLastError()));
            }
            break;
        }
        
        // Process each service
        for (DWORD i = 0; i < servicesReturned; i++) {
            ENUM_SERVICE_STATUS_PROCESSW& svc = services[i];
            
            // Convert service name to UTF-8
            int nameLen = WideCharToMultiByte(CP_UTF8, 0, svc.lpServiceName, -1, nullptr, 0, nullptr, nullptr);
            std::string serviceName(nameLen - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, svc.lpServiceName, -1, &serviceName[0], nameLen, nullptr, nullptr);
            
            // Convert display name to UTF-8
            int dispLen = WideCharToMultiByte(CP_UTF8, 0, svc.lpDisplayName, -1, nullptr, 0, nullptr, nullptr);
            std::string displayName(dispLen - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, svc.lpDisplayName, -1, &displayName[0], dispLen, nullptr, nullptr);
            
            // Open service to get detailed config
            SC_HANDLE serviceHandle = OpenServiceW(scmHandle, svc.lpServiceName, SERVICE_QUERY_CONFIG);
            if (!serviceHandle) {
                outErrors.push_back(CollectorError(GetName(), "Failed to open service: " + serviceName, GetLastError()));
                continue;
            }
            
            // Get service config
            DWORD configBytesNeeded = 0;
            if (!QueryServiceConfigW(serviceHandle, nullptr, 0, &configBytesNeeded)) {
                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                    outErrors.push_back(CollectorError(GetName(), "Failed to query service config for: " + serviceName, GetLastError()));
                    CloseServiceHandle(serviceHandle);
                    continue;
                }
            }
            
            QUERY_SERVICE_CONFIGW* config = (QUERY_SERVICE_CONFIGW*)LocalAlloc(LMEM_FIXED, configBytesNeeded);
            if (!config) {
                outErrors.push_back(CollectorError(GetName(), "Failed to allocate config buffer for: " + serviceName, GetLastError()));
                CloseServiceHandle(serviceHandle);
                continue;
            }
            
            if (!QueryServiceConfigW(serviceHandle, config, configBytesNeeded, &configBytesNeeded)) {
                outErrors.push_back(CollectorError(GetName(), "QueryServiceConfigW failed for: " + serviceName, GetLastError()));
                LocalFree(config);
                CloseServiceHandle(serviceHandle);
                continue;
            }
            
            // Convert binary path name to UTF-8
            int pathLen = WideCharToMultiByte(CP_UTF8, 0, config->lpBinaryPathName, -1, nullptr, 0, nullptr, nullptr);
            std::string binaryPath(pathLen - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, config->lpBinaryPathName, -1, &binaryPath[0], pathLen, nullptr, nullptr);
            
            // Create Entry
            Entry entry(
                GetName(),       // source = "Services"
                "Machine",       // scope = "Machine"
                serviceName,     // key = service name
                "",              // location (empty for services)
                "",              // arguments (to be resolved)
                ""               // imagePath (to be resolved)
            );
            
            // Resolve command
            CommandResolver::PopulateEntryCommand(entry, binaryPath, "");
            entry.displayName = displayName;
            
            outEntries.push_back(entry);
            
            LocalFree(config);
            CloseServiceHandle(serviceHandle);
        }
        
        if (GetLastError() != ERROR_MORE_DATA) {
            break;
        }
    } while (true);
    
    LocalFree(services);
    CloseServiceHandle(scmHandle);
}

} // namespace uboot
