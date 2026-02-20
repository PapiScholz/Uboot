#pragma once

#include <string>
#include <windows.h>

namespace uboot {
namespace util {

class GlobalLock {
public:
  GlobalLock();
  ~GlobalLock();

  // Tries to acquire the global lock.
  // Returns true if successful, false if another instance is running.
  bool Acquire();

  // Releases the lock (called automatically in destructor).
  void Release();

private:
  HANDLE hFile_ = INVALID_HANDLE_VALUE;
  std::string lockFilePath_;

  std::string GetLockFilePath();
};

} // namespace util
} // namespace uboot
