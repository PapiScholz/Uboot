#include "misconfig_checks.h"
#include <Windows.h>
#include <aclapi.h>
#include <algorithm>
#include <cctype>
#include <sddl.h>


#pragma comment(lib, "advapi32.lib")

namespace uboot {
namespace evidence {

MisconfigCheckResult
MisconfigChecker::Check(const std::wstring &commandOrPath,
                        const std::wstring &imagePath) noexcept {
  MisconfigCheckResult result;

  // Empty command check
  if (commandOrPath.empty() ||
      std::all_of(commandOrPath.begin(), commandOrPath.end(), ::iswspace)) {
    result.findings.push_back({EvidenceIssueType::EmptyCommand,
                               L"Command or path is empty", commandOrPath});
    return result;
  }

  // Run all checks
  CheckPathQuoting(commandOrPath, result.findings);
  CheckRelativePath(imagePath, result.findings);
  CheckExtension(imagePath, result.findings);
  CheckNetworkPath(imagePath, result.findings);
  CheckEnvironmentVars(commandOrPath, result.findings);
  CheckFileWritePerms(imagePath, result.findings);

  // Check if file exists
  if (!imagePath.empty()) {
    DWORD attrs = GetFileAttributesW(imagePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
      result.findings.push_back({EvidenceIssueType::ResultTargetMissing,
                                 L"Target file not found", imagePath});
    }
  }

  return result;
}

void MisconfigChecker::CheckPathQuoting(
    const std::wstring &commandOrPath,
    std::vector<EvidenceIssue> &findings) noexcept {
  // Check for unquoted paths with spaces
  if (!IsQuoted(commandOrPath) && HasSpaces(commandOrPath)) {
    // Find first space position to extract likely path
    size_t spacePos = commandOrPath.find(L' ');
    if (spacePos != std::wstring::npos) {
      std::wstring likelyPath = commandOrPath.substr(0, spacePos);

      // Check if this looks like a path (contains \ or :)
      if (likelyPath.find(L'\\') != std::wstring::npos ||
          likelyPath.find(L':') != std::wstring::npos) {
        findings.push_back({EvidenceIssueType::UnquotedPathWithSpaces,
                            L"Unquoted path with spaces", commandOrPath});
      }
    }
  }
}

void MisconfigChecker::CheckRelativePath(
    const std::wstring &imagePath,
    std::vector<EvidenceIssue> &findings) noexcept {
  if (imagePath.empty())
    return;

  if (!IsAbsolutePath(imagePath)) {
    findings.push_back({EvidenceIssueType::RelativePath,
                        L"Relative path detected", imagePath});
  }
}

void MisconfigChecker::CheckExtension(
    const std::wstring &imagePath,
    std::vector<EvidenceIssue> &findings) noexcept {
  if (imagePath.empty())
    return;

  std::wstring ext = GetFileExtension(imagePath);
  if (ext.empty())
    return; // No extension is not necessarily "ExtensionNotExecutable" unless
            // we want to be strict.
  // For now assuming we only flag if extension IS present and NOT executable.

  // Convert to lowercase for comparison
  std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

  // List of known executable extensions
  const std::vector<std::wstring> executableExts = {
      L".exe", L".dll", L".sys", L".drv", L".ocx", L".cpl", L".scr",
      L".com", L".bat", L".cmd", L".vbs", L".vbe", L".js",  L".jse",
      L".wsf", L".wsh", L".ps1", L".msi", L".msp"};

  bool isExec = false;
  for (const auto &execExt : executableExts) {
    if (ext == execExt) {
      isExec = true;
      break;
    }
  }

  if (!isExec) {
    findings.push_back({EvidenceIssueType::ExtensionNotExecutable,
                        L"Extension is not in standard executable list",
                        imagePath});
  }
}

void MisconfigChecker::CheckNetworkPath(
    const std::wstring &imagePath,
    std::vector<EvidenceIssue> &findings) noexcept {
  if (IsNetworkPath(imagePath)) {
    findings.push_back(
        {EvidenceIssueType::NetworkPath, L"Network path detected", imagePath});
  }
}

void MisconfigChecker::CheckEnvironmentVars(
    const std::wstring &commandOrPath,
    std::vector<EvidenceIssue> &findings) noexcept {
  // Check for unexpanded environment variables (%VAR%)
  if (commandOrPath.find(L'%') != std::wstring::npos) {
    size_t firstPercent = commandOrPath.find(L'%');
    size_t secondPercent = commandOrPath.find(L'%', firstPercent + 1);

    if (secondPercent != std::wstring::npos) {
      findings.push_back({EvidenceIssueType::EnvironmentVarUnresolved,
                          L"Unresolved environment variable", commandOrPath});
    }
  }
}

void MisconfigChecker::CheckFileWritePerms(
    const std::wstring &imagePath,
    std::vector<EvidenceIssue> &findings) noexcept {
  if (imagePath.empty())
    return;

  if (IsWriteableByUsers(imagePath)) {
    findings.push_back({EvidenceIssueType::WritableByNonAdmin,
                        L"File writable by non-admin users", imagePath});
  }
}

bool MisconfigChecker::IsWriteableByUsers(
    const std::wstring &filePath) noexcept {
  // Get file security descriptor
  PSECURITY_DESCRIPTOR pSD = nullptr;
  PACL pDacl = nullptr;

  DWORD result = GetNamedSecurityInfoW(filePath.c_str(), SE_FILE_OBJECT,
                                       DACL_SECURITY_INFORMATION, nullptr,
                                       nullptr, &pDacl, nullptr, &pSD);

  if (result != ERROR_SUCCESS || !pSD) {
    return false;
  }

  bool isWriteable = false;

  // Check if BUILTIN\Users or Authenticated Users have write access
  // SID for BUILTIN\Users: S-1-5-32-545
  // SID for Authenticated Users: S-1-5-11
  PSID pUsersSid = nullptr;
  PSID pAuthUsersSid = nullptr;

  if (ConvertStringSidToSidW(L"S-1-5-32-545", &pUsersSid) &&
      ConvertStringSidToSidW(L"S-1-5-11", &pAuthUsersSid)) {

    // Check Users
    {
      TRUSTEE_W trustee = {};
      trustee.TrusteeForm = TRUSTEE_IS_SID;
      trustee.TrusteeType = TRUSTEE_IS_GROUP;
      trustee.ptstrName = reinterpret_cast<LPWSTR>(pUsersSid);

      ACCESS_MASK access = 0;
      if (GetEffectiveRightsFromAclW(pDacl, &trustee, &access) ==
          ERROR_SUCCESS) {
        if (access &
            (FILE_WRITE_DATA | FILE_APPEND_DATA | WRITE_DAC | WRITE_OWNER)) {
          isWriteable = true;
        }
      }
    }

    // Check Authenticated Users if not already found writable
    if (!isWriteable) {
      TRUSTEE_W trustee = {};
      trustee.TrusteeForm = TRUSTEE_IS_SID;
      trustee.TrusteeType = TRUSTEE_IS_GROUP;
      trustee.ptstrName = reinterpret_cast<LPWSTR>(pAuthUsersSid);

      ACCESS_MASK access = 0;
      if (GetEffectiveRightsFromAclW(pDacl, &trustee, &access) ==
          ERROR_SUCCESS) {
        if (access &
            (FILE_WRITE_DATA | FILE_APPEND_DATA | WRITE_DAC | WRITE_OWNER)) {
          isWriteable = true;
        }
      }
    }
  }

  if (pUsersSid)
    LocalFree(pUsersSid);
  if (pAuthUsersSid)
    LocalFree(pAuthUsersSid);

  LocalFree(pSD);

  return isWriteable;
}

bool MisconfigChecker::HasSpaces(const std::wstring &str) noexcept {
  return str.find(L' ') != std::wstring::npos;
}

bool MisconfigChecker::IsQuoted(const std::wstring &str) noexcept {
  if (str.length() < 2)
    return false;
  return (str.front() == L'"' && str.back() == L'"');
}

bool MisconfigChecker::IsAbsolutePath(const std::wstring &path) noexcept {
  if (path.length() < 3)
    return false;

  // Check for drive letter (C:\)
  if (std::iswalpha(path[0]) && path[1] == L':' && path[2] == L'\\') {
    return true;
  }

  // Check for UNC path (\\server\share)
  if (path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\') {
    return true;
  }

  return false;
}

bool MisconfigChecker::IsNetworkPath(const std::wstring &path) noexcept {
  return (path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\');
}

std::wstring
MisconfigChecker::GetFileExtension(const std::wstring &path) noexcept {
  size_t dotPos = path.find_last_of(L'.');
  size_t slashPos = path.find_last_of(L"\\/");

  // Make sure dot is after last slash (not in directory name)
  if (dotPos != std::wstring::npos &&
      (slashPos == std::wstring::npos || dotPos > slashPos)) {
    return path.substr(dotPos);
  }

  return L"";
}

} // namespace evidence
} // namespace uboot
