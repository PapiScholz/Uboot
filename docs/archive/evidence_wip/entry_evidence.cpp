#include "entry_evidence.h"
#include <Windows.h>

namespace uboot {
namespace evidence {

EntryEvidence EntryEvidenceCollector::Collect(const Entry &entry) noexcept {
  EntryEvidence evidence;

  // Convert entry fields to wide strings
  evidence.source = ToWideString(entry.source);
  evidence.scope = ToWideString(entry.scope);
  evidence.key = ToWideString(entry.key);
  evidence.imagePath = ToWideString(entry.imagePath);

  std::wstring commandOrPath = ToWideString(entry.location);
  if (!entry.arguments.empty()) {
    commandOrPath += L" " + ToWideString(entry.arguments);
  }

  // Collect all evidence
  return CollectForFile(evidence.imagePath, commandOrPath);
}

EntryEvidence EntryEvidenceCollector::CollectForFile(
    const std::wstring &filePath, const std::wstring &commandLine) noexcept {
  EntryEvidence evidence;
  evidence.imagePath = filePath;

  // 1. File metadata (fast, always run)
  evidence.fileMetadata = FileProbe::Probe(filePath);

  // 2. Misconfiguration checks (fast, always run)
  std::wstring cmdToCheck = commandLine.empty() ? filePath : commandLine;
  evidence.misconfigs = MisconfigChecker::Check(cmdToCheck, filePath);

  // Only proceed with expensive operations if file exists
  if (evidence.fileMetadata.exists) {
    // 3. Hash computation (moderate cost)
    HashEvidence hashResult = Hashing::ComputeHashes(filePath);
    if (hashResult.IsSuccess()) {
      if (hashResult.md5)
        evidence.md5Hex = Hashing::ToHex(*hashResult.md5);
      if (hashResult.sha1)
        evidence.sha1Hex = Hashing::ToHex(*hashResult.sha1);
      if (hashResult.sha256)
        evidence.sha256Hex = Hashing::ToHex(*hashResult.sha256);
    } else {
      evidence.hashError = hashResult.ntStatus;
    }

    // 4. Version info extraction (low cost)
    evidence.versionInfo = VersionInfoExtractor::Extract(filePath);

    // 5. Authenticode verification (moderate cost)
    evidence.authenticode = AuthenticodeVerifier::Verify(filePath);
  }

  return evidence;
}

std::wstring
EntryEvidenceCollector::ToWideString(const std::string &str) noexcept {
  if (str.empty()) {
    return L"";
  }

  int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
  if (size <= 0) {
    return L"";
  }

  std::wstring wstr(size - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);

  return wstr;
}

} // namespace evidence
} // namespace uboot
