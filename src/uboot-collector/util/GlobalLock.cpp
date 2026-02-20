#include "GlobalLock.h"
#include <filesystem>
#include <iostream>
#include <shlobj.h>


namespace fs = std::filesystem;

namespace uboot {
namespace util {

GlobalLock::GlobalLock() {}

GlobalLock::~GlobalLock() { Release(); }

void GlobalLock::Release() {
  if (hFile_ != INVALID_HANDLE_VALUE) {
    CloseHandle(hFile_);
    hFile_ = INVALID_HANDLE_VALUE;
    // We do NOT delete the file, as that releases the lock for others to race
    // on creation. The lock is the open handle. Actually, if we delete it, it's
    // fine, but keeping it is also fine. Let's delete it to keep ProgramData
    // clean if we can. But we need to close handle first.
    if (!lockFilePath_.empty()) {
      DeleteFileA(lockFilePath_.c_str());
    }
  }
}

std::string GlobalLock::GetLockFilePath() {
  char path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
    fs::path p(path);
    p /= "Uboot";
    // Ensure directory exists
    std::error_code ec;
    fs::create_directories(p, ec);
    p /= "global.lock";
    return p.string();
  }
  return "";
}

bool GlobalLock::Acquire() {
  lockFilePath_ = GetLockFilePath();
  if (lockFilePath_.empty())
    return false;

  // Try to create/open the file with exclusive access.
  // dwShareMode = 0 means we want exclusive access.
  // If another process has it open, this will fail.
  hFile_ =
      CreateFileA(lockFilePath_.c_str(), GENERIC_READ | GENERIC_WRITE,
                  0, // Exclusive access (no sharing)
                  NULL,
                  OPEN_ALWAYS, // Create if not exists, open if exists
                  FILE_ATTRIBUTE_NORMAL |
                      FILE_FLAG_DELETE_ON_CLOSE, // Delete on close is nice but
                                                 // tricky if we crash?
                  // Actually FILE_FLAG_DELETE_ON_CLOSE is good for cleanup.
                  NULL);

  if (hFile_ == INVALID_HANDLE_VALUE) {
    DWORD dwErr = GetLastError();
    if (dwErr == ERROR_SHARING_VIOLATION) {
      // File is locked by another process
      return false;
    }
    // Other error
    return false;
  }

  return true;
}

} // namespace util
} // namespace uboot
