using System.Net;
using System.Net.Sockets;
using Serilog;

namespace Uboot.Online;

/// <summary>
/// DNS resolution service for domain intelligence
/// </summary>
public sealed class DnsService
{
    public async Task<DnsResult?> ResolveAsync(string domain, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(domain))
            return null;

        try
        {
            var hostEntry = await Dns.GetHostEntryAsync(domain, AddressFamily.InterNetwork, cancellationToken);
            var ipv4Addresses = hostEntry.AddressList
                .Where(a => a.AddressFamily == AddressFamily.InterNetwork)
                .Select(a => a.ToString())
                .ToList();

            // IPv6 (optional)
            var ipv6Task = Dns.GetHostEntryAsync(domain, AddressFamily.InterNetworkV6, cancellationToken);
            var ipv6Addresses = new List<string>();
            try
            {
                var ipv6Entry = await ipv6Task;
                ipv6Addresses = ipv6Entry.AddressList
                    .Where(a => a.AddressFamily == AddressFamily.InterNetworkV6)
                    .Select(a => a.ToString())
                    .ToList();
            }
            catch
            {
                // IPv6 not available
            }

            return new DnsResult
            {
                Domain = domain,
                IPv4Addresses = ipv4Addresses,
                IPv6Addresses = ipv6Addresses,
                QueriedAt = DateTimeOffset.Now
            };
        }
        catch (Exception ex)
        {
            Log.Error(ex, "DNS resolution failed for domain {Domain}", domain);
            return null;
        }
    }
}

public sealed class DnsResult
{
    public required string Domain { get; init; }
    public required List<string> IPv4Addresses { get; init; }
    public required List<string> IPv6Addresses { get; init; }
    public DateTimeOffset QueriedAt { get; init; }
}
