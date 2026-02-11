#include "CliArgs.h"
#include <algorithm>
#include <sstream>

namespace uboot {

CliArgs CliArgs::Parse(int argc, char* argv[]) {
    CliArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--source" || arg == "-s") {
            if (i + 1 < argc) {
                args.sources.push_back(argv[++i]);
            } else {
                args.errorMessage = "--source requires a value";
                return args;
            }
        }
        else if (arg == "--out" || arg == "-o") {
            if (i + 1 < argc) {
                args.outputPath = argv[++i];
            } else {
                args.errorMessage = "--out requires a value";
                return args;
            }
        }
        else if (arg == "--pretty" || arg == "-p") {
            args.prettyPrint = true;
        }
        else if (arg == "--schema-version") {
            if (i + 1 < argc) {
                args.schemaVersion = argv[++i];
            } else {
                args.errorMessage = "--schema-version requires a value";
                return args;
            }
        }
        else if (arg == "--help" || arg == "-h") {
            args.errorMessage = "USAGE";  // Special marker for help
            return args;
        }
        else {
            args.errorMessage = "Unknown argument: " + arg;
            return args;
        }
    }
    
    // Validate: at least one source is required
    if (args.sources.empty()) {
        // Default to all sources if none specified
        args.sources = { "all" };
    }
    
    args.isValid = true;
    return args;
}

} // namespace uboot
