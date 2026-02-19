#include "file_probe.h"
#include <fileapi.h>
#include <iomanip>
#include <memory>
#include <sstream>


namespace uboot {
namespace evidence {

namespace {
// RAII wrapper for file handle
struct FileHandleDeleter {
  void operator()(HANDLE h) const {
    if (h != INVALID_HANDLE_VALUE)
      CloseHandle(h);
  }
};
using FileHandlePtr =
    std::unique_ptr<std::remove_pointer_t<HANDLE>, FileHandleDeleter>;
} // namespace

FileMetadata FileProbe::Probe(const std::wstring &path) noexcept {
  FileMetadata meta;

  // Open file to get handle
  // FILE_FLAG_BACKUP_SEMANTICS is needed to open directories if we encounter
  // them
  FileHandlePtr hFile(CreateFileW(
      path.c_str(),
      0, // No access rights needed for metadata query if we have appropriate
         // permissions, but generic read might be safer for some cases.
         // ACTUALLY: GetFileInformationByHandleEx requires generic read or
         // specific attribute read rights? MSDN: "To retrieve file information,
         // you must have the FILE_READ_ATTRIBUTES access right."
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));

  // If opening with 0 access fails, try GENERIC_READ?
  // Let's try to open with 0 first (metadata only), if it fails, maybe
  // permission issue. Wait, CreateFileW with 0 access is valid for querying
  // attributes.

  if (hFile.get() == INVALID_HANDLE_VALUE) {
    meta.exists = false;
    meta.win32Error = GetLastError();
    return meta;
  }

  meta.exists = true;

  // Get Basic Info (Attributes, Times)
  FILE_BASIC_INFO basicInfo = {};
  if (!GetFileInformationByHandleEx(hFile.get(), FileBasicInfo, &basicInfo,
                                    sizeof(basicInfo))) {
    meta.win32Error = GetLastError();
    return meta;
  }

  meta.attributes = basicInfo.FileAttributes;
  meta.creationTimeIso = FileTimeToIso8601(basicInfo.CreationTime);
  meta.lastWriteTimeIso = FileTimeToIso8601(basicInfo.LastWriteTime);

  // Get Standard Info (Size, Directory, Links)
  FILE_STANDARD_INFO stdInfo = {};
  if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &stdInfo,
                                    sizeof(stdInfo))) {
    meta.win32Error = GetLastError();
    return meta;
  }

  meta.fileSize = static_cast<uint64_t>(stdInfo.EndOfFile.QuadPart);
  meta.isDirectory = stdInfo.Directory;

  return meta;
}

bool FileProbe::Exists(const std::wstring &path) noexcept {
  DWORD attrs = GetFileAttributesW(path.c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES);
}

std::optional<uint64_t>
FileProbe::GetFileSize(const std::wstring &path) noexcept {
  FileMetadata meta = Probe(path);
  if (meta.exists && !meta.isDirectory) {
    return meta.fileSize;
  }
  return std::nullopt;
}

std::string FileProbe::FileTimeToIso8601(const LARGE_INTEGER &li) noexcept {
  if (li.QuadPart == 0) {
    return "";
  }

  FILETIME ft;
  ft.dwLowDateTime = li.LowPart;
  ft.dwHighDateTime = li.HighPart;

  SYSTEMTIME st;
  if (!FileTimeToSystemTime(&ft, &st)) {
    return "";
  }

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << st.wYear << "-" << std::setw(2)
      << st.wMonth << "-" << std::setw(2) << st.wDay << "T" << std::setw(2)
      << st.wHour << ":" << std::setw(2) << st.wMinute << ":" << std::setw(2)
      << st.wSecond << "Z";

  return oss.str();
}

} // namespace evidence
} // namespace uboot
