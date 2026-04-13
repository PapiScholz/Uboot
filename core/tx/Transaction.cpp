#include "Transaction.h"
#include "../util/GlobalLock.h"
#include "../hardening/SecurityLog.h"
#include "../ops/OperationFactory.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <share.h> // For _fsopen
#include <sstream>

// Helper to generate a short ID
static std::string generateShortId() {
  static const char chars[] = "0123456789abcdef";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  std::string id;
  for (int i = 0; i < 8; ++i) {
    id += chars[dis(gen)];
  }
  return id;
}

Transaction::Transaction(BackupStore &store)
    : backupStore(store), state(TxState::Created) {
  txId = generateShortId();
}

Transaction::~Transaction() {
  std::lock_guard<std::mutex> lock(txMutex); // Protect destructor?
  if (journalFile) {
    fclose(journalFile);
    journalFile = nullptr;
  }
}

void Transaction::addOperation(std::unique_ptr<IOperation> op) {
  std::lock_guard<std::mutex> lock(txMutex);
  operations.push_back(std::move(op));
}

std::string Transaction::getId() const { return txId; }

TxState Transaction::getState() const {
  std::lock_guard<std::mutex> lock(txMutex);
  return state;
}

void Transaction::initJournal() {
  // Open journal with exclusive access
  if (journalFile)
    return;

  if (txPath.empty())
    return;

  std::filesystem::path journalPath =
      std::filesystem::path(txPath) / "journal.ndjson";

  // Open for appending, deny read/write to others
  journalFile = _fsopen(journalPath.string().c_str(), "a", _SH_DENYRW);

  if (!journalFile) {
    // Prepare might fail later if journal is critical, but we can't do much
    // here without changing signature.
    std::cerr << "Warning: Failed to open journal file exclusively.\n";
  }
}

void Transaction::setConfig(Config cfg) {
  std::lock_guard<std::mutex> lock(txMutex);
  config = cfg;
}

bool Transaction::loadFromManifest(const std::string &path) {
  std::lock_guard<std::mutex> lock(txMutex);

  // We use _fsopen to read, but we should allow others to read (e.g. they might
  // just be listing). But if we are loading to run, we might want to lock it?
  // Requirement says: "Mientras una TX esté activa: nadie puede escribir".
  // Reading is fine.

  std::ifstream ifs(path);
  if (!ifs.is_open())
    return false;

  std::stringstream buffer;
  buffer << ifs.rdbuf();
  std::string content = buffer.str();

  rapidjson::Document d;
  d.Parse(content.c_str());

  if (d.HasParseError())
    return false;

  if (d.HasMember("txid") && d["txid"].IsString()) {
    txId = d["txid"].GetString();
  }

  if (d.HasMember("operations") && d["operations"].IsArray()) {
    const auto &opsArray = d["operations"];
    operations.clear();
    for (const auto &opVal : opsArray.GetArray()) {
      auto op = uboot::ops::OperationFactory::CreatefromJson(opVal);
      if (op) {
        // Resolve txPath
        txPath = backupStore.getTransactionDirectory(txId);
        if (txPath.empty())
          txPath = backupStore.createTransactionDirectory(txId);

        op->prepare(txPath);

        operations.push_back(std::move(op));
      }
    }
  }

  // writeManifest(); // Don't write manifest on load, unless we change state.
  return true;
}

bool Transaction::plan() {
  std::lock_guard<std::mutex> lock(txMutex);

  // 1. Initialize Storage
  txPath = backupStore.createTransactionDirectory(txId);
  if (txPath.empty()) {
    std::cerr << "Failed to create transaction directory." << std::endl;
    state = TxState::Failed;
    return false;
  }

  // Open journal
  initJournal();

  appendJournal("INIT", "TX", "STARTED",
                "Transaction initialized for planning");

  // 2. Prepare
  bool success = prepare();
  if (!success) {
    // Even if failed, we write manifest so user sees errors
    state = TxState::Failed;
    appendJournal("PREPARE", "TX", "FAILED", "Plan failed hardening checks");
  } else {
    appendJournal("PREPARE", "TX", "SUCCESS", "Plan ready");
  }

  writeManifest();
  return success;
}

bool Transaction::runAll() {
  std::lock_guard<std::mutex> lock(txMutex);

  // GLOBAL LOCK: Ensure single instance
  uboot::util::GlobalLock globalLock;
  if (!globalLock.Acquire()) {
    std::cerr << "Error: Another transaction is currently in progress."
              << std::endl;
    return false;
  }

  // 1. Initialize Storage
  txPath = backupStore.createTransactionDirectory(txId);
  if (txPath.empty()) {
    std::cerr << "Failed to create transaction directory." << std::endl;
    state = TxState::Failed;
    return false;
  }

  // Open journal
  initJournal();

  appendJournal("INIT", "TX", "STARTED", "Transaction initialized");

  // 2. Prepare
  if (!prepare()) {
    state = TxState::Failed;
    appendJournal("PREPARE", "TX", "FAILED", "Prepare phase failed");
    writeManifest();
    return false;
  }

  // 3. Apply
  if (!apply()) {
    state = TxState::RolledBack;
    appendJournal("APPLY", "TX", "FAILED", "Apply phase failed, rolled back");
    writeManifest();
    return false;
  }

  // 4. Verify
  if (!verify()) {
    appendJournal("VERIFY", "TX", "FAILED",
                  "Verification failed, rolling back");
    rollback(operations.size() - 1);
    state = TxState::RolledBack;
    writeManifest();
    return false;
  }

  state = TxState::Committed;
  appendJournal("COMMIT", "TX", "SUCCESS", "Transaction committed");
  writeManifest();
  return true;
}

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#endif

// Helper for Admin check
static bool isElevated() {
#ifdef _WIN32
  BOOL fRet = FALSE;
  HANDLE hToken = NULL;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
    TOKEN_ELEVATION Elevation;
    DWORD cbSize = sizeof(TOKEN_ELEVATION);
    if (GetTokenInformation(hToken, TokenElevation, &Elevation,
                            sizeof(Elevation), &cbSize)) {
      fRet = Elevation.TokenIsElevated;
    }
  }
  if (hToken) {
    CloseHandle(hToken);
  }
  return fRet;
#else
  return true; // Assume true on non-Windows for now or implement geteuid
#endif
}

bool Transaction::prepare() {
  // Assuming lock held by caller

  if (!isElevated()) {
    std::cerr << "ERROR: administrative privileges required." << std::endl;
    appendJournal("PREPARE", "TX", "BLOCKED", "Admin privileges missing");
    return false;
  }

  // Bulk check
  if (operations.size() > 10 && !config.bulkConfirm) {
    uboot::hardening::SecurityLog::Instance().Log(
        "WARNING", "Bulk operation blocked without confirmation",
        "Count: " + std::to_string(operations.size()));
    std::cerr << "ERROR: Bulk operation detected (" << operations.size()
              << " items). Use --bulk-confirm to proceed.\n";
    return false;
  }

  state = TxState::Prepared;
  appendJournal("PREPARE", "TX", "START");

  for (size_t i = 0; i < operations.size(); ++i) {
    if (!operations[i]->prepare(txPath)) {
      appendJournal("PREPARE", operations[i]->getId(), "FAILED");
      return false;
    }
    appendJournal("PREPARE", operations[i]->getId(), "SUCCESS");
  }
  return true;
}

bool Transaction::apply() {
  // Assuming lock held by caller

  if (!isElevated()) {
    std::cerr << "ERROR: Administrative privileges required." << std::endl;
    appendJournal("APPLY", "TX", "BLOCKED", "Admin privileges missing");
    return false;
  }

  state = TxState::Applied;
  appendJournal("APPLY", "TX", "START");

  for (size_t i = 0; i < operations.size(); ++i) {
    if (!operations[i]->apply()) {
      appendJournal("APPLY", operations[i]->getId(), "FAILED");
      // Rollback previous operations
      if (i > 0) {
        rollback(i - 1);
      }
      return false;
    }
    appendJournal("APPLY", operations[i]->getId(), "SUCCESS");
  }
  return true;
}

bool Transaction::verify() {
  // Assuming lock held by caller
  state = TxState::Verified;
  appendJournal("VERIFY", "TX", "START");

  for (size_t i = 0; i < operations.size(); ++i) {
    if (!operations[i]->verify()) {
      appendJournal("VERIFY", operations[i]->getId(), "FAILED");
      return false;
    }
    appendJournal("VERIFY", operations[i]->getId(), "SUCCESS");
  }
  return true;
}

void Transaction::rollback(int fromIndex) {
  // Assuming lock held by caller
  appendJournal("ROLLBACK", "TX", "START", "Rolling back...");

  // Reverse order
  for (int i = fromIndex; i >= 0; --i) {
    bool success = operations[i]->rollback(txPath);
    appendJournal("ROLLBACK", operations[i]->getId(),
                  success ? "SUCCESS" : "FAILED");
  }

  state = TxState::RolledBack;
}

bool Transaction::undoAll() {
  std::lock_guard<std::mutex> lock(txMutex);

  // GLOBAL LOCK: Ensure single instance
  uboot::util::GlobalLock globalLock;
  if (!globalLock.Acquire()) {
    std::cerr << "Error: Another transaction is currently in progress."
              << std::endl;
    return false;
  }

  if (operations.empty())
    return true;

  // Ensure journal is open
  initJournal();

  rollback(operations.size() - 1);
  writeManifest();
  return state == TxState::RolledBack;
}

void Transaction::appendJournal(const std::string &phase,
                                const std::string &opId,
                                const std::string &result,
                                const std::string &details) {
  // Assuming lock held by caller

  // Open if not already open (e.g. if loaded from manifest for undo,
  // initJournal might not have been called yet if strict)
  if (!journalFile) {
    initJournal();
  }

  if (!journalFile)
    return;

  rapidjson::Document d;
  d.SetObject();

  auto &alloc = d.GetAllocator();

  // Timestamp
  auto now = std::chrono::system_clock::now();
  uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch())
                    .count();
  d.AddMember("ts", ms, alloc);

  rapidjson::Value vPhase;
  vPhase.SetString(phase.c_str(), alloc);
  d.AddMember("phase", vPhase, alloc);

  rapidjson::Value vOpId;
  vOpId.SetString(opId.c_str(), alloc);
  d.AddMember("op_id", vOpId, alloc);

  rapidjson::Value vResult;
  vResult.SetString(result.c_str(), alloc);
  d.AddMember("result", vResult, alloc);

  if (!details.empty()) {
    rapidjson::Value vDetails;
    vDetails.SetString(details.c_str(), alloc);
    d.AddMember("details", vDetails, alloc);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d.Accept(writer);

  // Write with newline
  fprintf(journalFile, "%s\n", buffer.GetString());
  fflush(journalFile);
}

std::string Transaction::stateToString(TxState s) const {
  switch (s) {
  case TxState::Created:
    return "Created";
  case TxState::Prepared:
    return "Prepared";
  case TxState::Applied:
    return "Applied";
  case TxState::Verified:
    return "Verified";
  case TxState::Committed:
    return "Committed";
  case TxState::RolledBack:
    return "RolledBack";
  case TxState::Failed:
    return "Failed";
  default:
    return "Unknown";
  }
}

void Transaction::writeManifest() {
  // Assuming lock held by caller
  if (txPath.empty())
    return;

  rapidjson::Document d;
  d.SetObject();
  auto &alloc = d.GetAllocator();

  d.AddMember("schema_version", "uboot.tx.v1", alloc);

  rapidjson::Value vTxId;
  vTxId.SetString(txId.c_str(), alloc);
  d.AddMember("txid", vTxId, alloc);

  std::string sState = stateToString(state);
  rapidjson::Value vState;
  vState.SetString(sState.c_str(), alloc);
  d.AddMember("state", vState, alloc);

  // Add machine/user/app_version placeholders
  d.AddMember("machine", "unknown", alloc);
  d.AddMember("user", "unknown", alloc);
  d.AddMember("app_version", "1.0.0", alloc);

  // Operations
  rapidjson::Value opsArray(rapidjson::kArrayType);
  for (const auto &op : operations) {
    rapidjson::Value opObj(rapidjson::kObjectType);
    op->toJson(opObj, alloc);
    opsArray.PushBack(opObj, alloc);
  }
  d.AddMember("operations", opsArray, alloc);

  // Write to file with exclusive locking
  std::filesystem::path manifestPath =
      std::filesystem::path(txPath) / "manifest.json";

  // Open exclusively for writing
  FILE *fp = _fsopen(manifestPath.string().c_str(), "w", _SH_DENYRW);
  if (fp) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    fputs(buffer.GetString(), fp);
    fclose(fp);
  } else {
    std::cerr << "Error: Failed to open manifest for writing (Pending Lock?)\n";
  }
}
