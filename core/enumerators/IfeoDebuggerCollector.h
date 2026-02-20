#pragma once

#include "ICollector.h"
#include <windows.h>

namespace uboot {

/// <summary>
/// Collects entries from Image File Execution Options (IFEO) Debugger values.
/// Enumerates:
/// - HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\Image File
/// Execution Options
/// - Looks for "Debugger" value in each executable subkey
/// </summary>
class IfeoDebuggerCollector : public ICollector {
public:
  std::string GetName() const override { return "IfeoDebugger"; }

  CollectorResult Collect() override;

private:
  /// <summary>
  /// Enumerate IFEO subkeys and collect Debugger values.
  /// </summary>
  void EnumerateIfeoSubkeys(HKEY parentHandle, const std::string &parentPath,
                            std::vector<Entry> &outEntries,
                            std::vector<CollectorError> &outErrors);
};

} // namespace uboot
