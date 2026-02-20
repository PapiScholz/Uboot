#include "StartupFolderCollector.h"
#include "../resolve/LnkResolver.h"
#include "../util/ComUtils.h"
#include "../util/CommandResolver.h"
#include <algorithm>
#include <filesystem>
#include <shlobj.h>
#include <windows.h>


namespace fs = std::filesystem;

namespace uboot {

CollectorResult StartupFolderCollector::Collect() {
  CollectorResult result;

  // Initialize COM for LnkResolver using RAII
  util::ComInit com(COINIT_MULTITHREADED);

  // User Startup folder
  std::string userStartup = GetSpecialFolderPath("AppData");
  if (!userStartup.empty()) {
    userStartup += "\\Microsoft\\Windows\\Start Menu\\Programs\\Startup";
    CollectFromFolder("User", userStartup, result.entries, result.errors);
  }

  // Common Startup folder
  std::string commonStartup = GetSpecialFolderPath("ProgramData");
  if (!commonStartup.empty()) {
    commonStartup += "\\Microsoft\\Windows\\Start Menu\\Programs\\Startup";
    CollectFromFolder("Machine", commonStartup, result.entries, result.errors);
  }

  // Sort entries deterministically
  std::sort(result.entries.begin(), result.entries.end(),
            [](const Entry &a, const Entry &b) {
              if (a.scope != b.scope)
                return a.scope < b.scope;
              if (a.location != b.location)
                return a.location < b.location;
              return a.key < b.key;
            });

  return result;
}

void StartupFolderCollector::CollectFromFolder(
    const std::string &scope, const std::string &folderPath,
    std::vector<Entry> &outEntries, std::vector<CollectorError> &outErrors) {
  // Convert UTF-8 path to UTF-16
  int wideLen =
      MultiByteToWideChar(CP_UTF8, 0, folderPath.c_str(), -1, nullptr, 0);
  if (wideLen <= 0) {
    return; // Path too long or invalid
  }

  std::wstring widePath(wideLen - 1, 0);
  MultiByteToWideChar(CP_UTF8, 0, folderPath.c_str(), -1, &widePath[0],
                      wideLen);

  // Check if folder exists
  DWORD attrs = GetFileAttributesW(widePath.c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
    return; // Folder doesn't exist or not a directory, not an error
  }

  EnumerateLnkFiles(scope, folderPath, "", outEntries, outErrors);
}

void StartupFolderCollector::EnumerateLnkFiles(
    const std::string &scope, const std::string &folderPath,
    const std::string &relativePath, std::vector<Entry> &outEntries,
    std::vector<CollectorError> &outErrors) {
  // Convert paths
  int wideLen =
      MultiByteToWideChar(CP_UTF8, 0, folderPath.c_str(), -1, nullptr, 0);
  if (wideLen <= 0)
    return;

  std::wstring wideFolderPath(wideLen - 1, 0);
  MultiByteToWideChar(CP_UTF8, 0, folderPath.c_str(), -1, &wideFolderPath[0],
                      wideLen);

  WIN32_FIND_DATAW findData = {};
  HANDLE findHandle =
      FindFirstFileW((wideFolderPath + L"\\*").c_str(), &findData);

  if (findHandle == INVALID_HANDLE_VALUE) {
    return;
  }

  do {
    std::string fileName(WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1,
                                             nullptr, 0, nullptr, nullptr) -
                             1,
                         0);
    WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1, &fileName[0],
                        fileName.size() + 1, nullptr, nullptr);

    // Skip . and ..
    if (fileName == "." || fileName == "..") {
      continue;
    }

    std::string newRelativePath =
        relativePath.empty() ? fileName : relativePath + "\\" + fileName;
    std::string fullPath =
        folderPath + (relativePath.empty() ? "\\" : "\\") + newRelativePath;

    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // Recurse into subdirectory
      EnumerateLnkFiles(scope, folderPath, newRelativePath, outEntries,
                        outErrors);
    } else if (fileName.size() > 4 &&
               fileName.substr(fileName.size() - 4) == ".lnk") {
      // Found a .lnk file, resolve it
      std::string lnkPath =
          folderPath + (relativePath.empty() ? "" : "\\") + newRelativePath;

      LnkResolutionResult lnkResult = LnkResolver::Resolve(lnkPath);

      std::string targetPath = "";
      std::string arguments = "";

      if (lnkResult.isResolved) {
        targetPath = lnkResult.targetPath;
        arguments = lnkResult.arguments;
      }

      // Create Entry object
      Entry entry(GetName(),       // source = "StartupFolder"
                  scope,           // scope = "User" or "Machine"
                  fileName,        // key = filename without extension
                  newRelativePath, // location = relative path in startup folder
                  arguments,       // arguments from .lnk
                  targetPath       // imagePath from .lnk target
      );

      outEntries.push_back(entry);
    }
  } while (FindNextFileW(findHandle, &findData));

  FindClose(findHandle);
}

std::string
StartupFolderCollector::GetSpecialFolderPath(const std::string &folderName) {
  PIDLIST_ABSOLUTE pidl = nullptr;
  int folderId = CSIDL_APPDATA; // Default to AppData

  if (folderName == "ProgramData") {
    folderId = CSIDL_COMMON_APPDATA;
  } else if (folderName == "Desktop") {
    folderId = CSIDL_DESKTOPDIRECTORY;
  } else if (folderName == "Documents") {
    folderId = CSIDL_MYDOCUMENTS;
  }

  if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, folderId, &pidl))) {
    wchar_t path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(pidl, path)) {
      int pathLen = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0,
                                        nullptr, nullptr);
      std::string result(pathLen - 1, 0);
      WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], pathLen, nullptr,
                          nullptr);

      CoTaskMemFree(pidl);
      return result;
    }
    CoTaskMemFree(pidl);
  }

  return "";
}

} // namespace uboot
