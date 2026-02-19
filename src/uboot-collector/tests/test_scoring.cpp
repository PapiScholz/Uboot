#include "../evidence/entry_evidence.h"
#include "../model/Entry.h"
#include "../scoring/RulesLoader.h"
#include "../scoring/ScoringEngine.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include <vector>


using namespace uboot;
using namespace uboot::scoring;

// Helper to create a basic scoring config
ScoringConfig CreateTestConfig() {
  ScoringConfig config;
  config.model = "weighted_sum";
  config.version = "1.0-test";

  // Thresholds
  config.thresholds = {{"High Risk", 80.0},
                       {"Suspicious", 50.0},
                       {"Warning", 20.0},
                       {"Clean", 0.0}};

  // Helper to add signal
  auto addSig = [&](const std::string &id, double w) {
    Signal s;
    s.id = id;
    s.description = "Test " + id;
    s.weight = w;
    config.signals[id] = s;
  };

  addSig("unsigned_binary", 20.0);
  addSig("invalid_signature", 50.0);
  addSig("missing_target", 10.0);
  addSig("user_writable_location", 40.0);
  addSig("lolbin_autorun", 70.0);
  addSig("obfuscated_command", 30.0);
  addSig("relative_path", 15.0);
  addSig("system32_legit_signed", -50.0);

  config.lolbins = {"powershell.exe", "cmd.exe"};

  return config;
}

void PrintResult(const std::string &testName, const ScoreResult &result) {
  std::cout << "Test: " << testName << std::endl;
  std::cout << "  Score: " << result.totalScore << std::endl;
  std::cout << "  Class: " << result.value << std::endl;
  std::cout << "  Model: " << result.modelVersion << std::endl;
  std::cout << "  Time:  " << result.timestamp << std::endl;
  if (result.fileHash.has_value()) {
    std::cout << "  Hash:  " << result.fileHash.value() << std::endl;
  }
  std::cout << "  Evidence: " << std::endl;
  for (const auto &e : result.evidence) {
    std::cout << "    - " << e.signalId << ": " << e.evidence << " ("
              << e.weight << ")" << std::endl;
  }
  std::cout << "------------------------------------------------" << std::endl;
}

// Test Case 1: Clean Entry
bool TestCleanEntry() {
  ScoringConfig config = CreateTestConfig();
  ScoringEngine engine(config);

  Entry entry("Run", "Machine", "Key", "C:\\Windows\\System32\\svchost.exe", "",
              "C:\\Windows\\System32\\svchost.exe");

  evidence::EntryEvidence evidence;
  evidence.imagePath = L"C:\\Windows\\System32\\svchost.exe";
  evidence.authenticode.isSigned = true;
  evidence.authenticode.trustStatus = ERROR_SUCCESS;
  evidence.authenticode.signerSubject = L"Microsoft Windows Publisher";
  evidence.sha256Hex = "deadbeef"; // fake hash

  ScoreResult result = engine.Evaluate(entry, evidence);

  PrintResult("Clean Entry", result);

  if (result.totalScore > 0)
    return false;
  if (result.value != "Clean")
    return false;
  if (result.fileHash != "deadbeef")
    return false;

  return true;
}

// Test Case 2: Unsigned Binary
bool TestUnsignedBinary() {
  ScoringConfig config = CreateTestConfig();
  ScoringEngine engine(config);

  Entry entry("Run", "User", "MyApp", "C:\\App\\myapp.exe", "",
              "C:\\App\\myapp.exe");

  evidence::EntryEvidence evidence;
  evidence.imagePath = L"C:\\App\\myapp.exe";
  evidence.authenticode.isSigned = false;

  ScoreResult result = engine.Evaluate(entry, evidence);

  PrintResult("Unsigned Binary", result);

  if (result.totalScore != 20.0)
    return false;
  if (result.value != "Warning")
    return false;

  return true;
}

// Test Case 3: Suspicious LOLBin
bool TestSuspiciousLolbin() {
  ScoringConfig config = CreateTestConfig();
  ScoringEngine engine(config);

  Entry entry("Run", "User", "Updater", "powershell.exe", "-enc KA...",
              "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");

  evidence::EntryEvidence evidence;
  evidence.imagePath =
      L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
  evidence.authenticode.isSigned = true;
  evidence.authenticode.trustStatus = ERROR_SUCCESS;
  evidence.authenticode.signerSubject = L"Microsoft Corporation";

  ScoreResult result = engine.Evaluate(entry, evidence);

  PrintResult("Suspicious LOLBin", result);

  // lolbin_autorun (70) + system32_legit_signed (-50) = 20
  if (result.totalScore != 20.0)
    return false;

  bool foundLolbin = false;
  for (const auto &ev : result.evidence) {
    if (ev.signalId == "lolbin_autorun")
      foundLolbin = true;
  }
  if (!foundLolbin)
    return false;

  return true;
}

// Test Case: Config validation tolerance
bool TestUnknownSignalInConfig() {
  ScoringConfig config = CreateTestConfig();
  // Add unknown signal
  Signal s;
  s.id = "unknown_sig";
  s.weight = 100.0;
  config.signals["unknown_sig"] = s;

  ScoringEngine engine(config);
  // Should initialize without crashing, ignoring the unknown signal

  Entry entry("Run", "User", "Test", "C:\\test.exe", "", "C:\\test.exe");
  evidence::EntryEvidence evidence;
  evidence.imagePath = L"C:\\test.exe";

  // Just ensure it runs
  ScoreResult result = engine.Evaluate(entry, evidence);
  PrintResult("Unknown Signal Tolerance", result);

  return true;
}

int main() {
  std::cout << "Starting Refactored Scoring Engine Tests..." << std::endl;

  bool allPassed = true;

  if (!TestCleanEntry()) {
    std::cout << "FAILED: TestCleanEntry" << std::endl;
    allPassed = false;
  }
  if (!TestUnsignedBinary()) {
    std::cout << "FAILED: TestUnsignedBinary" << std::endl;
    allPassed = false;
  }
  if (!TestSuspiciousLolbin()) {
    std::cout << "FAILED: TestSuspiciousLolbin" << std::endl;
    allPassed = false;
  }
  if (!TestUnknownSignalInConfig()) {
    std::cout << "FAILED: TestUnknownSignalInConfig" << std::endl;
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
  } else {
    std::cout << "SOME TESTS FAILED" << std::endl;
    return 1;
  }
}
