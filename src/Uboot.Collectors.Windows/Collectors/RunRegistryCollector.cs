using System.Runtime.Versioning;
using System.Security.Cryptography;
using System.Text;
using Uboot.Core.Interfaces;
using Uboot.Core.Models;
using Uboot.Collectors.Windows.Helpers;
using Microsoft.Win32;

namespace Uboot.Collectors.Windows.Collectors;

/// <summary>
/// Collector for Run/RunOnce registry keys (HKCU/HKLM, 32/64-bit)
/// </summary>
[SupportedOSPlatform("windows10.0.19041.0")]
public sealed class RunRegistryCollector : ICollector
{
    private readonly IHashService _hashService;
    private readonly ISignatureService _signatureService;
    private readonly bool _includeSha512;

    public EntrySource Source => EntrySource.RunRegistry;

    private static readonly string[] RegistryPaths = new[]
    {
        @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run",
        @"SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce",
        @"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Run",
        @"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\RunOnce"
    };

    public RunRegistryCollector(IHashService hashService, ISignatureService signatureService, bool includeSha512 = false)
    {
        _hashService = hashService;
        _signatureService = signatureService;
        _includeSha512 = includeSha512;
    }

    public async Task<List<Entry>> CollectAsync(CancellationToken cancellationToken = default)
    {
        var entries = new List<Entry>();
        var collectedAt = DateTimeOffset.Now;

        // HKLM (System)
        foreach (var path in RegistryPaths)
        {
            await CollectFromHiveAsync(Registry.LocalMachine, path, EntryScope.System, entries, collectedAt, cancellationToken);
        }

        // HKCU (User)
        foreach (var path in RegistryPaths)
        {
            await CollectFromHiveAsync(Registry.CurrentUser, path, EntryScope.User, entries, collectedAt, cancellationToken);
        }

        return entries;
    }

    private async Task CollectFromHiveAsync(
        RegistryKey hive,
        string path,
        EntryScope scope,
        List<Entry> entries,
        DateTimeOffset collectedAt,
        CancellationToken cancellationToken)
    {
        try
        {
            using var key = hive.OpenSubKey(path);
            if (key == null)
                return;

            foreach (var valueName in key.GetValueNames())
            {
                var command = key.GetValue(valueName)?.ToString();
                if (string.IsNullOrWhiteSpace(command))
                    continue;

                var (normalized, resolvedPath, arguments, wrapper) = CommandNormalizer.Normalize(command);

                var entry = await CreateEntryAsync(
                    hiveName: hive.Name,
                    path: path,
                    valueName: valueName,
                    command: command,
                    normalized: normalized,
                    resolvedPath: resolvedPath,
                    arguments: arguments,
                    wrapper: wrapper,
                    scope: scope,
                    collectedAt: collectedAt,
                    cancellationToken: cancellationToken
                );

                entries.Add(entry);
            }
        }
        catch
        {
            // Ignore inaccessible keys
        }
    }

    private async Task<Entry> CreateEntryAsync(
        string hiveName,
        string path,
        string valueName,
        string command,
        string normalized,
        string? resolvedPath,
        string? arguments,
        string? wrapper,
        EntryScope scope,
        DateTimeOffset collectedAt,
        CancellationToken cancellationToken)
    {
        var location = $"{hiveName}\\{path}";
        var id = GenerateId(location, valueName, command);

        bool? fileExists = null;
        long? fileSize = null;
        string? fileVersion = null;
        string? fileDescription = null;
        string? fileCompany = null;
        string? md5 = null, sha1 = null, sha256 = null, sha512 = null;
        bool? isSigned = null;
        bool? signatureValid = null;
        string? signerSubject = null, signerIssuer = null;
        DateTimeOffset? signatureTimestamp = null;

        // Try to get file info if resolved path exists
        if (!string.IsNullOrEmpty(resolvedPath) && File.Exists(resolvedPath))
        {
            fileExists = true;
            try
            {
                var fileInfo = new FileInfo(resolvedPath);
                fileSize = fileInfo.Length;

                // Hashes
                try
                {
                    var hashes = await _hashService.ComputeHashesAsync(resolvedPath, _includeSha512, cancellationToken);
                    md5 = hashes.MD5;
                    sha1 = hashes.SHA1;
                    sha256 = hashes.SHA256;
                    sha512 = hashes.SHA512;
                }
                catch
                {
                    // File locked or access denied - not critical
                }

                // Signature
                try
                {
                    var sigInfo = await _signatureService.VerifySignatureAsync(resolvedPath, cancellationToken);
                    isSigned = sigInfo.IsSigned;
                    signatureValid = sigInfo.IsValid;
                    signerSubject = sigInfo.Subject;
                    signerIssuer = sigInfo.Issuer;
                    signatureTimestamp = sigInfo.Timestamp;
                }
                catch
                {
                    // Signature verification failed - not critical
                }

                // File version info
                try
                {
                    var versionInfo = System.Diagnostics.FileVersionInfo.GetVersionInfo(resolvedPath);
                    fileVersion = versionInfo.FileVersion;
                    fileDescription = versionInfo.FileDescription;
                    fileCompany = versionInfo.CompanyName;
                }
                catch
                {
                    // Version info not available
                }
            }
            catch
            {
                // File access error - mark as exists but can't read
            }
        }
        else if (!string.IsNullOrEmpty(resolvedPath))
        {
            fileExists = false;
        }

        return new Entry
        {
            Id = id,
            Source = Source,
            Scope = scope,
            Location = location,
            EntryName = valueName,
            Command = command,
            NormalizedCommand = normalized,
            ResolvedPath = resolvedPath,
            Arguments = arguments,
            WrapperDetected = wrapper,
            IsLink = false,
            Enabled = true,
            CollectedAt = collectedAt,
            FileExists = fileExists,
            FileSize = fileSize,
            FileVersion = fileVersion,
            FileDescription = fileDescription,
            FileCompany = fileCompany,
            MD5 = md5,
            SHA1 = sha1,
            SHA256 = sha256,
            SHA512 = sha512,
            IsSigned = isSigned,
            SignatureValid = signatureValid,
            SignerSubject = signerSubject,
            SignerIssuer = signerIssuer,
            SignatureTimestamp = signatureTimestamp
        };
    }

    private string GenerateId(string location, string valueName, string command)
    {
        var input = $"{location}|{valueName}|{command}";
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(input));
        return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant();
    }
}
