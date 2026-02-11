using System.Runtime.Versioning;
using System.Security.Cryptography;
using System.Text;
using Uboot.Core.Interfaces;
using Uboot.Core.Models;
using Uboot.Collectors.Windows.Helpers;

namespace Uboot.Collectors.Windows.Collectors;

/// <summary>
/// Collector for Startup folders (User and Common) with .lnk resolution
/// </summary>
[SupportedOSPlatform("windows10.0.19041.0")]
public sealed class StartupFolderCollector : ICollector
{
    private readonly IHashService _hashService;
    private readonly ISignatureService _signatureService;
    private readonly bool _includeSha512;

    public EntrySource Source => EntrySource.StartupFolder;

    public StartupFolderCollector(IHashService hashService, ISignatureService signatureService, bool includeSha512 = false)
    {
        _hashService = hashService;
        _signatureService = signatureService;
        _includeSha512 = includeSha512;
    }

    public async Task<List<Entry>> CollectAsync(CancellationToken cancellationToken = default)
    {
        var entries = new List<Entry>();
        var collectedAt = DateTimeOffset.Now;

        // User startup folder
        var userStartup = Environment.GetFolderPath(Environment.SpecialFolder.Startup);
        await CollectFromFolderAsync(userStartup, EntryScope.User, entries, collectedAt, cancellationToken);

        // Common startup folder (all users)
        var commonStartup = Environment.GetFolderPath(Environment.SpecialFolder.CommonStartup);
        await CollectFromFolderAsync(commonStartup, EntryScope.System, entries, collectedAt, cancellationToken);

        return entries;
    }

    private async Task CollectFromFolderAsync(
        string folderPath,
        EntryScope scope,
        List<Entry> entries,
        DateTimeOffset collectedAt,
        CancellationToken cancellationToken)
    {
        if (!Directory.Exists(folderPath))
            return;

        try
        {
            var files = Directory.GetFiles(folderPath, "*.*", SearchOption.TopDirectoryOnly);

            foreach (var file in files)
            {
                var fileName = Path.GetFileName(file);
                var isLink = file.EndsWith(".lnk", StringComparison.OrdinalIgnoreCase);

                string command = file;
                string? linkTarget = null;
                string? resolvedPath = file;
                string? arguments = null;
                string? wrapper = null;

                // Resolve .lnk if applicable (simplified - full implementation would use Shell COM)
                if (isLink)
                {
                    // Stub: In a full implementation, use IWshRuntimeLibrary or P/Invoke to resolve .lnk
                    // For MVP, we'll just mark it as a link
                    linkTarget = file; // Placeholder
                    resolvedPath = null; // Would be the target path
                }
                else
                {
                    var (normalized, resolved, args, wrapperDetected) = CommandNormalizer.Normalize(command);
                    resolvedPath = resolved;
                    arguments = args;
                    wrapper = wrapperDetected;
                }

                var id = GenerateId(folderPath, fileName);

                var entry = new Entry
                {
                    Id = id,
                    Source = Source,
                    Scope = scope,
                    Location = folderPath,
                    EntryName = fileName,
                    Command = command,
                    NormalizedCommand = command,
                    ResolvedPath = resolvedPath,
                    Arguments = arguments,
                    WrapperDetected = wrapper,
                    IsLink = isLink,
                    LinkTarget = linkTarget,
                    Enabled = true,
                    CollectedAt = collectedAt,
                    FileExists = File.Exists(file)
                };

                // Hash and sign if file exists and is not a link (or resolved link target)
                if (resolvedPath != null && File.Exists(resolvedPath))
                {
                    try
                    {
                        var hashes = await _hashService.ComputeHashesAsync(resolvedPath, _includeSha512, cancellationToken);
                        entry = entry with
                        {
                            MD5 = hashes.MD5,
                            SHA1 = hashes.SHA1,
                            SHA256 = hashes.SHA256,
                            SHA512 = hashes.SHA512
                        };

                        var sigInfo = await _signatureService.VerifySignatureAsync(resolvedPath, cancellationToken);
                        entry = entry with
                        {
                            IsSigned = sigInfo.IsSigned,
                            SignatureValid = sigInfo.IsValid,
                            SignerSubject = sigInfo.Subject,
                            SignerIssuer = sigInfo.Issuer,
                            SignatureTimestamp = sigInfo.Timestamp
                        };
                    }
                    catch
                    {
                        // Hash/sign failed - not critical
                    }
                }

                entries.Add(entry);
            }
        }
        catch
        {
            // Folder access error
        }
    }

    private string GenerateId(string location, string fileName)
    {
        var input = $"{location}|{fileName}";
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(input));
        return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant();
    }
}
