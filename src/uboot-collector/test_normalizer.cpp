/*
 * Unit tests for CommandNormalizer and LnkResolver
 * 
 * To compile and run:
 *   cl /EHsc /std:c++17 test_normalizer.cpp 
 *      CommandNormalizer.cpp LnkResolver.cpp 
 *      /link pathcch.lib shlwapi.lib shell32.lib ole32.lib
 */

#include <cassert>
#include <iostream>
#include <string>
#include "normalize/CommandNormalizer.h"
#include "resolve/LnkResolver.h"
#include "util/CommandResolver.h"

using namespace uboot;

// Helper for test assertions
#define ASSERT_EQ(actual, expected, message) \
    if ((actual) != (expected)) { \
        std::cerr << "FAIL: " << (message) << "\n"; \
        std::cerr << "  Expected: " << (expected) << "\n"; \
        std::cerr << "  Actual:   " << (actual) << "\n"; \
        return false; \
    }

#define TEST_CASE(name) \
    std::cout << "Testing: " << (name) << "... "; \
    if (!(test_##name())) { \
        std::cout << "FAILED\n"; \
        return 1; \
    } \
    std::cout << "PASSED\n"

// Test 1: Simple executable
bool test_simple_exe() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "C:\\Windows\\System32\\cmd.exe arg1 arg2"
    );
    
    ASSERT_EQ(result.resolvedPath.find("cmd.exe"), size_t(0) == false, "cmd.exe in path");
    ASSERT_EQ(result.arguments.find("arg1"), size_t(0) == false, "arg1 in arguments");
    ASSERT_EQ(result.isComplete, true, "isComplete should be true");
    
    return true;
}

// Test 2: Quoted path
bool test_quoted_path() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "\"C:\\Program Files\\app.exe\" argument1"
    );
    
    ASSERT_EQ(result.resolvedPath.find("app.exe"), size_t(0) == false, "app.exe in path");
    ASSERT_EQ(result.arguments.find("argument1"), size_t(0) == false, "argument1 in arguments");
    
    return true;
}

// Test 3: Environment variable expansion
bool test_env_expansion() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "%SYSTEMROOT%\\System32\\notepad.exe test.txt"
    );
    
    // Should have expanded %SYSTEMROOT%
    ASSERT_EQ(result.resolveNotes.find("Expanded"), size_t(0) == false, "Should note expansion");
    ASSERT_EQ(result.resolvedPath.find("%"), size_t(0) == false, "Should not contain % after expansion");
    
    return true;
}

// Test 4: cmd.exe wrapper peeling
bool test_cmd_wrapper() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "cmd.exe /c \"C:\\app.exe\" arg1 arg2"
    );
    
    ASSERT_EQ(result.resolveNotes.find("cmd.exe"), size_t(0) == false, "Should mention cmd.exe peeling");
    ASSERT_EQ(result.resolvedPath.find("app.exe"), size_t(0) == false, "Should peel to find app.exe");
    ASSERT_EQ(result.arguments.find("arg1"), size_t(0) == false, "Arguments should be preserved");
    
    return true;
}

// Test 5: PowerShell wrapper
bool test_powershell_wrapper() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "powershell.exe -Command \"C:\\tool.exe\" -param value"
    );
    
    ASSERT_EQ(result.resolveNotes.find("powershell"), size_t(0) == false, "Should mention powershell peeling");
    ASSERT_EQ(result.resolvedPath.find("tool.exe"), size_t(0) == false, "Should extract tool.exe");
    
    return true;
}

// Test 6: Whitespace sanitization
bool test_whitespace_sanitization() {
    // Tab, non-breaking space, regular spaces mixed
    std::string command = "C:\\app.exe\t\xC2\xA0arg1  arg2";
    NormalizationResult result = CommandNormalizer::Normalize(command);
    
    ASSERT_EQ(result.resolveNotes.find("Sanitized"), size_t(0) == false, "Should sanitize whitespace");
    // Multiple spaces should be collapsed
    ASSERT_EQ(result.arguments.find("  "), std::string::npos, "Should not have multiple consecutive spaces");
    
    return true;
}

// Test 7: Mismatched quotes (tolerant parsing)
bool test_mismatched_quotes() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "\"C:\\Program Files\\app.exe arg1 arg2"
    );
    
    // Should still try to resolve despite bad quoting
    ASSERT_EQ(result.resolvedPath.empty(), false, "Should attempt to resolve despite bad quotes");
    
    return true;
}

// Test 8: Relative path resolution
bool test_relative_path() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "app.exe arg1",
        "C:\\Users\\user\\Desktop"
    );
    
    // Should show it's working with the path
    ASSERT_EQ(result.isComplete, false, "app.exe without full path should not be complete");
    ASSERT_EQ(result.resolveNotes.find("Canonicalized"), size_t(0) == false, "Should attempt canonicalization");
    
    return true;
}

// Test 9: Multiple wrapper peeling
bool test_nested_wrappers() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "cmd.exe /c powershell.exe -Command \"C:\\app.exe\" arg"
    );
    
    // Should peel both cmd.exe and powershell.exe
    ASSERT_EQ(result.resolveNotes.find("cmd.exe"), size_t(0) == false, "Should peel cmd.exe");
    ASSERT_EQ(result.resolveNotes.find("powershell"), size_t(0) == false, "Should peel powershell");
    
    return true;
}

// Test 10: CommandResolver integration
bool test_command_resolver() {
    NormalizationResult result = CommandResolver::ResolveCommand(
        "cmd.exe /c C:\\Windows\\System32\\calc.exe"
    );
    
    ASSERT_EQ(result.isComplete, true, "Should resolve successfully");
    
    return true;
}

// Test 11: LnkResolver (if file exists)
bool test_lnk_resolver() {
    // This test requires a .lnk file to exist
    // In a real test environment, create a sample .lnk first
    // For now, just show the API works
    LnkResolutionResult result = LnkResolver::Resolve(
        "C:\\NonExistent\\File.lnk"
    );
    
    // Should handle missing file gracefully
    ASSERT_EQ(result.isResolved, false, "Should fail for non-existent file");
    ASSERT_EQ(result.resolveNotes.empty(), false, "Should provide error notes");
    
    return true;
}

// Test 12: ScriptHost wrappers
bool test_scripthost_wrappers() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "cscript.exe \"C:\\script.vbs\" arg1"
    );
    
    ASSERT_EQ(result.resolveNotes.find("cscript"), size_t(0) == false, "Should peel cscript");
    ASSERT_EQ(result.resolvedPath.find("script.vbs"), size_t(0) == false, "Should extract script path");
    
    return true;
}

// Test 13: Rundll32 wrapper
bool test_rundll32_wrapper() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "rundll32.exe shell32.dll,ShellExec_RunDLL C:\\app.exe arg1"
    );
    
    ASSERT_EQ(result.resolveNotes.find("rundll32"), size_t(0) == false, "Should peel rundll32");
    
    return true;
}

// Test 14: MSHTA wrapper
bool test_mshta_wrapper() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "mshta.exe \"C:\\script.hta\" param1"
    );
    
    ASSERT_EQ(result.resolveNotes.find("mshta"), size_t(0) == false, "Should peel mshta");
    
    return true;
}

// Test 15: Regsvr32 wrapper
bool test_regsvr32_wrapper() {
    NormalizationResult result = CommandNormalizer::Normalize(
        "regsvr32.exe /s C:\\lib.dll"
    );
    
    ASSERT_EQ(result.resolveNotes.find("regsvr32"), size_t(0) == false, "Should peel regsvr32");
    ASSERT_EQ(result.resolvedPath.find("lib.dll"), size_t(0) == false, "Should extract dll path");
    
    return true;
}

int main() {
    std::cout << "\n=== CommandNormalizer and LnkResolver Unit Tests ===\n\n";
    
    TEST_CASE(simple_exe);
    TEST_CASE(quoted_path);
    TEST_CASE(env_expansion);
    TEST_CASE(cmd_wrapper);
    TEST_CASE(powershell_wrapper);
    TEST_CASE(whitespace_sanitization);
    TEST_CASE(mismatched_quotes);
    TEST_CASE(relative_path);
    TEST_CASE(nested_wrappers);
    TEST_CASE(command_resolver);
    TEST_CASE(lnk_resolver);
    TEST_CASE(scripthost_wrappers);
    TEST_CASE(rundll32_wrapper);
    TEST_CASE(mshta_wrapper);
    TEST_CASE(regsvr32_wrapper);
    
    std::cout << "\n=== All tests completed ===\n\n";
    
    return 0;
}
