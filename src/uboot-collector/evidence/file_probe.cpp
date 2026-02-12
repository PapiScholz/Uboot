#include "file_probe.h"
#include <fileapi.h>

namespace uboot {
namespace evidence {

FileMetadata FileProbe::Probe(const std::wstring& path) noexcept {
    FileMetadata meta;
    
    // Use GetFileAttributesExW for efficiency (single syscall)
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
        meta.exists = false;
        meta.win32Error = GetLastError();
        return meta;
    }
    
    meta.exists = true;
    meta.attributes = fad.dwFileAttributes;
    meta.creationTime = fad.ftCreationTime;
    meta.lastWriteTime = fad.ftLastWriteTime;
    
    // Combine high/low parts for file size
    ULARGE_INTEGER uli;
    uli.LowPart = fad.nFileSizeLow;
    uli.HighPart = fad.nFileSizeHigh;
    meta.fileSize = uli.QuadPart;
    
    return meta;
}

bool FileProbe::Exists(const std::wstring& path) noexcept {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

std::optional<uint64_t> FileProbe::GetFileSize(const std::wstring& path) noexcept {
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
        return std::nullopt;
    }
    
    ULARGE_INTEGER uli;
    uli.LowPart = fad.nFileSizeLow;
    uli.HighPart = fad.nFileSizeHigh;
    return uli.QuadPart;
}

} // namespace evidence
} // namespace uboot
