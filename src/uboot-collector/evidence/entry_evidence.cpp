#include "entry_evidence.h"
#include <Windows.h>

namespace uboot {
namespace evidence {

EntryEvidence EntryEvidenceCollector::Collect(const Entry& entry) noexcept {
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
    const std::wstring& filePath,
    const std::wstring& commandLine
) noexcept {
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
        HashResult hashResult = Hashing::ComputeSHA256(filePath);
        if (hashResult.IsSuccess()) {
            evidence.sha256Hex = Hashing::HashToHexString(hashResult.sha256.value());
        } else {
            evidence.hashError = hashResult.ntStatus;
        }
        
        // 4. Version info extraction (low cost)
        evidence.versionInfo = VersionInfoExtractor::Extract(filePath);
        
        // 5. Authenticode verification (moderate cost)
        evidence.authenticode = AuthenticodeVerifier::Verify(filePath);
    }
    
    // Populate summary flags
    PopulateSummaryFlags(evidence);
    
    return evidence;
}

void EntryEvidenceCollector::PopulateSummaryFlags(EntryEvidence& evidence) noexcept {
    evidence.fileExists = evidence.fileMetadata.exists;
    evidence.isSigned = evidence.authenticode.isSigned;
    
    // Signature is valid if:
    // - File is signed AND
    // - Trust status is SUCCESS (or CERT_E_UNTRUSTEDROOT for self-signed)
    evidence.signatureValid = 
        evidence.isSigned &&
        evidence.authenticode.trustStatus.has_value() &&
        (evidence.authenticode.trustStatus.value() == ERROR_SUCCESS ||
         evidence.authenticode.trustStatus.value() == static_cast<LONG>(CERT_E_UNTRUSTEDROOT));
    
    evidence.hasMisconfigs = evidence.misconfigs.HasFindings();
    evidence.hasCriticalMisconfigs = evidence.misconfigs.HasCriticalFindings();
}

std::wstring EntryEvidenceCollector::ToWideString(const std::string& str) noexcept {
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
