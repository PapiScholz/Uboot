#include "../evidence/entry_evidence.h"
#include "../model/Entry.h"
#include "../scoring/ScoringEngine.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>

using namespace uboot;
using namespace uboot::scoring;

// --- Helper Config ---
ScoringConfig CreatePhase3Config() {
  ScoringConfig config;
  config.model = "weighted_sum";
  config.version = "3.0-test";

  auto addSig = [&](const std::string &id, double w) {
    Signal s;
    s.id = id;
    s.description = "Test " + id;
    s.weight = w;
    config.signals[id] = s;
  };

  addSig("lolbin_autorun", 100.0);
  addSig("system32_legit_signed", -50.0);
  addSig("unsigned_binary", 20.0);

  config.lolbins = {"powershell.exe", "cmd.exe"};
  return config;
}

// --- Helper Serializer for Determinism Test ---
std::string SerializeEvidence(const std::vector<SignalEvidence> &evidence) {
  std::stringstream ss;
  ss << "[";
  for (size_t i = 0; i < evidence.size(); ++i) {
    if (i > 0)
      ss << ",";
    ss << "{\"id\":\"" << evidence[i].signalId << "\"}";
  }
  ss << "]";
  return ss.str();
}

// --- Tests ---

bool TestDeterminism() {
  std::cout << "[Test: Determinism] Running..." << std::endl;
  ScoringConfig config = CreatePhase3Config();

  // Create an engine.
  // Note: SignalRegistry is a singleton, so it might be initialized already.
  // That's fine for this test as long as we use standard signals.
  ScoringEngine engine(config);

  Entry entry("Run", "User", "Test", "cmd.exe", "/c calc",
              "C:\\Windows\\System32\\cmd.exe");
  evidence::EntryEvidence evidence;
  evidence.imagePath = L"C:\\Windows\\System32\\cmd.exe";
  evidence.authenticode.isSigned = true;
  evidence.authenticode.trustStatus = 0;
  evidence.authenticode.signerSubject = L"Microsoft Windows Publisher";

  // Run 3 times
  std::string firstOutput;
  for (int i = 0; i < 3; ++i) {
    ScoreResult result = engine.Evaluate(entry, evidence);

    // Verify sorting:
    for (size_t j = 0; j < result.evidence.size() - 1; ++j) {
      if (result.evidence[j].signalId > result.evidence[j + 1].signalId) {
        std::cout << "  [FAIL] Evidence is not sorted!" << std::endl;
        return false;
      }
    }

    std::string serialized = SerializeEvidence(result.evidence);
    if (i == 0) {
      firstOutput = serialized;
    } else {
      if (serialized != firstOutput) {
        std::cout << "  [FAIL] Output mismatch on run " << i << std::endl;
        return false;
      }
    }
  }
  std::cout << "  [PASS] Output is deterministic and sorted." << std::endl;
  return true;
}

bool TestLolbinContext() {
  std::cout << "[Test: LOLBin Context] Running..." << std::endl;
  ScoringConfig config = CreatePhase3Config();
  ScoringEngine engine(config);

  // Case 1: Legitimate PowerShell (System32, no suspicious args)
  {
    Entry entry(
        "Run", "User", "CleanPS", "powershell.exe", "",
        "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
    evidence::EntryEvidence evidence;
    evidence.imagePath =
        L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    evidence.authenticode.isSigned = true;
    evidence.authenticode.trustStatus = 0; // Signed

    ScoreResult result = engine.Evaluate(entry, evidence);
    bool lolbinTriggered = false;
    for (auto &e : result.evidence)
      if (e.signalId == "lolbin_autorun")
        lolbinTriggered = true;

    if (lolbinTriggered) {
      std::cout << "  [FAIL] Clean PowerShell in System32 triggered LOLBin!"
                << std::endl;
      return false;
    }
  }

  // Case 2: Suspicious Args (-enc)
  {
    Entry entry(
        "Run", "User", "EvilPS", "powershell.exe", "-enc BASE64...",
        "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
    evidence::EntryEvidence evidence;
    evidence.imagePath =
        L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";

    ScoreResult result = engine.Evaluate(entry, evidence);
    bool lolbinTriggered = false;
    for (auto &e : result.evidence)
      if (e.signalId == "lolbin_autorun")
        lolbinTriggered = true;

    if (!lolbinTriggered) {
      std::cout << "  [FAIL] PowerShell with -enc did NOT trigger LOLBin!"
                << std::endl;
      return false;
    }
  }

  // Case 3: Suspicious Location (Temp)
  {
    Entry entry("Run", "User", "TempPS", "powershell.exe", "",
                "C:\\Users\\User\\AppData\\Local\\Temp\\powershell.exe");
    evidence::EntryEvidence evidence;
    evidence.imagePath =
        L"C:\\Users\\User\\AppData\\Local\\Temp\\powershell.exe";

    ScoreResult result = engine.Evaluate(entry, evidence);
    bool lolbinTriggered = false;
    for (auto &e : result.evidence)
      if (e.signalId == "lolbin_autorun")
        lolbinTriggered = true;

    if (!lolbinTriggered) {
      std::cout << "  [FAIL] PowerShell in Temp did NOT trigger LOLBin!"
                << std::endl;
      return false;
    }
  }
  std::cout << "  [PASS] LOLBin context logic verified." << std::endl;
  return true;
}

bool TestScoreClamping() {
  std::cout << "[Test: Score Clamping] Running..." << std::endl;
  ScoringConfig config = CreatePhase3Config();
  ScoringEngine engine(config);

  // Entry that is signed/system32 (-50) but NOT a lolbin (no payload)
  // "notepad.exe" is not in our lolbins list for this config.
  // Wait, system32_legit_signed is -50.
  // If start score is 0, result should be 0.

  Entry entry("Run", "Machine", "Note", "notepad.exe", "",
              "C:\\Windows\\System32\\notepad.exe");
  evidence::EntryEvidence evidence;
  evidence.imagePath = L"C:\\Windows\\System32\\notepad.exe";
  evidence.authenticode.isSigned = true;
  evidence.authenticode.trustStatus = 0;
  evidence.authenticode.signerSubject = L"Microsoft Windows Publisher";

  ScoreResult result = engine.Evaluate(entry, evidence);

  // Expected raw score: -50.
  // Expected clamped score: 0.

  if (result.totalScore < 0) {
    std::cout << "  [FAIL] Score is negative: " << result.totalScore
              << std::endl;
    return false;
  }
  if (result.totalScore != 0) {
    std::cout << "  [FAIL] Score is not 0: " << result.totalScore << std::endl;
    return false;
  }

  std::cout << "  [PASS] Score was clamped to 0." << std::endl;
  return true;
}

int main() {
  bool passed = true;
  passed &= TestDeterminism();
  passed &= TestLolbinContext();
  passed &= TestScoreClamping();

  if (passed) {
    std::cout << "ALL PHASE 3 TESTS PASSED" << std::endl;
    return 0;
  } else {
    std::cout << "PHASE 3 TESTS FAILED" << std::endl;
    return 1;
  }
}
