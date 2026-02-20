#pragma once

#include "../backup/BackupStore.h"
#include "IOperation.h"
#include "rapidjson/document.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>


enum class TxState {
  Created,
  Prepared,
  Applied,
  Verified,
  Committed,
  RolledBack,
  Failed
};

class Transaction {
public:
  Transaction(BackupStore &store);
  ~Transaction();

  // Add an operation to the transaction
  void addOperation(std::unique_ptr<IOperation> op);

  // Execute the transaction flow: Prepare -> Apply -> Verify
  bool runAll();

  // Run only the preparation phase (Hardening checks) and write manifest
  bool plan();

  // Undo the transaction (Reverse order rollback)
  bool undoAll();

  // Get the Transaction ID
  std::string getId() const;

  // Load transaction from manifest file
  bool loadFromManifest(const std::string &path);

  // Configuration options
  struct Config {
    bool forceCritical = false;
    bool bulkConfirm = false;
    bool safeMode = false;
  };
  void setConfig(Config config);

  // Get current state
  TxState getState() const;

private:
  Config config;
  std::string txId;
  TxState state;
  std::vector<std::unique_ptr<IOperation>> operations;
  BackupStore &backupStore;
  std::string txPath; // Path where backups and journals are stored

  mutable std::mutex txMutex;
  FILE *journalFile = nullptr;

  // Internal steps
  bool prepare();
  bool apply();
  bool verify();
  void
  rollback(int fromIndex); // Rollback operations up to fromIndex (inclusive)

  // Logging & Persistence
  void initJournal();
  void appendJournal(const std::string &phase, const std::string &opId,
                     const std::string &result,
                     const std::string &details = "");
  void writeManifest();

  // Helper to stringify state
  std::string stateToString(TxState s) const;
};
