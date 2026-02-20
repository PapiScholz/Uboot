#include "SecurityLog.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>


#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace uboot::hardening {

SecurityLog &SecurityLog::Instance() {
  static SecurityLog instance;
  return instance;
}

SecurityLog::SecurityLog() {
#ifdef _WIN32
  char path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
    logPath = (fs::path(path) / "Uboot" / "SecurityLog.log").string();
  } else {
    logPath = "SecurityLog.log";
  }
#else
  logPath = "SecurityLog.log";
#endif
  // Ensure dir exists
  fs::path p(logPath);
  if (p.has_parent_path()) {
    fs::create_directories(p.parent_path());
  }
}

std::string SecurityLog::GetTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

void SecurityLog::Log(const std::string &level, const std::string &event,
                      const std::string &details) {
  std::lock_guard<std::mutex> lock(logMutex);
  std::ofstream outfile(logPath, std::ios_base::app);
  if (outfile.is_open()) {
    outfile << "[" << GetTimestamp() << "] "
            << "[" << level << "] " << event;
    if (!details.empty()) {
      outfile << " - " << details;
    }
    outfile << "\n";
  }
}

} // namespace uboot::hardening
