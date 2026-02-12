#include "authenticode.h"
#include <wincrypt.h>
#include <memory>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

namespace uboot {
namespace evidence {

namespace {
    // RAII wrapper for HCERTSTORE
    struct CertStoreDeleter {
        void operator()(HCERTSTORE h) const {
            if (h) CertCloseStore(h, 0);
        }
    };
    using CertStorePtr = std::unique_ptr<std::remove_pointer_t<HCERTSTORE>, CertStoreDeleter>;
    
    // RAII wrapper for PCCERT_CONTEXT
    struct CertContextDeleter {
        void operator()(PCCERT_CONTEXT p) const {
            if (p) CertFreeCertificateContext(p);
        }
    };
    using CertContextPtr = std::unique_ptr<const CERT_CONTEXT, CertContextDeleter>;
    
    // RAII wrapper for PCCERT_CHAIN_CONTEXT
    struct CertChainContextDeleter {
        void operator()(PCCERT_CHAIN_CONTEXT p) const {
            if (p) CertFreeCertificateChain(p);
        }
    };
    using CertChainContextPtr = std::unique_ptr<const CERT_CHAIN_CONTEXT, CertChainContextDeleter>;
}

AuthenticodeResult AuthenticodeVerifier::Verify(const std::wstring& filePath) noexcept {
    AuthenticodeResult result;
    
    // Setup WINTRUST_FILE_INFO
    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = filePath.c_str();
    fileInfo.hFile = nullptr;
    fileInfo.pgKnownSubject = nullptr;
    
    // Setup WINTRUST_DATA
    WINTRUST_DATA winTrustData = {};
    winTrustData.cbStruct = sizeof(WINTRUST_DATA);
    winTrustData.pPolicyCallbackData = nullptr;
    winTrustData.pSIPClientData = nullptr;
    winTrustData.dwUIChoice = WTD_UI_NONE;
    winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;  // CRITICAL: No revocation checks
    winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    winTrustData.pFile = &fileInfo;
    winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    winTrustData.hWVTStateData = nullptr;
    winTrustData.pwszURLReference = nullptr;
    winTrustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;  // CRITICAL: No network access
    winTrustData.dwUIContext = 0;
    
    // Action ID for Authenticode verification
    GUID actionID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    
    // Verify signature
    LONG trustStatus = WinVerifyTrust(
        static_cast<HWND>(INVALID_HANDLE_VALUE),
        &actionID,
        &winTrustData
    );
    
    result.trustStatus = trustStatus;
    
    // Check if file has a signature
    if (trustStatus == TRUST_E_NOSIGNATURE || trustStatus == TRUST_E_SUBJECT_FORM_UNKNOWN) {
        result.isSigned = false;
        
        // Clean up WinVerifyTrust state
        winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(nullptr, &actionID, &winTrustData);
        
        return result;
    }
    
    result.isSigned = true;
    
    // Get provider data to access certificate chain
    CRYPT_PROVIDER_DATA* pProvData = WTHelperProvDataFromStateData(winTrustData.hWVTStateData);
    if (pProvData) {
        CRYPT_PROVIDER_SGNR* pSigner = WTHelperGetProvSignerFromChain(pProvData, 0, FALSE, 0);
        if (pSigner) {
            // Get signer certificate
            CRYPT_PROVIDER_CERT* pSignerCert = WTHelperGetProvCertFromChain(pSigner, 0);
            if (pSignerCert && pSignerCert->pCert) {
                result.signerSubject = GetCertificateNameString(
                    pSignerCert->pCert,
                    CERT_NAME_SIMPLE_DISPLAY_TYPE,
                    0
                );
                
                result.signerIssuer = GetCertificateNameString(
                    pSignerCert->pCert,
                    CERT_NAME_SIMPLE_DISPLAY_TYPE,
                    CERT_NAME_ISSUER_FLAG
                );
            }
            
            // Get timestamp if present
            if (pSigner->psSigner && pSigner->psSigner->AuthAttrs.cAttr > 0) {
                // Check for countersignature
                for (DWORD i = 0; i < pSigner->csCounterSigners; ++i) {
                    CRYPT_PROVIDER_SGNR* pCounterSigner = &pSigner->pasCounterSigners[i];
                    if (pCounterSigner && pCounterSigner->sftVerifyAsOf.dwLowDateTime != 0) {
                        result.timestamp = pCounterSigner->sftVerifyAsOf;
                        break;
                    }
                }
            }
        }
        
        // Extract certificate chain with CERT_TRUST flags
        if (pSigner && pSigner->pChainContext) {
            PCCERT_CHAIN_CONTEXT pChainContext = pSigner->pChainContext;
            
            // CRITICAL: Preserve CERT_TRUST_STATUS flags without transformation
            // These flags must be reported as-is for deterministic offline analysis
            result.chainErrorStatus = pChainContext->TrustStatus.dwErrorStatus;
            result.chainInfoStatus = pChainContext->TrustStatus.dwInfoStatus;
            
            // Extract certificate chain
            if (pChainContext->cChain > 0) {
                PCERT_SIMPLE_CHAIN pSimpleChain = pChainContext->rgpChain[0];
                if (pSimpleChain) {
                    for (DWORD i = 0; i < pSimpleChain->cElement; ++i) {
                        PCERT_CHAIN_ELEMENT pElement = pSimpleChain->rgpElement[i];
                        if (pElement && pElement->pCertContext) {
                            CertificateInfo certInfo;
                            
                            certInfo.subjectName = GetCertificateNameString(
                                pElement->pCertContext,
                                CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                0
                            );
                            
                            certInfo.issuerName = GetCertificateNameString(
                                pElement->pCertContext,
                                CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                CERT_NAME_ISSUER_FLAG
                            );
                            
                            certInfo.notBefore = pElement->pCertContext->pCertInfo->NotBefore;
                            certInfo.notAfter = pElement->pCertContext->pCertInfo->NotAfter;
                            
                            // Get serial number as hex string
                            CRYPT_INTEGER_BLOB& serial = pElement->pCertContext->pCertInfo->SerialNumber;
                            std::wostringstream oss;
                            oss << std::hex << std::setfill(L'0');
                            for (DWORD j = serial.cbData; j > 0; --j) {
                                oss << std::setw(2) << static_cast<int>(serial.pbData[j - 1]);
                            }
                            certInfo.serialNumber = oss.str();
                            
                            result.certificateChain.push_back(std::move(certInfo));
                        }
                    }
                }
            }
        }
    }
    
    // Clean up WinVerifyTrust state
    winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &actionID, &winTrustData);
    
    return result;
}

std::wstring AuthenticodeVerifier::GetCertificateNameString(
    PCCERT_CONTEXT pCert,
    DWORD dwType,
    DWORD dwFlags
) noexcept {
    if (!pCert) {
        return L"";
    }
    
    DWORD size = CertGetNameStringW(pCert, dwType, dwFlags, nullptr, nullptr, 0);
    if (size <= 1) {
        return L"";
    }
    
    std::wstring name(size - 1, L'\0');
    CertGetNameStringW(pCert, dwType, dwFlags, nullptr, &name[0], size);
    
    return name;
}

} // namespace evidence
} // namespace uboot
