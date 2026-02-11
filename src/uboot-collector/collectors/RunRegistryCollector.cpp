#include "RunRegistryCollector.h"
#include "../util/CommandResolver.h"
#include <windows.h>
#include <algorithm>
#include <sstream>

namespace uboot {

CollectorResult RunRegistryCollector::Collect() {
    CollectorResult result;
    std::string scope;
    
    // HKEY_CURRENT_USER - Run keys
    scope = "User";
    CollectFromKey(scope, HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", result.entries, result.errors);
    CollectFromKey(scope, HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", result.entries, result.errors);
    
    // HKEY_LOCAL_MACHINE - Run keys (32-bit view and 64-bit view)
    scope = "Machine";
    
    // 32-bit view
    CollectFromKey(scope, HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", result.entries, result.errors);
    CollectFromKey(scope, HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", result.entries, result.errors);
    
    // 64-bit view (only on 64-bit Windows)
#ifdef _WIN64
    CollectFromKey(scope, HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", result.entries, result.errors);
    CollectFromKey(scope, HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", result.entries, result.errors);
#endif
    
    // Sort entries deterministically
    std::sort(result.entries.begin(), result.entries.end(), [](const Entry& a, const Entry& b) {
        if (a.scope != b.scope) return a.scope < b.scope;
        if (a.location != b.location) return a.location < b.location;
        return a.key < b.key;
    });
    
    return result;
}

void RunRegistryCollector::CollectFromKey(
    const std::string& scope,
    HKEY hive,
    const std::string& subkey,
    std::vector<Entry>& outEntries,
    std::vector<CollectorError>& outErrors
) {
    // Convert UTF-8 subkey to UTF-16
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, subkey.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) {
        outErrors.push_back(CollectorError(GetName(), "Failed to convert subkey to UTF-16", GetLastError()));
        return;
    }
    
    std::wstring wideSubkey(wideLen - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, subkey.c_str(), -1, &wideSubkey[0], wideLen);
    
    HKEY keyHandle = nullptr;
    LONG status = RegOpenKeyExW(hive, wideSubkey.c_str(), 0, KEY_READ, &keyHandle);
    
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
        // Key doesn't exist, not an error
        return;
    }
    
    if (status != ERROR_SUCCESS) {
        outErrors.push_back(CollectorError(GetName(), "RegOpenKeyEx failed for " + subkey, status));
        return;
    }
    
    EnumerateRegistry(scope, keyHandle, subkey, GetName(), outEntries, outErrors);
    
    RegCloseKey(keyHandle);
}

void RunRegistryCollector::EnumerateRegistry(
    const std::string& scope,
    HKEY keyHandle,
    const std::string& keyPath,
    const std::string& source,
    std::vector<Entry>& outEntries,
    std::vector<CollectorError>& outErrors
) {
    DWORD valueIndex = 0;
    wchar_t valueName[256] = {};
    DWORD valueNameSize = sizeof(valueName) / sizeof(valueName[0]);
    wchar_t valueData[4096] = {};
    DWORD valueDataSize = sizeof(valueData);
    DWORD valueType = 0;
    
    while (true) {
        valueNameSize = sizeof(valueName) / sizeof(valueName[0]);
        valueDataSize = sizeof(valueData);
        
        LONG status = RegEnumValueW(keyHandle, valueIndex, valueName, &valueNameSize, nullptr, &valueType, (LPBYTE)valueData, &valueDataSize);
        
        if (status == ERROR_NO_MORE_ITEMS) {
            break;
        }
        
        if (status != ERROR_SUCCESS) {
            outErrors.push_back(CollectorError(source, "RegEnumValue failed at index " + std::to_string(valueIndex), status));
            valueIndex++;
            continue;
        }
        
        // Only process REG_SZ (string) values
        if (valueType != REG_SZ) {
            valueIndex++;
            continue;
        }
        
        // Convert wide strings to UTF-8
        int nameLen = WideCharToMultiByte(CP_UTF8, 0, valueName, -1, nullptr, 0, nullptr, nullptr);
        std::string entryName(nameLen - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, valueName, -1, &entryName[0], nameLen, nullptr, nullptr);
        
        int cmdLen = WideCharToMultiByte(CP_UTF8, 0, valueData, -1, nullptr, 0, nullptr, nullptr);
        std::string command(cmdLen - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, valueData, -1, &command[0], cmdLen, nullptr, nullptr);
        
        // Create Entry object
        Entry entry(
            source,      // source = "RunRegistry"
            scope,       // scope = "User" or "Machine"
            entryName,   // key = registry value name
            keyPath,     // location = registry key path
            "",          // arguments (to be resolved)
            ""           // imagePath (to be resolved)
        );
        
        // Resolve command using CommandResolver
        CommandResolver::PopulateEntryCommand(entry, command, "");
        entry.keyName = entryName;
        
        outEntries.push_back(entry);
        
        valueIndex++;
    }
}

} // namespace uboot
