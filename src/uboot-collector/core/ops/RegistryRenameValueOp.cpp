#include "RegistryRenameValueOp.h"
#include "../hardening/PolicyInspector.h"
#include "../hardening/SecurityLog.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#ifdef _WIN32
#include <shlwapi.h>
#include <windows.h>
#pragma comment(lib, "shlwapi.lib")
#endif

namespace uboot {
namespace ops {

RegistryRenameValueOp::RegistryRenameValueOp(const std::string &fullPath,
                                             const std::string &val, Action act)
    : fullKeyPath(fullPath), valueName(val), action(act) {

  static const char chars[] = "0123456789abcdef";
  for (int i = 0; i < 8; ++i)
    opId += chars[std::rand() % 16];

  // Parse root key and subkey
  std::string p = fullKeyPath;
  if (p.find("HKLM\\") == 0 || p.find("HKEY_LOCAL_MACHINE\\") == 0) {
    auto pos = p.find('\\');
    rootKey = HKEY_LOCAL_MACHINE;
    keyPath = p.substr(pos + 1);
  } else if (p.find("HKCU\\") == 0 || p.find("HKEY_CURRENT_USER\\") == 0) {
    auto pos = p.find('\\');
    rootKey = HKEY_CURRENT_USER;
    keyPath = p.substr(pos + 1);
  } else {
    // Default or fail? Assume HKLM for now if no prefix or handle it.
    // In practice, collector sends full path.
    // For this refactor, let's assume valid input or handle logic.
    rootKey = HKEY_LOCAL_MACHINE;
    keyPath = fullKeyPath;
  }
}

std::string RegistryRenameValueOp::getDisabledName(const std::string &txId) {
  return valueName + "__UBOOT_DISABLED__" + txId;
}

bool RegistryRenameValueOp::prepare(const std::string &txBasePath) {
  // 1. Detect Run/RunOnce
  std::string lowerKey = keyPath;
  std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lowerKey.find("\\currentversion\\run") != std::string::npos ||
      lowerKey.find("\\currentversion\\runonce") != std::string::npos) {
    useMoveMethod = true;
    disabledKeyPath = keyPath + "\\UbootDisabled";
  } else {
    useMoveMethod = false;
    disabledName = getDisabledName(opId);
  }

  // 2. Check if value exists
  HKEY hKey = OpenRegKey(keyPath, KEY_READ);
  if (!hKey)
    return false; // Key doesn't exist

  // Check value
  DWORD type;
  LSTATUS status = RegQueryValueExA(hKey, valueName.c_str(), 0, &type, 0, 0);
  RegCloseKey(hKey);

  return (status == ERROR_SUCCESS);
}

bool RegistryRenameValueOp::apply() {
  if (action == Action::Disable) {
    if (useMoveMethod) {
      // Create/Open subkey UbootDisabled
      HKEY hKey;
      if (RegCreateKeyExA(rootKey, disabledKeyPath.c_str(), 0, NULL,
                          REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey,
                          NULL) != ERROR_SUCCESS) {
        return false;
      }
      RegCloseKey(hKey);

      return moveValue(keyPath, valueName, disabledKeyPath, valueName) &&
             deleteValue(keyPath, valueName);
    } else {
      return renameValue(keyPath, valueName, disabledName);
    }
  } else { // Enable
    if (useMoveMethod) {
      return moveValue(disabledKeyPath, valueName, keyPath, valueName) &&
             deleteValue(disabledKeyPath, valueName);
    } else {
      return renameValue(keyPath, disabledName, valueName);
    }
  }
}

bool RegistryRenameValueOp::verify() {
  // If Disable: value should NOT exist in keyPath (and EXIST in
  // disabledKeyPath/disabledName) If Enable: value SHOULD exist in keyPath

  HKEY hKey = OpenRegKey(keyPath, KEY_READ);
  bool existsInSource = false;
  if (hKey) {
    existsInSource = (RegQueryValueExA(hKey, valueName.c_str(), 0, NULL, NULL,
                                       NULL) == ERROR_SUCCESS);
    RegCloseKey(hKey);
  }

  if (action == Action::Disable) {
    if (existsInSource)
      return false;

    // Check destination
    if (useMoveMethod) {
      HKEY hDest = OpenRegKey(disabledKeyPath, KEY_READ);
      bool existsInDest = false;
      if (hDest) {
        existsInDest = (RegQueryValueExA(hDest, valueName.c_str(), 0, NULL,
                                         NULL, NULL) == ERROR_SUCCESS);
        RegCloseKey(hDest);
      }
      return existsInDest;
    } else {
      // Check renamed
      HKEY hSrc = OpenRegKey(keyPath, KEY_READ); // re-open
      if (!hSrc)
        return false;
      bool renamedExists =
          (RegQueryValueExA(hSrc, disabledName.c_str(), 0, NULL, NULL, NULL) ==
           ERROR_SUCCESS);
      RegCloseKey(hSrc);
      return renamedExists;
    }
  } else { // Enable
    return existsInSource;
  }
}

bool RegistryRenameValueOp::rollback(const std::string &txBasePath) {
  // Reverse of apply
  if (action == Action::Disable) {
    // We disabled it, so rollback means Enable
    if (useMoveMethod) {
      return moveValue(disabledKeyPath, valueName, keyPath, valueName) &&
             deleteValue(disabledKeyPath, valueName);
    } else {
      return renameValue(keyPath, disabledName, valueName);
    }
  } else {
    // We enabled it, so rollback means Disable
    if (useMoveMethod) {
      // ... create key ...
      return moveValue(keyPath, valueName, disabledKeyPath, valueName) &&
             deleteValue(keyPath, valueName);
    } else {
      return renameValue(keyPath, valueName, disabledName);
    }
  }
}

std::string RegistryRenameValueOp::getId() const { return opId; }

std::string RegistryRenameValueOp::getDescription() const {
  return "RegistryRename " + fullKeyPath + "\\" + valueName + " -> " +
         (action == Action::Disable ? "DISABLE" : "ENABLE");
}

void RegistryRenameValueOp::toJson(
    rapidjson::Value &outObj,
    rapidjson::Document::AllocatorType &allocator) const {
  outObj.AddMember("type", "RegistryRenameValue", allocator);
  outObj.AddMember("opId", rapidjson::Value(opId.c_str(), allocator),
                   allocator);
  outObj.AddMember("fullKeyPath",
                   rapidjson::Value(fullKeyPath.c_str(), allocator), allocator);
  outObj.AddMember("valueName", rapidjson::Value(valueName.c_str(), allocator),
                   allocator);
  outObj.AddMember(
      "action",
      rapidjson::Value(action == Action::Disable ? "Disable" : "Enable",
                       allocator),
      allocator);
}

// Helpers
HKEY RegistryRenameValueOp::OpenRegKey(const std::string &path, REGSAM access) {
  HKEY hKey;
  if (RegOpenKeyExA(rootKey, path.c_str(), 0, access, &hKey) == ERROR_SUCCESS) {
    return hKey;
  }
  return NULL;
}

bool RegistryRenameValueOp::moveValue(const std::string &srcKey,
                                      const std::string &srcVal,
                                      const std::string &dstKey,
                                      const std::string &dstVal) {
#ifdef _WIN32
  HKEY hSrc = OpenRegKey(srcKey, KEY_READ);
  if (!hSrc)
    return false;

  DWORD size = 0;
  DWORD type = 0;
  if (RegQueryValueExA(hSrc, srcVal.c_str(), NULL, &type, NULL, &size) !=
      ERROR_SUCCESS) {
    RegCloseKey(hSrc);
    return false;
  }

  std::vector<BYTE> data(size);
  if (RegQueryValueExA(hSrc, srcVal.c_str(), NULL, &type, data.data(), &size) !=
      ERROR_SUCCESS) {
    RegCloseKey(hSrc);
    return false;
  }
  RegCloseKey(hSrc);

  HKEY hDst = OpenRegKey(dstKey, KEY_WRITE);
  if (!hDst) {
    // Try to create if it doesn't exist
    if (RegCreateKeyExA(rootKey, dstKey.c_str(), 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hDst,
                        NULL) != ERROR_SUCCESS) {
      return false;
    }
  }

  if (RegSetValueExA(hDst, dstVal.c_str(), 0, type, data.data(), size) !=
      ERROR_SUCCESS) {
    RegCloseKey(hDst);
    return false;
  }
  RegCloseKey(hDst);
  return true;
#else
  return false;
#endif
}

bool RegistryRenameValueOp::renameValue(const std::string &key,
                                        const std::string &oldVal,
                                        const std::string &newVal) {
  return moveValue(key, oldVal, key, newVal) && deleteValue(key, oldVal);
}

bool RegistryRenameValueOp::deleteValue(const std::string &key,
                                        const std::string &value) {
#ifdef _WIN32
  HKEY hKey = OpenRegKey(key, KEY_SET_VALUE);
  if (!hKey)
    return false;
  LSTATUS res = RegDeleteValueA(hKey, value.c_str());
  RegCloseKey(hKey);
  return res == ERROR_SUCCESS;
#else
  return false;
#endif
}

} // namespace ops
} // namespace uboot
