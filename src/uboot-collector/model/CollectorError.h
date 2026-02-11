#pragma once

#include <string>

namespace uboot {

/// <summary>
/// Encapsulates any error that occurs during collection.
/// Ensures no exceptions propagate to main().
/// </summary>
struct CollectorError {
    std::string source;       // which collector failed
    std::string message;      // error description
    int errorCode;            // Windows error code if applicable
    
    CollectorError(
        std::string src,
        std::string msg,
        int code = 0
    ) : source(std::move(src)),
        message(std::move(msg)),
        errorCode(code)
    {}
    
    CollectorError() : errorCode(0) {}
};

} // namespace uboot
