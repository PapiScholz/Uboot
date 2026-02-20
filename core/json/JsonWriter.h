#pragma once

#include "../model/Entry.h"
#include "../model/CollectorError.h"
#include <vector>
#include <string>
#include <fstream>

namespace uboot {

/// <summary>
/// Writes snapshot data to JSON with deterministic output.
/// Uses RapidJSON for UTF-8 support and stable property ordering.
/// </summary>
class JsonWriter {
public:
    /// <summary>
    /// Write entries and errors to JSON file.
    /// </summary>
    /// <param name="outputPath">Output file path (or empty for stdout)</param>
    /// <param name="entries">Collected entries</param>
    /// <param name="errors">Collection errors</param>
    /// <param name="prettyPrint">Enable pretty formatting</param>
    /// <param name="schemaVersion">Schema version string</param>
    /// <returns>true on success, false on failure</returns>
    static bool Write(
        const std::string& outputPath,
        const std::vector<Entry>& entries,
        const std::vector<CollectorError>& errors,
        bool prettyPrint,
        const std::string& schemaVersion
    );

private:
    static std::string ToJson(
        const std::vector<Entry>& entries,
        const std::vector<CollectorError>& errors,
        bool prettyPrint,
        const std::string& schemaVersion
    );
    
    static std::string EscapeJson(const std::string& str);
};

} // namespace uboot
