using System.Runtime.Versioning;
using System.Security.Cryptography;
using System.ServiceProcess;
using System.Text;
using Uboot.Core.Interfaces;
using Uboot.Core.Models;
using Uboot.Collectors.Windows.Helpers;

namespace Uboot.Collectors.Windows.Collectors;

/// <summary>
/// Collector for Windows Services (userland only, no drivers)
/// </summary>
[SupportedOSPlatform("windows10.0.19041.0")]
public sealed class ServicesCollector : ICollector
{
    private readonly IHashService _hashService;
    private readonly ISignatureService _signatureService;
    private readonly bool _includeSha512;

    public EntrySource Source => EntrySource.Service;

    public ServicesCollector(IHashService hashService, ISignatureService signatureService, bool includeSha512 = false)
    {
        _hashService = hashService;
        _signatureService = signatureService;
        _includeSha512 = includeSha512;
    }

    public async Task<List<Entry>> CollectAsync(CancellationToken cancellationToken = default)
    {
        var entries = new List<Entry>();
        var collectedAt = DateTimeOffset.Now;

        try
        {
            var services = ServiceController.GetServices();

            foreach (var service in services)
            {
                try
                {
                    // Skip kernel drivers - userland only
                    if (service.ServiceType.HasFlag(ServiceType.KernelDriver) ||
                        service.ServiceType.HasFlag(ServiceType.FileSystemDriver))
                        continue;

                    // Get service image path from registry
                    var imagePath = GetServiceImagePath(service.ServiceName);
                    if (string.IsNullOrEmpty(imagePath))
                        continue;

                    var (normalized, resolvedPath, arguments, wrapper) = CommandNormalizer.Normalize(imagePath);

                    var id = GenerateId(service.ServiceName);

                    var entry = new Entry
                    {
                        Id = id,
                        Source = Source,
                        Scope = EntryScope.System,
                        Location = $"Services\\{service.ServiceName}",
                        EntryName = service.DisplayName ?? service.ServiceName,
                        Command = imagePath,
                        NormalizedCommand = normalized,
                        ResolvedPath = resolvedPath,
                        Arguments = arguments,
                        WrapperDetected = wrapper,
                        Enabled = service.StartType != ServiceStartMode.Disabled,
                        CollectedAt = collectedAt
                    };

                    // Hash and sign if executable exists
                    if (resolvedPath != null && File.Exists(resolvedPath))
                    {
                        try
                        {
                            entry = entry with { FileExists = true };

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
                            // Hash/sign failed
                        }
                    }

                    entries.Add(entry);
                }
                catch
                {
                    // Service enumeration error - skip this service
                }
            }
        }
        catch
        {
            // Services enumeration failed
        }

        return entries;
    }

    private string? GetServiceImagePath(string serviceName)
    {
        try
        {
            using var key = Microsoft.Win32.Registry.LocalMachine.OpenSubKey($@"SYSTEM\CurrentControlSet\Services\{serviceName}");
            return key?.GetValue("ImagePath")?.ToString();
        }
        catch
        {
            return null;
        }
    }

    private string GenerateId(string serviceName)
    {
        var input = $"Service|{serviceName}";
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(input));
        return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant();
    }
}
