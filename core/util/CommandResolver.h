#pragma once

#include "../normalize/CommandNormalizer.h"
#include "../resolve/LnkResolver.h"
#include "../model/Entry.h"
#include <string>
#include <optional>

namespace uboot {

/// <summary>
/// Utility for complete command resolution pipeline.
/// Combines CommandNormalizer and LnkResolver for comprehensive command extraction.
/// </summary>
class CommandResolver {
public:
    /// <summary>
    /// Resolve a command, including .lnk file resolution and normalization.
    /// Handles both direct commands and shortcut files.
    /// </summary>
    /// <param name="commandOrPath">Raw command or path to .lnk file</param>
    /// <param name="workingDir">Optional working directory</param>
    /// <returns>Resolved command with executable and arguments</returns>
    static NormalizationResult ResolveCommand(
        const std::string& commandOrPath,
        const std::string& workingDir = ""
    );
    
    /// <summary>
    /// Populate an Entry with resolved command information.
    /// </summary>
    static void PopulateEntryCommand(
        Entry& entry,
        const std::string& rawCommand,
        const std::string& workingDir = ""
    );

private:
    static bool IsLnkFile(const std::string& path);
};

} // namespace uboot
