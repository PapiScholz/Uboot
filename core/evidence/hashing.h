#pragma once

#include <string>
#include <optional>
#include <array>
#include <cstdint>
#include <vector>
#include <Windows.h>

namespace uboot {
namespace evidence {

/// <summary>
/// MD5 hash result (16 bytes).
/// </summary>
using MD5Hash = std::array<uint8_t, 16>;

/// <summary>
/// SHA-1 hash result (20 bytes).
/// </summary>
using SHA1Hash = std::array<uint8_t, 20>;

/// <summary>
/// SHA-256 hash result (32 bytes).
/// </summary>
using SHA256Hash = std::array<uint8_t, 32>;

/// <summary>
/// Result of a hashing operation.
/// Contains all computed hashes or error information.
/// </summary>
struct HashEvidence {
    std::optional<MD5Hash> md5;
    std::optional<SHA1Hash> sha1;
    std::optional<SHA256Hash> sha256;
    
    std::optional<NTSTATUS> ntStatus;  // CNG error code if failed
    std::optional<DWORD> win32Error;   // File access error if failed
    
    bool IsSuccess() const { 
        return md5.has_value() && sha1.has_value() && sha256.has_value(); 
    }
};

/// <summary>
/// File hashing using BCrypt (CNG) for deterministic results.
/// Offline-first, no network dependencies.
/// </summary>
class Hashing {
public:
    /// <summary>
    /// Compute MD5, SHA-1, and SHA-256 hashes of a file.
    /// Uses BCrypt (CNG) which is available on Win10 22H2+.
    /// </summary>
    /// <param name="filePath">Full path to file</param>
    /// <returns>HashEvidence with hashes or error codes</returns>
    static HashEvidence ComputeHashes(const std::wstring& filePath) noexcept;
    
    /// <summary>
    /// Convert byte array to lowercase hex string.
    /// </summary>
    static std::string ToHex(const std::vector<uint8_t>& bytes) noexcept;
    
    template<size_t N>
    static std::string ToHex(const std::array<uint8_t, N>& bytes) noexcept {
        std::vector<uint8_t> vec(bytes.begin(), bytes.end());
        return ToHex(vec);
    }
    
private:
    Hashing() = delete;
    
    static constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1MB read buffer
};

} // namespace evidence
} // namespace uboot
