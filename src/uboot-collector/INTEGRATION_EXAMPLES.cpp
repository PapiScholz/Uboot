/*
 * Example Integration: How to use CommandNormalizer and LnkResolver
 * in existing collectors
 */

#include "normalize/CommandNormalizer.h"
#include "resolve/LnkResolver.h"
#include "util/CommandResolver.h"
#include "model/Entry.h"
#include <string>
#include <iostream>

namespace uboot {

// ============================================================================
// EXAMPLE 1: Using CommandNormalizer in a Registry Collector
// ============================================================================

class RegistryCollectorExample {
public:
    static void ProcessRegistryEntry(
        const std::string& registryKey,
        const std::string& rawCommand,
        std::vector<Entry>& entries
    ) {
        // Get the command from registry (already in UTF-8 from registry reading)
        std::string command = rawCommand;
        
        // Normalize it using CommandNormalizer
        NormalizationResult normalized = CommandNormalizer::Normalize(
            command,
            "C:\\Windows\\System32"  // typical working directory
        );
        
        // Create an Entry with normalized data
        Entry entry(
            "RunRegistry",           // source
            "Machine",               // scope
            registryKey,             // unique key
            registryKey,             // location (registry path)
            normalized.arguments,    // arguments (now separated)
            normalized.resolvedPath  // imagePath (now canonicalized)
        );
        
        // Optionally store resolution metadata
        if (!normalized.isComplete) {
            entry.description = "Partial resolution: " + normalized.resolveNotes;
        }
        
        entries.push_back(entry);
    }
};

// ============================================================================
// EXAMPLE 2: Using LnkResolver in Startup Folder Collector
// ============================================================================

class StartupFolderCollectorExample {
public:
    static void ProcessStartupFile(
        const std::string& filePath,
        const std::string& fileName,
        std::vector<Entry>& entries
    ) {
        Entry entry(
            "StartupFolder",
            "User",
            fileName,
            filePath,
            "",
            ""
        );
        
        // Check if it's a shortcut
        if (filePath.length() >= 4 && 
            filePath.substr(filePath.length() - 4) == ".lnk") {
            
            // Resolve the shortcut
            LnkResolutionResult lnkResult = LnkResolver::Resolve(filePath);
            
            if (lnkResult.isResolved) {
                entry.imagePath = lnkResult.targetPath;
                entry.arguments = lnkResult.arguments;
                
                if (!lnkResult.workingDirectory.empty()) {
                    entry.description = "Working dir: " + lnkResult.workingDirectory;
                }
            } else {
                // Resolution failed - store original path and error
                entry.imagePath = filePath;
                entry.description = "Failed to resolve shortcut: " + lnkResult.resolveNotes;
            }
        } else {
            // Direct executable - normalize it
            NormalizationResult normalized = CommandNormalizer::Normalize(filePath);
            entry.imagePath = normalized.resolvedPath;
            entry.arguments = normalized.arguments;
        }
        
        entries.push_back(entry);
    }
};

// ============================================================================
// EXAMPLE 3: Complete Pipeline with CommandResolver
// ============================================================================

class CompleteResolverExample {
public:
    static void ProcessAnyCommand(
        const std::string& source,
        const std::string& scope,
        const std::string& identifier,
        const std::string& location,
        const std::string& rawCommandOrPath,
        std::vector<Entry>& entries
    ) {
        // Use CommandResolver which handles both:
        // - Direct commands
        // - .lnk files
        // - Wrappers
        // - Canonicalization
        
        NormalizationResult result = CommandResolver::ResolveCommand(
            rawCommandOrPath,
            "C:\\Windows\\System32"  // default working dir
        );
        
        Entry entry(
            source,
            scope,
            identifier,
            location,
            result.arguments,
            result.resolvedPath
        );
        
        // Store original command for audit trail
        if (entry.imagePath != result.originalCommand) {
            entry.description = "Original: " + result.originalCommand;
            if (!result.resolveNotes.empty()) {
                entry.description += " | " + result.resolveNotes;
            }
        }
        
        // Mark if resolution was incomplete
        if (!result.isComplete) {
            entry.keyName = "PARTIAL_RESOLUTION";
        }
        
        entries.push_back(entry);
    }
};

// ============================================================================
// EXAMPLE 4: Error Handling Pattern
// ============================================================================

class RobustCollectorExample {
public:
    static void CollectWithErrorHandling(
        const std::string& sourceName,
        std::vector<Entry>& entries,
        std::vector<CollectorError>& errors
    ) {
        try {
            // Try to process multiple entries
            std::vector<std::string> commandsToProcess = {
                "C:\\Windows\\System32\\cmd.exe /c notepad.exe",
                "powershell.exe -Command C:\\scripts\\script.ps1",
                "C:\\Program Files\\App\\app.exe --arg=value",
                "%PROGRAMFILES%\\Tool\\tool.exe"
            };
            
            for (const auto& cmd : commandsToProcess) {
                try {
                    NormalizationResult result = CommandNormalizer::Normalize(cmd);
                    
                    if (result.isComplete) {
                        Entry entry(sourceName, "Auto", cmd, cmd,
                                   result.arguments, result.resolvedPath);
                        entries.push_back(entry);
                    } else {
                        // Partial resolution - still add but mark it
                        Entry entry(sourceName, "Auto", cmd, cmd,
                                   result.arguments, result.resolvedPath);
                        entry.description = "Partial: " + result.resolveNotes;
                        entries.push_back(entry);
                    }
                    
                } catch (const std::exception& e) {
                    // Should not happen (no exceptions in our code)
                    // but add error handling for robustness
                    errors.push_back(CollectorError(
                        sourceName,
                        std::string("Exception processing command: ") + e.what()
                    ));
                }
            }
            
        } catch (const std::exception& e) {
            errors.push_back(CollectorError(
                sourceName,
                std::string("Collector exception: ") + e.what()
            ));
        }
    }
};

// ============================================================================
// EXAMPLE 5: Testing Edge Cases
// ============================================================================

void TestEdgeCases() {
    std::cout << "TEST 1: Simple command\n";
    {
        NormalizationResult r = CommandNormalizer::Normalize("notepad.exe");
        std::cout << "  Path: " << r.resolvedPath << "\n";
        std::cout << "  Args: " << r.arguments << "\n";
        std::cout << "  Complete: " << (r.isComplete ? "yes" : "no") << "\n";
    }
    
    std::cout << "\nTEST 2: Wrapper peeling\n";
    {
        NormalizationResult r = CommandNormalizer::Normalize(
            "cmd.exe /c C:\\Windows\\System32\\calc.exe"
        );
        std::cout << "  Path: " << r.resolvedPath << "\n";
        std::cout << "  Notes: " << r.resolveNotes << "\n";
    }
    
    std::cout << "\nTEST 3: Env expansion\n";
    {
        NormalizationResult r = CommandNormalizer::Normalize(
            "%SYSTEMROOT%\\System32\\cmd.exe",
            "%TEMP%"
        );
        std::cout << "  Path: " << r.resolvedPath << "\n";
        std::cout << "  Notes: " << r.resolveNotes << "\n";
    }
    
    std::cout << "\nTEST 4: Quoted path with spaces\n";
    {
        NormalizationResult r = CommandNormalizer::Normalize(
            "\"C:\\Program Files\\app.exe\" arg1 arg2"
        );
        std::cout << "  Path: " << r.resolvedPath << "\n";
        std::cout << "  Args: " << r.arguments << "\n";
    }
    
    std::cout << "\nTEST 5: .lnk file\n";
    {
        // If you have a .lnk file to test:
        LnkResolutionResult r = LnkResolver::Resolve(
            "C:\\Users\\user\\Desktop\\Example.lnk"
        );
        std::cout << "  Resolved: " << (r.isResolved ? "yes" : "no") << "\n";
        if (r.isResolved) {
            std::cout << "  Target: " << r.targetPath << "\n";
            std::cout << "  Args: " << r.arguments << "\n";
        } else {
            std::cout << "  Error: " << r.resolveNotes << "\n";
        }
    }
    
    std::cout << "\nTEST 6: Nested wrappers\n";
    {
        NormalizationResult r = CommandNormalizer::Normalize(
            "cmd.exe /c powershell.exe -Command \"C:\\app.exe\" arg"
        );
        std::cout << "  Path: " << r.resolvedPath << "\n";
        std::cout << "  Args: " << r.arguments << "\n";
        std::cout << "  Notes: " << r.resolveNotes << "\n";
    }
}

} // namespace uboot

// ============================================================================
// Main: Example usage
// ============================================================================

int main() {
    using namespace uboot;
    
    std::cout << "=== Command Normalization Examples ===\n\n";
    
    // Example 1: Registry Entry
    std::cout << "EXAMPLE 1: Registry Entry Processing\n";
    {
        std::vector<Entry> entries;
        RegistryCollectorExample::ProcessRegistryEntry(
            "HKLM\\Software\\Microsoft\\Windows\\Run\\Example",
            "cmd.exe /c %PROGRAMFILES%\\app.exe --flag",
            entries
        );
        
        if (!entries.empty()) {
            std::cout << "  Source: " << entries[0].source << "\n";
            std::cout << "  Path: " << entries[0].imagePath << "\n";
            std::cout << "  Args: " << entries[0].arguments << "\n";
        }
    }
    
    std::cout << "\n";
    
    // Example 2: Startup Folder
    std::cout << "EXAMPLE 2: Startup Folder File Processing\n";
    {
        std::vector<Entry> entries;
        StartupFolderCollectorExample::ProcessStartupFile(
            "C:\\Users\\user\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\shortcut.lnk",
            "shortcut.lnk",
            entries
        );
        
        if (!entries.empty()) {
            std::cout << "  Source: " << entries[0].source << "\n";
            std::cout << "  Path: " << entries[0].imagePath << "\n";
        }
    }
    
    std::cout << "\n";
    
    // Example 3: Complete Resolver
    std::cout << "EXAMPLE 3: Complete Resolver Pipeline\n";
    {
        std::vector<Entry> entries;
        CompleteResolverExample::ProcessAnyCommand(
            "AutoStart",
            "Machine",
            "test.exe",
            "HKLM\\Software\\Run\\Test",
            "powershell.exe -Command \"C:\\Tools\\script.ps1\" param",
            entries
        );
        
        if (!entries.empty()) {
            std::cout << "  Path: " << entries[0].imagePath << "\n";
            std::cout << "  Args: " << entries[0].arguments << "\n";
        }
    }
    
    std::cout << "\n";
    
    // Example 4: Edge Cases
    std::cout << "EXAMPLE 4: Edge Cases\n";
    TestEdgeCases();
    
    std::cout << "\n=== Examples completed ===\n";
    
    return 0;
}
