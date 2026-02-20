#include "../core/ops/RegistryRenameValueOp.h"
#include "../core/ops/TaskToggleEnabledOp.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>


using namespace uboot::ops;

// Helper to run command
int RunCmd(const std::string &cmd) { return std::system(cmd.c_str()); }

// Helper to check registry value existence
bool RegValueExists(HKEY hRoot, const std::string &keyPath,
                    const std::string &valueName) {
  HKEY hKey;
  if (RegOpenKeyExA(hRoot, keyPath.c_str(), 0, KEY_READ, &hKey) !=
      ERROR_SUCCESS)
    return false;
  DWORD type;
  LSTATUS status =
      RegQueryValueExA(hKey, valueName.c_str(), NULL, &type, NULL, NULL);
  RegCloseKey(hKey);
  return status == ERROR_SUCCESS;
}

void TestTaskToggle() {
  std::cout << "[Test: TaskToggle] Running..." << std::endl;
  // 1. Create a dummy task
  std::string taskName = "UbootTestTask";
  RunCmd("schtasks /Create /TN " + taskName +
         " /TR \"calc.exe\" /SC ONCE /ST 00:00 /F > NUL 2>&1");

  // Ensure it exists and is enabled by default
  // We can't easily check enabled status via system() without parsing,
  // but we trust the Op's COM logic to read it.

  // 2. Prepare Op (Disable)
  TaskToggleEnabledOp op(taskName, TaskToggleEnabledOp::Action::Disable);
  if (!op.prepare(".")) {
    std::cout << "  [FAIL] Prepare failed" << std::endl;
    RunCmd("schtasks /Delete /TN " + taskName + " /F > NUL 2>&1");
    return;
  }

  // 3. Apply
  if (!op.apply()) {
    std::cout << "  [FAIL] Apply failed" << std::endl;
  } else {
    std::cout << "  [PASS] Apply success" << std::endl;
  }

  // 4. Verify
  if (!op.verify()) { // Should be disabled
    std::cout << "  [FAIL] Verify failed (should be disabled)" << std::endl;
  } else {
    std::cout << "  [PASS] Verify success" << std::endl;
  }

  // 5. Rollback
  if (!op.rollback(".")) {
    std::cout << "  [FAIL] Rollback failed" << std::endl;
  } else {
    std::cout << "  [PASS] Rollback success" << std::endl;
  }

  // Cleanup
  RunCmd("schtasks /Delete /TN " + taskName + " /F > NUL 2>&1");
}

void TestRegistryRunMove() {
  std::cout << "[Test: RegistryRunMove] Running..." << std::endl;

  // 1. Create dummy value in HKCU Run
  std::string keyPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  std::string valName = "UbootTestVal";
  std::string valData = "calc.exe";

  HKEY hKey;
  if (RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_WRITE, &hKey) !=
      ERROR_SUCCESS) {
    std::cout << "  [SKIP] Cannot open HKCU Run key for write" << std::endl;
    return;
  }
  RegSetValueExA(hKey, valName.c_str(), 0, REG_SZ,
                 (const BYTE *)valData.c_str(), valData.length() + 1);
  RegCloseKey(hKey);

  // 2. Prepare Op (Disable)
  // Note: RegistryRenameValueOp expects full path starting with HKLM/HKCU?
  // The implementation checks:
  // if (key.find("HKCU") == 0 || key.find("HKEY_CURRENT_USER") == 0)
  std::string fullKeyPath = "HKCU\\" + keyPath;

  RegistryRenameValueOp op(fullKeyPath, valName,
                           RegistryRenameValueOp::Action::Disable);
  if (!op.prepare(".")) {
    std::cout << "  [FAIL] Prepare failed" << std::endl;
    // Cleanup
    HKEY hK;
    RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_SET_VALUE, &hK);
    RegDeleteValueA(hK, valName.c_str());
    RegCloseKey(hK);
    return;
  }

  // 3. Apply
  if (!op.apply()) {
    std::cout << "  [FAIL] Apply failed" << std::endl;
  } else {
    // Check if moved
    if (!RegValueExists(HKEY_CURRENT_USER, keyPath + "\\UbootDisabled",
                        valName)) {
      std::cout << "  [FAIL] Value not found in UbootDisabled" << std::endl;
    } else if (RegValueExists(HKEY_CURRENT_USER, keyPath, valName)) {
      std::cout << "  [FAIL] Value still exists in original key" << std::endl;
    } else {
      std::cout << "  [PASS] Move success" << std::endl;
    }
  }

  // 4. Rollback
  if (!op.rollback(".")) {
    std::cout << "  [FAIL] Rollback failed" << std::endl;
  } else {
    // Check if restored
    if (RegValueExists(HKEY_CURRENT_USER, keyPath + "\\UbootDisabled",
                       valName)) {
      std::cout << "  [FAIL] Value still in UbootDisabled after rollback"
                << std::endl;
    } else if (!RegValueExists(HKEY_CURRENT_USER, keyPath, valName)) {
      std::cout << "  [FAIL] Value not restored to original key" << std::endl;
    } else {
      std::cout << "  [PASS] Rollback success" << std::endl;
    }
  }

  // Cleanup
  RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_SET_VALUE, &hKey);
  RegDeleteValueA(hKey, valName.c_str());
  RegCloseKey(hKey);

  // Remove subkey if empty? The op doesn't remove the key itself.
  // We leave UbootDisabled key, it's fine.
}

int main() {
  std::cout << "Running Ops Tests..." << std::endl;
  TestTaskToggle();
  TestRegistryRunMove();
  std::cout << "Done." << std::endl;
  return 0;
}
