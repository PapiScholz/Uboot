#include "CliArgs.h"
#include <algorithm>
#include <sstream>

namespace uboot {

CliArgs CliArgs::Parse(int argc, char *argv[]) {
  CliArgs args;

  // Default mode is collection
  args.command = "collect";

  int argIdx = 1;
  if (argc > 1) {
    std::string firstArg = argv[1];
    if (firstArg == "actions") {
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
    } else if (arg == "--tx") {
      if (i + 1 < argc) {
        args.txId = argv[++i];
      } else {
        args.errorMessage = "--tx requires a value";
        return args;
      }
    } else if (arg == "--ids") {
      if (i + 1 < argc) {
        std::string ids = argv[++i];
        std::stringstream ss(ids);
        std::string item;
        while (std::getline(ss, item, ',')) {
          args.targetIds.push_back(item);
        }
      } else {
        args.errorMessage = "--ids requires a value (comma-separated)";
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
      args.errorMessage = "undo action requires --tx <id>";
      return args;
    }
  }

  args.isValid = true;
  return args;
}

} // namespace uboot
