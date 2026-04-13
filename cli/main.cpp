#include "../core/json/JsonWriter.h"
#include "../core/model/CollectorError.h"
#include "../core/model/Entry.h"
#include "../core/runner/CollectorRunner.h"
#include "../core/util/CliArgs.h"
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

// Entry point for uboot-core.exe
// Communicates exclusively via JSON on stdout.
// stderr is reserved for fatal startup errors only.

int main(int argc, char *argv[]) {
    // Force UTF-8 output on Windows
    SetConsoleOutputCP(CP_UTF8);

    uboot::CliArgs args = uboot::CliArgs::Parse(argc, argv);

    if (!args.isValid) {
        std::cerr << "{\"error\":\"" << args.errorMessage << "\"}" << std::endl;
        return 1;
    }

    // Default to all sources when none specified
    if (args.sources.empty()) {
        args.sources.push_back("all");
    }

    if (args.command == "collect" || args.command.empty()) {
        uboot::CollectorRunner runner;
        std::vector<uboot::Entry> entries;
        std::vector<uboot::CollectorError> errors;

        runner.Run(args.sources, entries, errors);

        bool ok = uboot::JsonWriter::Write(
            args.outputPath,
            entries,
            errors,
            args.prettyPrint,
            args.schemaVersion
        );

        return ok ? 0 : 1;
    }

    // Unknown command fallback
    std::cerr << "{\"error\":\"Unknown command: " << args.command << "\"}" << std::endl;
    return 1;
}
