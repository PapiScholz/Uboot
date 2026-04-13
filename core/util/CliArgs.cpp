#include "CliArgs.h"
#include <algorithm>
#include <sstream>

namespace uboot {

CliArgs CliArgs::Parse(int argc, char *argv[]) {
  CliArgs args;

  // Default mode is collection
  args.command = "collect";
  args.reason = "User-initiated remediation";

  int argIdx = 1;
  if (argc > 1) {
    std::string firstArg = argv[1];
    if (firstArg == "actions" || firstArg == "tx") {
      args.command = "actions";
      argIdx++; // Consumed 'actions'

      if (argIdx < argc) {
        args.subCommand = argv[argIdx++];

        if (args.subCommand != "list" && args.subCommand != "undo" &&
            args.subCommand != "plan" && args.subCommand != "apply") {
          args.errorMessage = "Unknown action: " + args.subCommand +
                              ". Valid actions: list, undo, plan, apply";
          return args;
        }
      } else {
        args.errorMessage =
            "actions command requires a subcommand (list, undo, plan, apply)";
        return args;
      }
    }
  }

  for (int i = argIdx; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--source" || arg == "-s") {
      if (i + 1 < argc) {
        args.sources.push_back(argv[++i]);
      } else {
        args.errorMessage = "--source requires a value";
        return args;
      }
    } else if (arg == "--out" || arg == "-o") {
      if (i + 1 < argc) {
        args.outputPath = argv[++i];
      } else {
        args.errorMessage = "--out requires a value";
        return args;
      }
    } else if (arg == "--pretty" || arg == "-p") {
      args.prettyPrint = true;
    } else if (arg == "--schema-version") {
      if (i + 1 < argc) {
        args.schemaVersion = argv[++i];
      } else {
        args.errorMessage = "--schema-version requires a value";
        return args;
      }
    } else if (arg == "--tx" || arg == "--tx-id") {
      if (i + 1 < argc) {
        args.txId = argv[++i];
      } else {
        args.errorMessage = "--tx/--tx-id requires a value";
        return args;
      }
    } else if (arg == "--ids" || arg == "--entry-ids") {
      if (i + 1 < argc) {
        std::string ids = argv[++i];
        std::stringstream ss(ids);
        std::string item;
        while (std::getline(ss, item, ',')) {
          if (!item.empty()) {
            args.targetIds.push_back(item);
          }
        }
      } else {
        args.errorMessage =
            "--ids/--entry-ids requires a value (comma-separated)";
        return args;
      }
    } else if (arg == "--entry-id") {
      if (i + 1 < argc) {
        std::string entryId = argv[++i];
        if (!entryId.empty()) {
          args.targetIds.push_back(entryId);
        }
      } else {
        args.errorMessage = "--entry-id requires a value";
        return args;
      }
    } else if (arg == "--reason") {
      if (i + 1 < argc) {
        args.reason = argv[++i];
      } else {
        args.errorMessage = "--reason requires a value";
        return args;
      }
    } else if (arg == "--disable") {
      args.disableAction = true;
    } else if (arg == "--enable") {
      args.enableAction = true;
    } else if (arg == "--force-critical") {
      args.forceCritical = true;
    } else if (arg == "--bulk-confirm") {
      args.bulkConfirm = true;
    } else if (arg == "--safe-mode") {
      args.safeMode = true;
    } else if (arg == "--backup-dir") {
      if (i + 1 < argc)
        i++;
    } else if (arg == "--help" || arg == "-h") {
      args.errorMessage = "USAGE";
      return args;
    } else {
      args.errorMessage = "Unknown argument: " + arg;
      return args;
    }
  }

  // Validation
  if (args.command == "collect") {
    if (args.sources.empty()) {
      args.sources = {"all"};
    }
  } else if (args.command == "actions") {
    if (args.subCommand == "undo" && args.txId.empty()) {
      args.errorMessage = "undo action requires --tx-id <id>";
      return args;
    }
    if (args.subCommand == "apply" && args.txId.empty()) {
      args.errorMessage = "apply action requires --tx-id <id>";
      return args;
    }
    if (args.subCommand == "plan" && args.targetIds.empty()) {
      args.errorMessage =
          "plan action requires --entry-id <id> or --entry-ids <id1,id2,...>";
      return args;
    }
  }

  args.isValid = true;
  return args;
}

} // namespace uboot
