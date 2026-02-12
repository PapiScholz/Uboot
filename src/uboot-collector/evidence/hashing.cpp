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
}

HashResult Hashing::ComputeSHA256(const std::wstring& filePath) noexcept {
    HashResult result;
    
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
    
    // Open algorithm provider
    BCRYPT_ALG_HANDLE hAlgRaw = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &hAlgRaw,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0
    );
    AlgHandlePtr hAlg(hAlgRaw);
    
    if (status < 0) {
        result.ntStatus = status;
        return result;
    }
    
    // Get hash object size
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
    
    if (status < 0) {
        result.ntStatus = status;
        return result;
    }
    
    // Get hash size (should be 32 for SHA-256)
    DWORD hashSize = 0;
    status = BCryptGetProperty(
        hAlg.get(),
        BCRYPT_HASH_LENGTH,
        reinterpret_cast<PBYTE>(&hashSize),
        sizeof(DWORD),
        &resultSize,
        0
    );
    
    if (status < 0 || hashSize != 32) {
        result.ntStatus = status;
        return result;
    }
    
    // Allocate hash object
    std::vector<uint8_t> hashObject(hashObjectSize);
    
    // Create hash
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
    HashHandlePtr hHash(hHashRaw);
    
    if (status < 0) {
        result.ntStatus = status;
        return result;
    }
    
    // Read file and hash in chunks
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    DWORD bytesRead = 0;
    
    while (ReadFile(hFile.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0) {
        status = BCryptHashData(
            hHash.get(),
            buffer.data(),
            bytesRead,
            0
        );
        
        if (status < 0) {
            result.ntStatus = status;
            return result;
        }
    }
    
    // Finalize hash
    SHA256Hash hash;
    status = BCryptFinishHash(
        hHash.get(),
        hash.data(),
        static_cast<ULONG>(hash.size()),
        0
    );
    
    if (status < 0) {
        result.ntStatus = status;
        return result;
    }
    
    result.sha256 = hash;
    return result;
}

std::string Hashing::HashToHexString(const SHA256Hash& hash) noexcept {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    for (uint8_t byte : hash) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    
    return oss.str();
}

} // namespace evidence
} // namespace uboot
