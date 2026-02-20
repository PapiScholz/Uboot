#include "IfeoDebuggerCollector.h"
#include "../util/CommandResolver.h"
#include <windows.h>
#include <algorithm>

namespace uboot {

CollectorResult IfeoDebuggerCollector::Collect() {
    CollectorResult result;
    std::string ifeoPath = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options";
    
    // Convert UTF-8 path to UTF-16
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, ifeoPath.c_str(), -1, nullptr, 0);
    std::wstring wideIfeoPath(wideLen - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, ifeoPath.c_str(), -1, &wideIfeoPath[0], wideLen);
    
    HKEY ifeoHandle = nullptr;
    LONG status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, wideIfeoPath.c_str(), 0, KEY_READ, &ifeoHandle);
    
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
        return result;  // Key doesn't exist
    }
    
    if (status != ERROR_SUCCESS) {
        result.errors.push_back(CollectorError(GetName(), "RegOpenKeyEx failed for IFEO", status));
        return result;
    }
    
    EnumerateIfeoSubkeys(ifeoHandle, ifeoPath, result.entries, result.errors);
    
    RegCloseKey(ifeoHandle);
    
    // Sort entries deterministically
    std::sort(result.entries.begin(), result.entries.end(), [](const Entry& a, const Entry& b) {
        return a.key < b.key;
    });
    
    return result;
}

void IfeoDebuggerCollector::EnumerateIfeoSubkeys(
    HKEY parentHandle,
    const std::string& parentPath,
    std::vector<Entry>& outEntries,
    std::vector<CollectorError>& outErrors
) {
    DWORD subkeyIndex = 0;
    wchar_t subkeyName[256] = {};
    DWORD subkeyNameSize = sizeof(subkeyName) / sizeof(subkeyName[0]);
    
    while (true) {
        subkeyNameSize = sizeof(subkeyName) / sizeof(subkeyName[0]);
        
        LONG status = RegEnumKeyW(parentHandle, subkeyIndex, subkeyName, subkeyNameSize);
        
        if (status == ERROR_NO_MORE_ITEMS) {
            break;
        }
        
        if (status != ERROR_SUCCESS) {
            outErrors.push_back(CollectorError(GetName(), "RegEnumKey failed", status));
            subkeyIndex++;
            continue;
        }
        
        // Convert subkey name to UTF-8
        int nameLen = WideCharToMultiByte(CP_UTF8, 0, subkeyName, -1, nullptr, 0, nullptr, nullptr);
        std::string executableName(nameLen - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, subkeyName, -1, &executableName[0], nameLen, nullptr, nullptr);
        
        // Open the subkey
        HKEY subkeyHandle = nullptr;
        status = RegOpenKeyExW(parentHandle, subkeyName, 0, KEY_READ, &subkeyHandle);
        
        if (status != ERROR_SUCCESS) {
            outErrors.push_back(CollectorError(GetName(), "Failed to open IFEO subkey: " + executableName, status));
            subkeyIndex++;
            continue;
        }
        
        // Look for "Debugger" value
        wchar_t debuggerValue[4096] = {};
        DWORD debuggerSize = sizeof(debuggerValue);
        DWORD valueType = 0;
        
        status = RegQueryValueExW(subkeyHandle, L"Debugger", nullptr, &valueType, (LPBYTE)debuggerValue, &debuggerSize);
        
        if (status == ERROR_SUCCESS && valueType == REG_SZ) {
            // Convert debugger command to UTF-8
            int cmdLen = WideCharToMultiByte(CP_UTF8, 0, debuggerValue, -1, nullptr, 0, nullptr, nullptr);
            std::string debuggerCommand(cmdLen - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, debuggerValue, -1, &debuggerCommand[0], cmdLen, nullptr, nullptr);
            
            // Create Entry
            Entry entry(
                GetName(),           // source = "IfeoDebugger"
                "Machine",           // scope = "Machine"
                executableName,      // key = executable name
                parentPath,          // location = IFEO registry path
                "",                  // arguments
                ""                   // imagePath
            );
            
            // Resolve command
            CommandResolver::PopulateEntryCommand(entry, debuggerCommand, "");
            
            outEntries.push_back(entry);
        }
        
        RegCloseKey(subkeyHandle);
        subkeyIndex++;
    }
}

} // namespace uboot
