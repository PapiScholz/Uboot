using System.Net;
using System.Net.Sockets;
using System.Text;
using Serilog;

namespace Uboot.Online;

/// <summary>
/// WHOIS lookup service (TCP/43) for domain intelligence
/// </summary>
public sealed class WhoisService
{
    private const int WhoisPort = 43;
    private const int TimeoutSeconds = 10;

    public async Task<WhoisResult?> LookupAsync(string domain, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(domain))
            return null;

        try
        {
            // Determine WHOIS server based on TLD
            var whoisServer = GetWhoisServer(domain);
            if (string.IsNullOrEmpty(whoisServer))
            {
                Log.Warning("No WHOIS server found for domain {Domain}", domain);
                return null;
            }

            using var client = new TcpClient();
            await client.ConnectAsync(whoisServer, WhoisPort, cancellationToken);

            using var stream = client.GetStream();
            var query = Encoding.ASCII.GetBytes($"{domain}\r\n");
            await stream.WriteAsync(query, cancellationToken);

            using var reader = new StreamReader(stream, Encoding.ASCII);
            var response = await reader.ReadToEndAsync(cancellationToken);

            return new WhoisResult
            {
                Domain = domain,
                Server = whoisServer,
                RawResponse = response,
                QueriedAt = DateTimeOffset.Now
            };
        }
        catch (Exception ex)
        {
            Log.Error(ex, "WHOIS lookup failed for domain {Domain}", domain);
            return null;
        }
    }

    private string? GetWhoisServer(string domain)
    {
        var tld = domain.Split('.').LastOrDefault()?.ToLowerInvariant();

        return tld switch
        {
            "com" => "whois.verisign-grs.com",
            "net" => "whois.verisign-grs.com",
            "org" => "whois.pir.org",
            "info" => "whois.afilias.net",
            "biz" => "whois.biz",
            "us" => "whois.nic.us",
            _ => "whois.iana.org" // Fallback
        };
    }
}

public sealed class WhoisResult
{
    public required string Domain { get; init; }
    public required string Server { get; init; }
    public required string RawResponse { get; init; }
    public DateTimeOffset QueriedAt { get; init; }
}
