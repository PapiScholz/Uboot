#pragma once

#include <string>
#include <vector>

namespace uboot {

/// <summary>
/// Parses command-line arguments for uboot-collector.
/// </summary>
class CliArgs {
public:
  std::vector<std::string> sources;  // --source flags (can be multiple)
  std::string outputPath;            // --out flag (optional, stdout if empty)
  bool prettyPrint = false;          // --pretty flag
  std::string schemaVersion = "1.0"; // --schema-version flag

  // Commands
  std::string command;    // "collect" (default), "actions"/"tx"
  std::string subCommand; // "list", "undo", "plan", "apply"
  std::string txId;       // --tx flag for undo/apply
  std::string reason;     // --reason for plan

  // Action flags
  std::vector<std::string> targetIds; // --ids
  std::vector<std::string> opSpecs;   // --op-spec "type|location|key|action" (repeatable)
  bool disableAction = false;         // --disable
  bool enableAction = false;          // --enable

  // Hardening flags
  bool forceCritical = false; // --force-critical
  bool bulkConfirm = false;   // --bulk-confirm
  bool safeMode = false;      // --safe-mode

  bool isValid = false;
  std::string errorMessage;

  /// <summary>
  /// Parse command-line arguments.
  /// </summary>
  /// <param name="argc">Argument count</param>
  /// <param name="argv">Argument values</param>
  /// <returns>Parsed arguments with isValid flag</returns>
  static CliArgs Parse(int argc, char *argv[]);

private:
  CliArgs() = default;
};

} // namespace uboot
