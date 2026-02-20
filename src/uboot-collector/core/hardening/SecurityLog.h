#pragma once

#include <mutex>
#include <string>


namespace uboot::hardening {

class SecurityLog {
public:
  static SecurityLog &Instance();

  void Log(const std::string &level, const std::string &event,
           const std::string &details = "");

private:
  SecurityLog();
  ~SecurityLog() = default;

  std::string logPath;
  std::mutex logMutex;

  std::string GetTimestamp();
};

} // namespace uboot::hardening
