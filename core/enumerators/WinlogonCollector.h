#pragma once

#include "../model/CollectorError.h"
#include "../model/ENTRY.h"
#include <vector>
#include <windows.h>


#include "ICollector.h"

namespace uboot {

/// <summary>
/// Collects entries from Winlogon registry keys.
/// Monitors:
/// - HKEY_CURRENT_USER\Software\Microsoft\Windows NT\CurrentVersion\Winlogon
/// (Shell, Userinit)
/// - HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\Winlogon
/// (Shell, Userinit)
/// </summary>
class WinlogonCollector : public ICollector {
public:
  std::string GetName() const override { return "Winlogon"; }

  CollectorResult Collect() override;

private:
  /// <summary>
  /// Collect Winlogon entries from a registry hive.
  /// </summary>
  void CollectFromWinlogonKey(const std::string &scope, HKEY hive,
                              std::vector<Entry> &outEntries,
                              std::vector<CollectorError> &outErrors);
};

} // namespace uboot
