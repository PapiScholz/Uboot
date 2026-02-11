using System.Runtime.Versioning;
using System.Security.Cryptography;
using Uboot.Core.Interfaces;

namespace Uboot.Collectors.Windows;

/// <summary>
/// Hash computation service - MD5, SHA1, SHA256 (always), SHA512 (optional)
/// </summary>
[SupportedOSPlatform("windows10.0.19041.0")]
public sealed class HashService : IHashService
{
    public async Task<FileHashes> ComputeHashesAsync(string filePath, bool includeSha512 = false, CancellationToken cancellationToken = default)
    {
        if (!File.Exists(filePath))
            throw new FileNotFoundException($"File not found: {filePath}");

        try
        {
            await using var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read, 4096, useAsync: true);

            var md5Task = ComputeHashAsync(stream, MD5.Create(), cancellationToken);
            stream.Position = 0;
            var sha1Task = ComputeHashAsync(stream, SHA1.Create(), cancellationToken);
            stream.Position = 0;
            var sha256Task = ComputeHashAsync(stream, SHA256.Create(), cancellationToken);

            string? sha512 = null;
            if (includeSha512)
            {
                stream.Position = 0;
                sha512 = await ComputeHashAsync(stream, SHA512.Create(), cancellationToken);
            }

            var md5 = await md5Task;
            var sha1 = await sha1Task;
            var sha256 = await sha256Task;

            return new FileHashes(md5, sha1, sha256, sha512);
        }
        catch (UnauthorizedAccessException)
        {
            throw new UnauthorizedAccessException($"Access denied to file: {filePath}");
        }
        catch (IOException ex)
        {
            throw new IOException($"Error reading file: {filePath}", ex);
        }
    }

    private async Task<string> ComputeHashAsync(Stream stream, HashAlgorithm algorithm, CancellationToken cancellationToken)
    {
        using (algorithm)
        {
            var hash = await algorithm.ComputeHashAsync(stream, cancellationToken);
            return BitConverter.ToString(hash).Replace("-", "").ToUpperInvariant();
        }
    }
}
