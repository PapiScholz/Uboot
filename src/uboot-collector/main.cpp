#include "util/CliArgs.h"
#include "runner/CollectorRunner.h"
#include "json/JsonWriter.h"
#include <iostream>
#include <string>
#include <cstdlib>

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
    std::cout << "  --source, -s <name>      Collector source to run (can be specified multiple times)\n";
    std::cout << "                           Available: RunRegistry, Services, StartupFolder,\n";
    std::cout << "                                      ScheduledTasks, WmiPersistence,\n";
    std::cout << "                                      Winlogon, IfeoDebugger\n";
    std::cout << "                           Default: all\n";
    std::cout << "  --out, -o <path>         Output file path (default: stdout)\n";
    std::cout << "  --pretty, -p             Pretty-print JSON output\n";
    std::cout << "  --schema-version <ver>   Schema version string (default: 1.0)\n";
    std::cout << "  --help, -h               Show this help message\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "  uboot-collector --source RunRegistry --out snapshot.json\n";
    std::cout << "  uboot-collector --source all --pretty\n";
    std::cout << "  uboot-collector -s Services -s StartupFolder -o output.json\n\n";
    std::cout << "EXIT CODES:\n";
    std::cout << "  0 - Success\n";
    std::cout << "  2 - Invalid arguments\n";
    std::cout << "  3 - Fatal initialization error\n";
    std::cout << "  4 - No sources executed successfully\n";
}

int main(int argc, char* argv[]) {
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
    
    // Initialize collector runner
    CollectorRunner runner;
    
    std::vector<Entry> entries;
    std::vector<CollectorError> errors;
    
    // Run collectors
    int successCount = 0;
    try {
        successCount = runner.Run(args.sources, entries, errors);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error during collection: " << e.what() << std::endl;
        return EXIT_FATAL_INIT;
    }
    catch (...) {
        std::cerr << "Unknown fatal error during collection" << std::endl;
        return EXIT_FATAL_INIT;
    }
    
    // Check if any sources executed
    if (successCount == 0 && entries.empty()) {
        std::cerr << "Error: No collectors executed successfully\n";
        
        // Print errors to stderr
        for (const auto& error : errors) {
            std::cerr << "  [" << error.source << "] " << error.message;
            if (error.errorCode != 0) {
                std::cerr << " (code: " << error.errorCode << ")";
            }
            std::cerr << "\n";
        }
        
        return EXIT_NO_SOURCES;
    }
    
    // Write output
    bool writeSuccess = JsonWriter::Write(
        args.outputPath,
        entries,
        errors,
        args.prettyPrint,
        args.schemaVersion
    );
    
    if (!writeSuccess) {
        std::cerr << "Error: Failed to write output to " << args.outputPath << std::endl;
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
