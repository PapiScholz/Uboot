#include "CommandResolver.h"
#include <algorithm>

namespace uboot {

bool CommandResolver::IsLnkFile(const std::string& path) {
    if (path.length() < 4) {
        return false;
    }
    
    // Check for .lnk extension (case-insensitive)
    std::string extension = path.substr(path.length() - 4);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    return extension == ".lnk";
}

NormalizationResult CommandResolver::ResolveCommand(
    const std::string& commandOrPath,
    const std::string& workingDir
) {
    NormalizationResult result;
    
    if (commandOrPath.empty()) {
        result.originalCommand = commandOrPath;
        result.isComplete = false;
        result.resolveNotes = "Empty command or path";
        return result;
    }
    
    // Check if it's a .lnk file
    if (IsLnkFile(commandOrPath)) {
        LnkResolutionResult lnkResult = LnkResolver::Resolve(commandOrPath);
        
        if (lnkResult.isResolved) {
            // We got a target from the .lnk - now normalize it
            std::string commandToNormalize = lnkResult.targetPath;
            if (!lnkResult.arguments.empty()) {
                commandToNormalize += " " + lnkResult.arguments;
            }
            
            // Use working directory from .lnk if available
            std::string workDir = lnkResult.workingDirectory.empty() ? workingDir : lnkResult.workingDirectory;
            
            result = CommandNormalizer::Normalize(commandToNormalize, workDir);
            result.originalCommand = commandOrPath;
            result.resolveNotes = "Resolved .lnk file. " + result.resolveNotes;
            
            return result;
        } else {
            // .lnk resolution failed
            result.originalCommand = commandOrPath;
            result.isComplete = false;
            result.resolveNotes = "Failed to resolve .lnk file: " + lnkResult.resolveNotes;
            return result;
        }
    }
    
    // Not a .lnk file - normalize directly
    result = CommandNormalizer::Normalize(commandOrPath, workingDir);
    return result;
}

void CommandResolver::PopulateEntryCommand(
    Entry& entry,
    const std::string& rawCommand,
    const std::string& workingDir
) {
    NormalizationResult result = ResolveCommand(rawCommand, workingDir);
    
    entry.imagePath = result.resolvedPath;
    entry.arguments = result.arguments;
    
    // Preserve original command in location or key if imagePath is empty
    if (entry.imagePath.empty()) {
        if (entry.location.empty()) {
            entry.location = result.originalCommand;
        }
    }
}

} // namespace uboot
