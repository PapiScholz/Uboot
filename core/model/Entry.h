#pragma once

#include <string>
#include <optional>
#include <cstdint>

namespace uboot {

/// <summary>
/// Represents a single boot entry discovered by a collector.
/// All fields map to the JSON schema expected by the analyzer.
/// </summary>
struct Entry {
    std::string source;           // e.g., "RunRegistry", "Services", "StartupFolder"
    std::string scope;            // e.g., "User", "Machine"
    std::string key;              // unique identification within source
    std::string location;         // path or registry key
    std::string arguments;        // command arguments if any
    std::string imagePath;        // resolved executable path
    
    std::optional<std::string> keyName;         // for registry entries
    std::optional<std::string> displayName;     // service display name, etc.
    std::optional<std::string> publisher;       // file publisher if available
    std::optional<std::string> description;     // optional description
    
    // Constructor with required fields
    Entry(
        std::string src,
        std::string scp,
        std::string k,
        std::string loc,
        std::string args,
        std::string img
    ) : source(std::move(src)),
        scope(std::move(scp)),
        key(std::move(k)),
        location(std::move(loc)),
        arguments(std::move(args)),
        imagePath(std::move(img))
    {}
    
    Entry() = default;
};

} // namespace uboot
