#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstdint>
#include <Windows.h>
#include <wintrust.h>
#include <softpub.h>

namespace uboot {
namespace evidence {

/// <summary>
/// Certificate information from a code signing certificate.
/// </summary>
struct CertificateInfo {
    std::wstring subjectName;
    std::wstring issuerName;
    std::optional<std::wstring> serialNumber;
    FILETIME notBefore = {};
    FILETIME notAfter = {};
};

/// <summary>
/// Result of Authenticode signature verification.
/// All CERT_TRUST flags are preserved without transformation.
/// Offline-first: uses WTD_REVOKE_NONE and WTD_CACHE_ONLY_URL_RETRIEVAL.
/// </summary>
struct AuthenticodeResult {
    bool isSigned = false;
    
    // WinVerifyTrust result
    std::optional<LONG> trustStatus;  // HRESULT from WinVerifyTrust
    
    // Certificate chain trust flags (CERT_TRUST_STATUS)
    // Preserved as-is for deterministic offline analysis
    std::optional<DWORD> chainErrorStatus;
    std::optional<DWORD> chainInfoStatus;
    
    // Certificate information
    std::vector<CertificateInfo> certificateChain;
    
    // Signer info
    std::optional<std::wstring> signerSubject;
    std::optional<std::wstring> signerIssuer;
    
    // Countersignature timestamp (if present)
    std::optional<FILETIME> timestamp;
    
    // Error state
    std::optional<DWORD> win32Error;
    
    bool IsSuccess() const {
        return trustStatus.has_value() && 
               (trustStatus.value() == ERROR_SUCCESS || 
                trustStatus.value() == CERT_E_UNTRUSTEDROOT);  // Self-signed is "success" for analysis
    }
};

/// <summary>
/// Verifies Authenticode signatures using WinVerifyTrust.
/// Strictly offline-first and deterministic.
/// 
/// CRITICAL REQUIREMENTS:
/// - Uses WTD_REVOKE_NONE (no revocation checks)
/// - Uses WTD_CACHE_ONLY_URL_RETRIEVAL (no network access)
/// - Reports all CERT_TRUST flags without transformation
/// - Same input → same output regardless of Win10/Win11
/// </summary>
class AuthenticodeVerifier {
public:
    /// <summary>
    /// Verify Authenticode signature of a PE file.
    /// </summary>
    /// <param name="filePath">Full path to PE file</param>
    /// <returns>AuthenticodeResult with all trust flags preserved</returns>
    static AuthenticodeResult Verify(const std::wstring& filePath) noexcept;
    
private:
    AuthenticodeVerifier() = delete;
    
    static void ExtractCertificateChain(
        HCERTSTORE hStore,
        PCCERT_CONTEXT pCert,
        AuthenticodeResult& result
    ) noexcept;
    
    static std::wstring GetCertificateNameString(
        PCCERT_CONTEXT pCert,
        DWORD dwType,
        DWORD dwFlags
    ) noexcept;
};

} // namespace evidence
} // namespace uboot
