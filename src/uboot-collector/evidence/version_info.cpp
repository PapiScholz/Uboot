#include "version_info.h"
#include <vector>
#include <memory>

#pragma comment(lib, "version.lib")

namespace uboot {
namespace evidence {

VersionInfo VersionInfoExtractor::Extract(const std::wstring& filePath) noexcept {
    VersionInfo info;
    
    // Get version info size
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(filePath.c_str(), &handle);
    
    if (size == 0) {
        info.win32Error = GetLastError();
        return info;
    }
    
    // Allocate buffer and get version info
    std::vector<uint8_t> buffer(size);
    if (!GetFileVersionInfoW(filePath.c_str(), 0, size, buffer.data())) {
        info.win32Error = GetLastError();
        return info;
    }
    
    // Get language and codepage
    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate = nullptr;
    
    UINT translateSize = 0;
    if (!VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation", 
                        reinterpret_cast<LPVOID*>(&lpTranslate), &translateSize)) {
        // No translation info, try default
        lpTranslate = nullptr;
    }
    
    // Build subblock prefix
    std::wstring subBlockPrefix;
    if (lpTranslate && translateSize >= sizeof(LANGANDCODEPAGE)) {
        wchar_t prefix[64];
        swprintf_s(prefix, L"\\StringFileInfo\\%04x%04x\\",
                   lpTranslate[0].wLanguage,
                   lpTranslate[0].wCodePage);
        subBlockPrefix = prefix;
    } else {
        // Try common English/US default
        subBlockPrefix = L"\\StringFileInfo\\040904b0\\";
    }
    
    // Extract string values
    info.companyName = QueryStringValue(buffer.data(), subBlockPrefix + L"CompanyName");
    info.productName = QueryStringValue(buffer.data(), subBlockPrefix + L"ProductName");
    info.productVersion = QueryStringValue(buffer.data(), subBlockPrefix + L"ProductVersion");
    info.fileVersion = QueryStringValue(buffer.data(), subBlockPrefix + L"FileVersion");
    info.fileDescription = QueryStringValue(buffer.data(), subBlockPrefix + L"FileDescription");
    info.originalFilename = QueryStringValue(buffer.data(), subBlockPrefix + L"OriginalFilename");
    info.legalCopyright = QueryStringValue(buffer.data(), subBlockPrefix + L"LegalCopyright");
    info.internalName = QueryStringValue(buffer.data(), subBlockPrefix + L"InternalName");
    
    // Get fixed file info (binary version)
    VS_FIXEDFILEINFO* pFileInfo = nullptr;
    UINT fileInfoSize = 0;
    if (VerQueryValueW(buffer.data(), L"\\", 
                       reinterpret_cast<LPVOID*>(&pFileInfo), &fileInfoSize)) {
        if (pFileInfo && fileInfoSize >= sizeof(VS_FIXEDFILEINFO)) {
            // Combine dwFileVersionMS and dwFileVersionLS into 64-bit value
            uint64_t fileVer = (static_cast<uint64_t>(pFileInfo->dwFileVersionMS) << 32) |
                               static_cast<uint64_t>(pFileInfo->dwFileVersionLS);
            info.fileVersionBinary = fileVer;
            
            uint64_t prodVer = (static_cast<uint64_t>(pFileInfo->dwProductVersionMS) << 32) |
                               static_cast<uint64_t>(pFileInfo->dwProductVersionLS);
            info.productVersionBinary = prodVer;
        }
    }
    
    return info;
}

std::optional<std::wstring> VersionInfoExtractor::QueryStringValue(
    const void* versionData,
    const std::wstring& subBlock
) noexcept {
    wchar_t* pValue = nullptr;
    UINT valueSize = 0;
    
    if (!VerQueryValueW(const_cast<void*>(versionData), subBlock.c_str(),
                        reinterpret_cast<LPVOID*>(&pValue), &valueSize)) {
        return std::nullopt;
    }
    
    if (!pValue || valueSize == 0) {
        return std::nullopt;
    }
    
    // Return string (VerQueryValue returns null-terminated string)
    return std::wstring(pValue);
}

} // namespace evidence
} // namespace uboot
