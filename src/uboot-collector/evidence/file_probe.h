#pragma once

#include <Windows.h>
#include <cstdint>
#include <optional>
#include <string>


namespace uboot {
namespace evidence {

/// <summary>
/// Basic file metadata obtained through Win32 API.
/// Deterministic and offline-first.
/// </summary>
struct FileMetadata {
  bool exists = false;
  uint64_t fileSize = 0;
  std::string creationTimeIso;  // UTC ISO 8601 (e.g. 2023-01-01T12:00:00Z)
  std::string lastWriteTimeIso; // UTC ISO 8601
  DWORD attributes = 0;
  bool isDirectory = false;

  // Error state
  std::optional<DWORD> win32Error;
};

/// <summary>
/// Probes a file for basic metadata without executing or heuristics.
/// </summary>
class FileProbe {
public:
  /// <summary>
  /// Probe a file at the given path.
  /// Uses CreateFileW + GetFileInformationByHandleEx
  /// (FileBasicInfo/FileStandardInfo).
  /// </summary>
  /// <param name="path">Full path to the file</param>
  /// <returns>FileMetadata with all fields populated</returns>
  static FileMetadata Probe(const std::wstring &path) noexcept;

  /// <summary>
  /// Check if a file exists without full metadata.
  /// </summary>
  static bool Exists(const std::wstring &path) noexcept;

  /// <summary>
  /// Get file size only.
  /// </summary>
  static std::optional<uint64_t> GetFileSize(const std::wstring &path) noexcept;

private:
  FileProbe() = delete;

  static std::string FileTimeToIso8601(const LARGE_INTEGER &ft) noexcept;
};

} // namespace evidence
} // namespace uboot
