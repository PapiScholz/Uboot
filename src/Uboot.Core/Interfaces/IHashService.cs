namespace Uboot.Core.Interfaces;

/// <summary>
/// File hashing service
/// </summary>
public interface IHashService
{
    Task<FileHashes> ComputeHashesAsync(string filePath, bool includeSha512 = false, CancellationToken cancellationToken = default);
}

public sealed record FileHashes(
    string MD5,
    string SHA1,
    string SHA256,
    string? SHA512
);
