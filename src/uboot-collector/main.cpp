#include "core/backup/BackupStore.h"
#include "core/ops/RegistryRenameValueOp.h"
#include "core/ops/ServiceStartTypeOp.h"
#include "core/ops/TaskToggleEnabledOp.h"
#include "core/tx/Transaction.h"
#include "runner/CollectorRunner.h"
#include "util/CliArgs.h"
#include "json/JsonWriter.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace uboot;

// Exit codes
constexpr int EXIT_OK = 0;
constexpr int EXIT_INVALID_ARGS = 2;
constexpr int EXIT_FATAL_INIT = 3;
constexpr int EXIT_NO_SOURCES = 4;

void PrintUsage() {
  std::cout << "uboot-collector v1.0 - Windows Boot Entry Collector\n\n";
  std::cout << "USAGE:\n";
  std::cout << "  uboot-collector [options]\n\n";
  std::cout << "OPTIONS:\n";
  std::cout << "  --source, -s <name>      Collector source to run (can be "
               "specified multiple times)\n";
  std::cout << "                           Available: RunRegistry, Services, "
               "StartupFolder,\n";
  std::cout << "                                      ScheduledTasks, "
               "WmiPersistence,\n";
  std::cout << "                                      Winlogon, IfeoDebugger\n";
  std::cout << "                           Default: all\n";
  std::cout
      << "  --out, -o <path>         Output file path (default: stdout)\n";
  std::cout << "  --pretty, -p             Pretty-print JSON output\n";
  std::cout
      << "  --schema-version <ver>   Schema version string (default: 1.0)\n";
  std::cout << "  --help, -h               Show this help message\n\n";
  std::cout << "EXAMPLES:\n";
  std::cout << "  uboot-collector --source RunRegistry --out snapshot.json\n";
  std::cout << "  uboot-collector --source all --pretty\n";
  std::cout
      << "  uboot-collector -s Services -s StartupFolder -o output.json\n\n";
  std::cout << "EXIT CODES:\n";
  std::cout << "  0 - Success\n";
  std::cout << "  2 - Invalid arguments\n";
  std::cout << "  3 - Fatal initialization error\n";
  std::cout << "  4 - No sources executed successfully\n";
}

int main(int argc, char *argv[]) {
  // Parse command-line arguments
  CliArgs args = CliArgs::Parse(argc, argv);

  if (!args.isValid) {
    if (args.errorMessage == "USAGE") {
      PrintUsage();
      return EXIT_OK;
    }
    std::cerr << "Error: " << args.errorMessage << "\n\n";
    PrintUsage();
    return EXIT_INVALID_ARGS;
  }

  // Handle Actions Mode
  if (args.command == "actions") {
    BackupStore store;
    store.init();

    if (args.subCommand == "list") {
      auto txs = store.listTransactions();
      if (txs.empty()) {
        std::cout << "No transactions found.\n";
      } else {
        std::cout << "Transactions:\n";
        std::cout
            << "---------------------------------------------------------\n";
        for (const auto &tx : txs) {
          std::cout << tx.timestamp << "  " << tx.id;
          if (tx.isPinned)
            std::cout << " [PINNED]";
          std::cout << "\n";
        }
      }
    } else if (args.subCommand == "undo") {
      // MVP: Undo not fully implemented with disk loading yet.
      std::cout << "Undo requested for TX: " << args.txId << "\n";
      // To implement undo: load tx, call undoAll()
      Transaction tx(store);
      std::string path = store.getTransactionDirectory(args.txId);
      if (path.empty()) {
        std::cerr << "Transaction not found: " << args.txId << "\n";
        return EXIT_INVALID_ARGS;
      }

      std::string manifestPath =
          fs::path(path).append("manifest.json").string();
      if (!tx.loadFromManifest(manifestPath)) {
        std::cerr << "Failed to load manifest from " << manifestPath << "\n";
        return EXIT_FATAL_INIT;
      }

      std::cout << "Undoing transaction " << tx.getId() << "...\n";
      if (tx.undoAll()) {
        std::cout << "Undo successful.\n";
      } else {
        std::cerr << "Undo failed. Check logs.\n";
        return EXIT_FATAL_INIT;
      }
    } else if (args.subCommand == "apply") {
      if (args.txId.empty()) {
        std::cerr << "--tx <id> required for apply.\n";
        return EXIT_INVALID_ARGS;
      }

      Transaction tx(store);
      std::string path = store.getTransactionDirectory(args.txId);
      if (path.empty()) {
        std::cerr << "Transaction not found: " << args.txId << "\n";
        return EXIT_INVALID_ARGS;
      }

      std::string manifestPath =
          fs::path(path).append("manifest.json").string();
      if (!tx.loadFromManifest(manifestPath)) {
        std::cerr << "Failed to load manifest from " << manifestPath << "\n";
        return EXIT_FATAL_INIT;
      }

      std::cout << "Applying transaction " << tx.getId() << "...\n";
      if (tx.runAll()) { // runAll does prepare->apply->verify.
        // Since it's already prepared (presumably), runAll re-runs prepare.
        // This is good for re-verification.
        std::cout << "Transaction applied successfully.\n";
      } else {
        std::cerr << "Transaction failed. State: " << (int)tx.getState()
                  << "\n";
        return EXIT_FATAL_INIT;
      }
    } else if (args.subCommand == "plan") {
      if (args.targetIds.empty()) {
        std::cerr << "--ids <id1,id2...> required for plan.\n";
        return EXIT_INVALID_ARGS;
      }
      if (!args.disableAction && !args.enableAction) {
        std::cerr << "Action required: --disable or --enable.\n";
        return EXIT_INVALID_ARGS;
      }

      // 1. Run Collectors to resolve IDs
      std::cout << "Scanning system to resolve IDs...\n";
      CollectorRunner runner;
      std::vector<Entry> entries;
      std::vector<CollectorError> cErrors;
      runner.Run({"all"}, entries, cErrors);

      // 2. Create Transaction
      Transaction tx(store);

      // Configure hardening
      Transaction::Config cfg;
      cfg.forceCritical = args.forceCritical;
      cfg.bulkConfirm = args.bulkConfirm;
      cfg.safeMode = args.safeMode;
      tx.setConfig(cfg);

      int addedCount = 0;
      for (const auto &id : args.targetIds) {
        // Find entry with key == id
        // Or maybe partial match? strict for now.
        auto it = std::find_if(entries.begin(), entries.end(),
                               [&](const Entry &e) { return e.key == id; });

        if (it != entries.end()) {
          const auto &entry = *it;
          if (entry.source == "RunRegistry" ||
              entry.source == "StartupFolder") {
            // StartupFolder might use file deletion? Or RegistryRename logic if
            // it's registry based? StartupFolderCollector gathers files in
            // Startup. We don't have FileMoveOp yet. Only
            // RegistryRenameValueOp.
            if (entry.source == "RunRegistry") {
              uboot::ops::RegistryRenameValueOp::Action action =
                  args.disableAction
                      ? uboot::ops::RegistryRenameValueOp::Action::Disable
                      : uboot::ops::RegistryRenameValueOp::Action::Enable;

              // entry.key is value name. entry.location is Key Path?
              // RunRegistryCollector: location = key path, key = value name.
              tx.addOperation(
                  std::make_unique<uboot::ops::RegistryRenameValueOp>(
                      entry.location, entry.key, action));
              addedCount++;
            }
          } else if (entry.source == "Services") {
            uboot::ops::ServiceStartTypeOp::Action action =
                args.disableAction
                    ? uboot::ops::ServiceStartTypeOp::Action::Disable
                    : uboot::ops::ServiceStartTypeOp::Action::Enable;
            tx.addOperation(std::make_unique<uboot::ops::ServiceStartTypeOp>(
                entry.key, action));
            addedCount++;
          } else if (entry.source == "ScheduledTasks") {
            uboot::ops::TaskToggleEnabledOp::Action action =
                args.disableAction
                    ? uboot::ops::TaskToggleEnabledOp::Action::Disable
                    : uboot::ops::TaskToggleEnabledOp::Action::Enable;
            // entry.location is task path? or entry.key?
            // ScheduledTasksCollector: key is Path (e.g. \MyTask).
            tx.addOperation(std::make_unique<uboot::ops::TaskToggleEnabledOp>(
                entry.key, action));
            addedCount++;
          }
        } else {
          std::cerr << "Warning: ID not found: " << id << "\n";
        }
      }

      if (addedCount == 0) {
        std::cerr << "No valid operations generated from IDs.\n";
        return EXIT_NO_SOURCES;
      }

      std::cout << "Generated " << addedCount
                << " operations. Running Pre-check (Prepare)...\n";

      if (tx.plan()) {
        std::cout << "Plan generated successfully. Manifest ID: " << tx.getId()
                  << "\n";
        std::cout << "Review warnings in manifest or logs before applying.\n";
        std::cout << "To apply: uboot-collector actions apply --tx "
                  << tx.getId() << "\n";
      } else {
        std::cerr << "Plan failed hardening checks. See logs for details. "
                     "Transaction ID: "
                  << tx.getId() << "\n";
        std::cerr << "Use --force-critical if blocking critical services.\n";
        return EXIT_FATAL_INIT;
      }
    }

    return EXIT_OK;
  }

  // Initialize collector runner
  CollectorRunner runner;

  std::vector<Entry> entries;
  std::vector<CollectorError> errors;

  // Run collectors
  int successCount = 0;
  try {
    successCount = runner.Run(args.sources, entries, errors);
  } catch (const std::exception &e) {
    std::cerr << "Fatal error during collection: " << e.what() << std::endl;
    return EXIT_FATAL_INIT;
  } catch (...) {
    std::cerr << "Unknown fatal error during collection" << std::endl;
    return EXIT_FATAL_INIT;
  }

  // Check if any sources executed
  if (successCount == 0 && entries.empty()) {
    std::cerr << "Error: No collectors executed successfully\n";

    // Print errors to stderr
    for (const auto &error : errors) {
      std::cerr << "  [" << error.source << "] " << error.message;
      if (error.errorCode != 0) {
        std::cerr << " (code: " << error.errorCode << ")";
      }
      std::cerr << "\n";
    }

    return EXIT_NO_SOURCES;
  }

  // Write output
  bool writeSuccess = JsonWriter::Write(args.outputPath, entries, errors,
                                        args.prettyPrint, args.schemaVersion);

  if (!writeSuccess) {
    std::cerr << "Error: Failed to write output to " << args.outputPath
              << std::endl;
    return EXIT_FATAL_INIT;
  }

  // Print summary to stderr if writing to file
  if (!args.outputPath.empty()) {
    std::cerr << "Collected " << entries.size() << " entries from "
              << successCount << " sources";
    if (!errors.empty()) {
      std::cerr << " (" << errors.size() << " errors)";
    }
    std::cerr << std::endl;
  }

  return EXIT_OK;
}
