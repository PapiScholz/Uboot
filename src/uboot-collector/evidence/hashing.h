#pragma once

#include <string>
#include <optional>
#include <array>
#include <cstdint>
#include <Windows.h>

namespace uboot {
namespace evidence {

/// <summary>
/// SHA-256 hash result (32 bytes).
/// </summary>
using SHA256Hash = std::array<uint8_t, 32>;

/// <summary>
/// Result of a hashing operation.
/// </summary>
struct HashResult {
    std::optional<SHA256Hash> sha256;
    std::optional<NTSTATUS> ntStatus;  // CNG error code if failed
    std::optional<DWORD> win32Error;   // File access error if failed
    
    bool IsSuccess() const { return sha256.has_value(); }
};

/// <summary>
/// File hashing using BCrypt (CNG) for deterministic results.
/// Offline-first, no network dependencies.
/// </summary>
class Hashing {
public:
    /// <summary>
    /// Compute SHA-256 hash of a file.
    /// Uses BCrypt (CNG) which is available on Win10 22H2+.
    /// </summary>
    /// <param name="filePath">Full path to file</param>
    /// <returns>HashResult with sha256 or error codes</returns>
    static HashResult ComputeSHA256(const std::wstring& filePath) noexcept;
    
    /// <summary>
    /// Convert hash to lowercase hex string (64 chars).
    /// </summary>
    static std::string HashToHexString(const SHA256Hash& hash) noexcept;
    
private:
    Hashing() = delete;
    
    static constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1MB read buffer
};

} // namespace evidence
} // namespace uboot
