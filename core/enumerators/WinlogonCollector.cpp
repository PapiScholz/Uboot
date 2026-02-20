#include "WinlogonCollector.h"
#include "../util/CommandResolver.h"
#include <windows.h>
#include <algorithm>

namespace uboot {

CollectorResult WinlogonCollector::Collect() {
    CollectorResult result;
    std::string subkey = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
    
    // HKEY_CURRENT_USER
    CollectFromWinlogonKey("User", HKEY_CURRENT_USER, result.entries, result.errors);
    
    // HKEY_LOCAL_MACHINE
    CollectFromWinlogonKey("Machine", HKEY_LOCAL_MACHINE, result.entries, result.errors);
    
    // Sort entries deterministically
    std::sort(result.entries.begin(), result.entries.end(), [](const Entry& a, const Entry& b) {
        if (a.scope != b.scope) return a.scope < b.scope;
        return a.key < b.key;
    });
    
    return result;
}

void WinlogonCollector::CollectFromWinlogonKey(
    const std::string& scope,
    HKEY hive,
    std::vector<Entry>& outEntries,
    std::vector<CollectorError>& outErrors
) {
    std::string subkey = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
    
    // Convert UTF-8 subkey to UTF-16
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, subkey.c_str(), -1, nullptr, 0);
    std::wstring wideSubkey(wideLen - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, subkey.c_str(), -1, &wideSubkey[0], wideLen);
    
    HKEY keyHandle = nullptr;
    LONG status = RegOpenKeyExW(hive, wideSubkey.c_str(), 0, KEY_READ, &keyHandle);
    
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
        return;  // Key doesn't exist, not an error
    }
    
    if (status != ERROR_SUCCESS) {
        outErrors.push_back(CollectorError(GetName(), "RegOpenKeyEx failed for Winlogon", status));
        return;
    }
    
    // List of values to check
    const char* valuesToCheck[] = {"Shell", "Userinit"};
    
    for (const char* valueName : valuesToCheck) {
        // Convert value name to UTF-16
        int valWideLen = MultiByteToWideChar(CP_UTF8, 0, valueName, -1, nullptr, 0);
        std::wstring wideValueName(valWideLen - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, valueName, -1, &wideValueName[0], valWideLen);
        
        wchar_t valueData[4096] = {};
        DWORD valueDataSize = sizeof(valueData);
        DWORD valueType = 0;
        
        LONG regStatus = RegQueryValueExW(keyHandle, wideValueName.c_str(), nullptr, &valueType, (LPBYTE)valueData, &valueDataSize);
        
        if (regStatus == ERROR_SUCCESS && valueType == REG_SZ) {
            // Convert value data to UTF-8
            int dataLen = WideCharToMultiByte(CP_UTF8, 0, valueData, -1, nullptr, 0, nullptr, nullptr);
            std::string command(dataLen - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, valueData, -1, &command[0], dataLen, nullptr, nullptr);
            
            // Create Entry
            Entry entry(
                GetName(),           // source = "Winlogon"
                scope,               // scope = "User" or "Machine"
                std::string(valueName), // key = registry value name
                subkey,              // location = registry key path
                "",                  // arguments
                ""                   // imagePath
            );
            
            // Resolve command
            CommandResolver::PopulateEntryCommand(entry, command, "");
            
            outEntries.push_back(entry);
        }
    }
    
    RegCloseKey(keyHandle);
}

} // namespace uboot
