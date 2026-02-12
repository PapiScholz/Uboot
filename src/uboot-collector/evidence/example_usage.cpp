#include "evidence/entry_evidence.h"
#include <iostream>
#include <iomanip>

using namespace uboot::evidence;

/// <summary>
/// Example usage of the evidence collection module.
/// Demonstrates offline, deterministic file analysis.
/// </summary>
void PrintEvidence(const EntryEvidence& ev) {
    std::wcout << L"═══════════════════════════════════════\n";
    std::wcout << L"Evidence for: " << ev.imagePath << L"\n";
    std::wcout << L"═══════════════════════════════════════\n\n";
    
    // File existence
    std::wcout << L"[File Metadata]\n";
    std::wcout << L"  Exists: " << (ev.fileExists ? L"Yes" : L"No") << L"\n";
    if (ev.fileExists) {
        std::wcout << L"  Size: " << ev.fileMetadata.fileSize << L" bytes\n";
        std::wcout << L"  Attributes: 0x" << std::hex << ev.fileMetadata.attributes << std::dec << L"\n";
    } else if (ev.fileMetadata.win32Error.has_value()) {
        std::wcout << L"  Win32 Error: " << ev.fileMetadata.win32Error.value() << L"\n";
    }
    std::wcout << L"\n";
    
    // Hash
    std::wcout << L"[Hash]\n";
    if (ev.sha256Hex.has_value()) {
        std::cout << "  SHA-256: " << ev.sha256Hex.value() << "\n";
    } else if (ev.hashError.has_value()) {
        std::wcout << L"  Hash Error (NTSTATUS): 0x" << std::hex 
                   << ev.hashError.value() << std::dec << L"\n";
    } else {
        std::wcout << L"  Hash: N/A (file not found)\n";
    }
    std::wcout << L"\n";
    
    // Version info
    std::wcout << L"[Version Info]\n";
    if (ev.versionInfo.HasAnyInfo()) {
        if (ev.versionInfo.companyName.has_value())
            std::wcout << L"  Company: " << ev.versionInfo.companyName.value() << L"\n";
        if (ev.versionInfo.productName.has_value())
            std::wcout << L"  Product: " << ev.versionInfo.productName.value() << L"\n";
        if (ev.versionInfo.fileVersion.has_value())
            std::wcout << L"  Version: " << ev.versionInfo.fileVersion.value() << L"\n";
        if (ev.versionInfo.fileDescription.has_value())
            std::wcout << L"  Description: " << ev.versionInfo.fileDescription.value() << L"\n";
    } else {
        std::wcout << L"  No version info available\n";
    }
    std::wcout << L"\n";
    
    // Authenticode
    std::wcout << L"[Authenticode Signature]\n";
    std::wcout << L"  Signed: " << (ev.isSigned ? L"Yes" : L"No") << L"\n";
    if (ev.isSigned) {
        std::wcout << L"  Valid: " << (ev.signatureValid ? L"Yes" : L"No") << L"\n";
        
        if (ev.authenticode.trustStatus.has_value()) {
            std::wcout << L"  Trust Status: 0x" << std::hex 
                       << ev.authenticode.trustStatus.value() << std::dec << L"\n";
        }
        
        if (ev.authenticode.signerSubject.has_value()) {
            std::wcout << L"  Signer: " << ev.authenticode.signerSubject.value() << L"\n";
        }
        
        if (ev.authenticode.chainErrorStatus.has_value()) {
            std::wcout << L"  Chain Error Status: 0x" << std::hex 
                       << ev.authenticode.chainErrorStatus.value() << std::dec << L"\n";
        }
        
        if (ev.authenticode.chainInfoStatus.has_value()) {
            std::wcout << L"  Chain Info Status: 0x" << std::hex 
                       << ev.authenticode.chainInfoStatus.value() << std::dec << L"\n";
        }
        
        std::wcout << L"  Certificate Chain: " << ev.authenticode.certificateChain.size() 
                   << L" certificate(s)\n";
    }
    std::wcout << L"\n";
    
    // Misconfigurations
    std::wcout << L"[Misconfiguration Checks]\n";
    std::wcout << L"  Findings: " << ev.misconfigs.findings.size() << L"\n";
    std::wcout << L"  Critical: " << (ev.hasCriticalMisconfigs ? L"Yes" : L"No") << L"\n";
    
    if (ev.hasMisconfigs) {
        std::wcout << L"\n  Details:\n";
        for (const auto& finding : ev.misconfigs.findings) {
            std::wcout << L"    - " << finding.description << L"\n";
            std::wcout << L"      Value: " << finding.affectedValue << L"\n";
        }
    }
    
    std::wcout << L"\n";
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        std::wcout << L"Usage: " << argv[0] << L" <file_path> [command_line]\n";
        std::wcout << L"\nExamples:\n";
        std::wcout << L"  " << argv[0] << L" C:\\Windows\\System32\\notepad.exe\n";
        std::wcout << L"  " << argv[0] << L" C:\\App\\app.exe \"C:\\Some App\\app.exe /flag\"\n";
        return 1;
    }
    
    std::wstring filePath = argv[1];
    std::wstring commandLine = (argc >= 3) ? argv[2] : L"";
    
    std::wcout << L"Collecting evidence (offline, deterministic analysis)...\n\n";
    
    // Collect evidence
    EntryEvidence evidence = EntryEvidenceCollector::CollectForFile(filePath, commandLine);
    
    // Print results
    PrintEvidence(evidence);
    
    // Summary
    std::wcout << L"═══════════════════════════════════════\n";
    std::wcout << L"Summary:\n";
    std::wcout << L"  File exists: " << (evidence.fileExists ? L"✓" : L"✗") << L"\n";
    std::wcout << L"  Has signature: " << (evidence.isSigned ? L"✓" : L"✗") << L"\n";
    std::wcout << L"  Signature valid: " << (evidence.signatureValid ? L"✓" : L"✗") << L"\n";
    std::wcout << L"  Has misconfigs: " << (evidence.hasMisconfigs ? L"✓" : L"✗") << L"\n";
    std::wcout << L"  Critical issues: " << (evidence.hasCriticalMisconfigs ? L"✓" : L"✗") << L"\n";
    std::wcout << L"═══════════════════════════════════════\n";
    
    return 0;
}
