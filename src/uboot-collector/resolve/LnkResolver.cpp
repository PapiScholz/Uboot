#include "LnkResolver.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

// We need to define IShellLinkW and IPersistFile if not available
// These are standard COM interfaces available in Windows 7+

namespace uboot {

// Helper: Convert UTF-8 to UTF-16
std::wstring LnkResolver::Utf8ToUtf16(const std::string& utf8) {
    if (utf8.empty()) return L"";
    
    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (requiredSize <= 0) return L"";
    
    std::wstring utf16(requiredSize - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &utf16[0], requiredSize);
    
    return utf16;
}

// Helper: Convert UTF-16 to UTF-8
std::string LnkResolver::Utf16ToUtf8(const std::wstring& utf16) {
    if (utf16.empty()) return "";
    
    int requiredSize = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (requiredSize <= 0) return "";
    
    std::string utf8(requiredSize - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], requiredSize, nullptr, nullptr);
    
    return utf8;
}

LnkResolutionResult LnkResolver::Resolve(const std::string& lnkFilePath) {
    LnkResolutionResult result;
    result.isResolved = false;
    
    if (lnkFilePath.empty()) {
        result.resolveNotes = "Empty file path provided";
        return result;
    }
    
    // Convert to UTF-16
    std::wstring lnkPathW = Utf8ToUtf16(lnkFilePath);
    
    // Initialize COM (thread-safe initialization)
    // Note: Caller is responsible for ensuring CoInitializeEx is called if not already done
    // We'll attempt to use COM without explicit initialization assuming it's already done
    
    // Create IShellLinkW instance
    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    
    if (FAILED(hr)) {
        result.resolveNotes = "Failed to create IShellLinkW instance (HRESULT: 0x" 
                             + std::to_string(static_cast<unsigned long>(hr)) + ")";
        return result;
    }
    
    // Get IPersistFile interface
    IPersistFile* ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
    
    if (FAILED(hr)) {
        result.resolveNotes = "Failed to get IPersistFile interface (HRESULT: 0x"
                             + std::to_string(static_cast<unsigned long>(hr)) + ")";
        psl->Release();
        return result;
    }
    
    // Load the shortcut file
    hr = ppf->Load(lnkPathW.c_str(), STGM_READ);
    
    if (FAILED(hr)) {
        result.resolveNotes = "Failed to load shortcut file (HRESULT: 0x"
                             + std::to_string(static_cast<unsigned long>(hr)) + ")";
        ppf->Release();
        psl->Release();
        return result;
    }
    
    // Resolve the shortcut links
    hr = psl->Resolve(nullptr, SLR_NO_UI | SLR_UPDATE);
    
    if (FAILED(hr)) {
        result.resolveNotes = "Failed to resolve shortcut link (HRESULT: 0x"
                             + std::to_string(static_cast<unsigned long>(hr)) + ")";
        // Continue anyway - we may still have valid target path
    }
    
    // Get the target path
    wchar_t targetPath[MAX_PATH];
    WIN32_FIND_DATAW wfd;
    hr = psl->GetPath(targetPath, MAX_PATH, &wfd, SLGP_RAWPATH);
    
    if (SUCCEEDED(hr)) {
        result.targetPath = Utf16ToUtf8(targetPath);
        result.isResolved = true;
    } else {
        result.resolveNotes = "Failed to get target path (HRESULT: 0x"
                             + std::to_string(static_cast<unsigned long>(hr)) + ")";
    }
    
    // Get the arguments
    wchar_t arguments[MAX_PATH];
    hr = psl->GetArguments(arguments, MAX_PATH);
    
    if (SUCCEEDED(hr) && wcslen(arguments) > 0) {
        result.arguments = Utf16ToUtf8(arguments);
    }
    
    // Get the working directory
    wchar_t workingDir[MAX_PATH];
    hr = psl->GetWorkingDirectory(workingDir, MAX_PATH);
    
    if (SUCCEEDED(hr) && wcslen(workingDir) > 0) {
        result.workingDirectory = Utf16ToUtf8(workingDir);
    }
    
    // Add additional resolution notes if we have partial data
    if (result.isResolved) {
        result.resolveNotes = "Successfully resolved shortcut file";
        if (!result.arguments.empty()) {
            result.resolveNotes += " (with arguments)";
        }
        if (!result.workingDirectory.empty()) {
            result.resolveNotes += " (with working directory)";
        }
    }
    
    // Clean up COM objects
    ppf->Release();
    psl->Release();
    
    return result;
}

} // namespace uboot
