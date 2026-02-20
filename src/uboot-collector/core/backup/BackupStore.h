#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct TransactionInfo {
  std::string id;
  std::string timestamp; // String format for display
  std::string path;
  bool isPinned;
};

class BackupStore {
public:
  explicit BackupStore(
      const std::optional<std::string> &overridePath = std::nullopt);

  // Initializes the backup store (creates directories if needed)
  bool init();

  // Creates a new transaction directory: tx-<UTC>-<shortid>
  // Returns the full path to the created directory
  std::string createTransactionDirectory(const std::string &txId);

  // Get existing transaction directory by ID
  std::string getTransactionDirectory(const std::string &txId);

  // Enforces retention policy: deletes backups older than 30 days
  // unless they are marked as pinned.
  void enforceRetentionPolicy();

  // List all transactions found in the store
  std::vector<TransactionInfo> listTransactions();

  // Returns the root backup directory
  std::string getRootDirectory() const;

private:
  fs::path rootPath;

  // Helper to get the default ProgramData path
  fs::path getDefaultPath();

  // Check if a backup is pinned (e.g., contains a .pinned file)
  bool isPinned(const fs::path &txPath);

  // Helper to parse timestamp from folder name
  // Folder format: tx-<timestamp>-<shortid>
  // We rely on filesystem creation time or name parsing?
  // Name parsing is safer for portability.
  std::optional<std::chrono::system_clock::time_point>
  getTxTime(const std::string &folderName);
};
