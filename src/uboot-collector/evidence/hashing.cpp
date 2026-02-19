#include "hashing.h"
#include <bcrypt.h>
#include <fileapi.h>
#include <memory>
#include <vector>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

namespace uboot {
namespace evidence {

namespace {
    // RAII wrapper for BCRYPT_ALG_HANDLE
    struct AlgHandleDeleter {
        void operator()(BCRYPT_ALG_HANDLE h) const {
            if (h) BCryptCloseAlgorithmProvider(h, 0);
        }
    };
    using AlgHandlePtr = std::unique_ptr<std::remove_pointer_t<BCRYPT_ALG_HANDLE>, AlgHandleDeleter>;
    
    // RAII wrapper for BCRYPT_HASH_HANDLE
    struct HashHandleDeleter {
        void operator()(BCRYPT_HASH_HANDLE h) const {
            if (h) BCryptDestroyHash(h);
        }
    };
    using HashHandlePtr = std::unique_ptr<std::remove_pointer_t<BCRYPT_HASH_HANDLE>, HashHandleDeleter>;
    
    // RAII wrapper for file handle
    struct FileHandleDeleter {
        void operator()(HANDLE h) const {
            if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        }
    };
    using FileHandlePtr = std::unique_ptr<std::remove_pointer_t<HANDLE>, FileHandleDeleter>;

    // Helper class to manage a single hash algorithm context
    class HashContext {
    public:
        NTSTATUS Initialize(LPCWSTR algId) {
            BCRYPT_ALG_HANDLE hAlgRaw = nullptr;
            NTSTATUS status = BCryptOpenAlgorithmProvider(
                &hAlgRaw,
                algId,
                nullptr,
                0
            );
            if (status < 0) return status;
            hAlg.reset(hAlgRaw);

            DWORD hashObjectSize = 0;
            DWORD resultSize = 0;
            status = BCryptGetProperty(
                hAlg.get(),
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PBYTE>(&hashObjectSize),
                sizeof(DWORD),
                &resultSize,
                0
            );
            if (status < 0) return status;

            hashObject.resize(hashObjectSize);

            BCRYPT_HASH_HANDLE hHashRaw = nullptr;
            status = BCryptCreateHash(
                hAlg.get(),
                &hHashRaw,
                hashObject.data(),
                hashObjectSize,
                nullptr,
                0,
                0
            );
            if (status < 0) return status;
            hHash.reset(hHashRaw);

            return 0; // STATUS_SUCCESS
        }

        NTSTATUS Update(const uint8_t* buffer, DWORD size) {
            return BCryptHashData(hHash.get(), const_cast<PUCHAR>(buffer), size, 0);
        }

        NTSTATUS Finish(std::vector<uint8_t>& output) {
            DWORD hashSize = 0;
            DWORD resultSize = 0;
            NTSTATUS status = BCryptGetProperty(
                hAlg.get(),
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PBYTE>(&hashSize),
                sizeof(DWORD),
                &resultSize,
                0
            );
            if (status < 0) return status;

            output.resize(hashSize);
            return BCryptFinishHash(hHash.get(), output.data(), hashSize, 0);
        }

    private:
        AlgHandlePtr hAlg;
        HashHandlePtr hHash;
        std::vector<uint8_t> hashObject;
    };
}

HashEvidence Hashing::ComputeHashes(const std::wstring& filePath) noexcept {
    HashEvidence result;
    
    // Open file for reading
    FileHandlePtr hFile(CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    ));
    
    if (hFile.get() == INVALID_HANDLE_VALUE) {
        result.win32Error = GetLastError();
        return result;
    }

    HashContext ctxMd5, ctxSha1, ctxSha256;
    NTSTATUS status;

    if ((status = ctxMd5.Initialize(BCRYPT_MD5_ALGORITHM)) < 0) { result.ntStatus = status; return result; }
    if ((status = ctxSha1.Initialize(BCRYPT_SHA1_ALGORITHM)) < 0) { result.ntStatus = status; return result; }
    if ((status = ctxSha256.Initialize(BCRYPT_SHA256_ALGORITHM)) < 0) { result.ntStatus = status; return result; }

    // Read file and update all hashes
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    DWORD bytesRead = 0;
    
    while (ReadFile(hFile.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0) {
        if ((status = ctxMd5.Update(buffer.data(), bytesRead)) < 0) { result.ntStatus = status; return result; }
        if ((status = ctxSha1.Update(buffer.data(), bytesRead)) < 0) { result.ntStatus = status; return result; }
        if ((status = ctxSha256.Update(buffer.data(), bytesRead)) < 0) { result.ntStatus = status; return result; }
    }

    // Finalize hashes
    std::vector<uint8_t> md5Bytes, sha1Bytes, sha256Bytes;

    if ((status = ctxMd5.Finish(md5Bytes)) < 0) { result.ntStatus = status; return result; }
    if ((status = ctxSha1.Finish(sha1Bytes)) < 0) { result.ntStatus = status; return result; }
    if ((status = ctxSha256.Finish(sha256Bytes)) < 0) { result.ntStatus = status; return result; }

    // Copy to result structs
    result.md5.emplace();
    std::copy(md5Bytes.begin(), md5Bytes.end(), result.md5->begin());

    result.sha1.emplace();
    std::copy(sha1Bytes.begin(), sha1Bytes.end(), result.sha1->begin());

    result.sha256.emplace();
    std::copy(sha256Bytes.begin(), sha256Bytes.end(), result.sha256->begin());

    return result;
}

std::string Hashing::ToHex(const std::vector<uint8_t>& bytes) noexcept {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    for (uint8_t byte : bytes) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    
    return oss.str();
}

} // namespace evidence
} // namespace uboot
