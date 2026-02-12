#include "misconfig_checks.h"
#include <Windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "advapi32.lib")

namespace uboot {
namespace evidence {

bool MisconfigCheckResult::HasCriticalFindings() const {
    for (const auto& finding : findings) {
        if (finding.type == MisconfigType::PathContainsSpacesNoQuotes ||
            finding.type == MisconfigType::WriteableByUsers ||
            finding.type == MisconfigType::FileNotFound) {
            return true;
        }
    }
    return false;
}

MisconfigCheckResult MisconfigChecker::Check(
    const std::wstring& commandOrPath,
    const std::wstring& imagePath
) noexcept {
    MisconfigCheckResult result;
    
    // Empty command check
    if (commandOrPath.empty() || 
        std::all_of(commandOrPath.begin(), commandOrPath.end(), ::iswspace)) {
        result.findings.push_back({
            MisconfigType::EmptyCommand,
            L"Command or path is empty or whitespace-only",
            commandOrPath
        });
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
            result.findings.push_back({
                MisconfigType::FileNotFound,
                L"Referenced file does not exist",
                imagePath
            });
        }
    }
    
    return result;
}

void MisconfigChecker::CheckPathQuoting(
    const std::wstring& commandOrPath,
    std::vector<MisconfigFinding>& findings
) noexcept {
    // Check for unquoted paths with spaces
    if (!IsQuoted(commandOrPath) && HasSpaces(commandOrPath)) {
        // Find first space position to extract likely path
        size_t spacePos = commandOrPath.find(L' ');
        if (spacePos != std::wstring::npos) {
            std::wstring likelyPath = commandOrPath.substr(0, spacePos);
            
            // Check if this looks like a path (contains \ or :)
            if (likelyPath.find(L'\\') != std::wstring::npos || 
                likelyPath.find(L':') != std::wstring::npos) {
                findings.push_back({
                    MisconfigType::PathContainsSpacesNoQuotes,
                    L"Path contains spaces but is not quoted (command injection risk)",
                    commandOrPath
                });
            }
        }
    }
}

void MisconfigChecker::CheckRelativePath(
    const std::wstring& imagePath,
    std::vector<MisconfigFinding>& findings
) noexcept {
    if (imagePath.empty()) return;
    
    if (!IsAbsolutePath(imagePath)) {
        findings.push_back({
            MisconfigType::RelativePath,
            L"Relative path detected (DLL hijacking or search order risk)",
            imagePath
        });
    }
}

void MisconfigChecker::CheckExtension(
    const std::wstring& imagePath,
    std::vector<MisconfigFinding>& findings
) noexcept {
    if (imagePath.empty()) return;
    
    std::wstring ext = GetFileExtension(imagePath);
    if (ext.empty()) return;
    
    // List of suspicious extensions (non-executable)
    const std::vector<std::wstring> suspiciousExts = {
        L".txt", L".dat", L".log", L".tmp", L".ini", L".cfg", L".conf"
    };
    
    for (const auto& suspExt : suspiciousExts) {
        if (_wcsicmp(ext.c_str(), suspExt.c_str()) == 0) {
            findings.push_back({
                MisconfigType::SuspiciousExtension,
                L"File has non-executable extension",
                imagePath
            });
            break;
        }
    }
}

void MisconfigChecker::CheckNetworkPath(
    const std::wstring& imagePath,
    std::vector<MisconfigFinding>& findings
) noexcept {
    if (IsNetworkPath(imagePath)) {
        findings.push_back({
            MisconfigType::NetworkPath,
            L"UNC network path detected",
            imagePath
        });
    }
}

void MisconfigChecker::CheckEnvironmentVars(
    const std::wstring& commandOrPath,
    std::vector<MisconfigFinding>& findings
) noexcept {
    // Check for unexpanded environment variables (%VAR%)
    if (commandOrPath.find(L'%') != std::wstring::npos) {
        size_t firstPercent = commandOrPath.find(L'%');
        size_t secondPercent = commandOrPath.find(L'%', firstPercent + 1);
        
        if (secondPercent != std::wstring::npos) {
            findings.push_back({
                MisconfigType::EnvironmentVarUnresolved,
                L"Unresolved environment variable detected",
                commandOrPath
            });
        }
    }
}

void MisconfigChecker::CheckFileWritePerms(
    const std::wstring& imagePath,
    std::vector<MisconfigFinding>& findings
) noexcept {
    if (imagePath.empty()) return;
    
    if (IsWriteableByUsers(imagePath)) {
        findings.push_back({
            MisconfigType::WriteableByUsers,
            L"File is writeable by non-administrator users",
            imagePath
        });
    }
}

bool MisconfigChecker::IsWriteableByUsers(const std::wstring& filePath) noexcept {
    // Get file security descriptor
    PSECURITY_DESCRIPTOR pSD = nullptr;
    PACL pDacl = nullptr;
    
    DWORD result = GetNamedSecurityInfoW(
        filePath.c_str(),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        &pDacl,
        nullptr,
        &pSD
    );
    
    if (result != ERROR_SUCCESS || !pSD) {
        return false;
    }
    
    bool isWriteable = false;
    
    // Check if BUILTIN\Users or Authenticated Users have write access
    // SID for BUILTIN\Users: S-1-5-32-545
    // SID for Authenticated Users: S-1-5-11
    PSID pUsersSid = nullptr;
    PSID pAuthUsersSid = nullptr;
    
    if (ConvertStringSidToSidW(L"S-1-5-32-545", &pUsersSid) ||
        ConvertStringSidToSidW(L"S-1-5-11", &pAuthUsersSid)) {
        
        TRUSTEE_W trustee = {};
        trustee.TrusteeForm = TRUSTEE_IS_SID;
        trustee.TrusteeType = TRUSTEE_IS_GROUP;
        
        if (pUsersSid) {
            trustee.ptstrName = reinterpret_cast<LPWSTR>(pUsersSid);
            
            ACCESS_MASK access = 0;
            if (GetEffectiveRightsFromAclW(pDacl, &trustee, &access) == ERROR_SUCCESS) {
                if (access & (FILE_WRITE_DATA | FILE_APPEND_DATA | WRITE_DAC | WRITE_OWNER)) {
                    isWriteable = true;
                }
            }
        }
        
        if (!isWriteable && pAuthUsersSid) {
            trustee.ptstrName = reinterpret_cast<LPWSTR>(pAuthUsersSid);
            
            ACCESS_MASK access = 0;
            if (GetEffectiveRightsFromAclW(pDacl, &trustee, &access) == ERROR_SUCCESS) {
                if (access & (FILE_WRITE_DATA | FILE_APPEND_DATA | WRITE_DAC | WRITE_OWNER)) {
                    isWriteable = true;
                }
            }
        }
        
        if (pUsersSid) LocalFree(pUsersSid);
        if (pAuthUsersSid) LocalFree(pAuthUsersSid);
    }
    
    LocalFree(pSD);
    
    return isWriteable;
}

bool MisconfigChecker::HasSpaces(const std::wstring& str) noexcept {
    return str.find(L' ') != std::wstring::npos;
}

bool MisconfigChecker::IsQuoted(const std::wstring& str) noexcept {
    if (str.length() < 2) return false;
    return (str.front() == L'"' && str.back() == L'"');
}

bool MisconfigChecker::IsAbsolutePath(const std::wstring& path) noexcept {
    if (path.length() < 3) return false;
    
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

bool MisconfigChecker::IsNetworkPath(const std::wstring& path) noexcept {
    return (path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\');
}

std::wstring MisconfigChecker::GetFileExtension(const std::wstring& path) noexcept {
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
