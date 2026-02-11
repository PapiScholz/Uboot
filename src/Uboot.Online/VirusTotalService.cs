using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Serialization;
using Serilog;

namespace Uboot.Online;

/// <summary>
/// VirusTotal hash/URL/domain lookup (toggle OFF by default)
/// API key: env var VIRUSTOTAL_API_KEY
/// </summary>
public sealed class VirusTotalService
{
    private readonly HttpClient _httpClient;
    private readonly string? _apiKey;
    private readonly string _cacheDirectory;
    private const string ApiBaseUrl = "https://www.virustotal.com/api/v3";

    public bool IsConfigured => !string.IsNullOrEmpty(_apiKey);

    public VirusTotalService(string? cacheDirectory = null)
    {
        _apiKey = Environment.GetEnvironmentVariable("VIRUSTOTAL_API_KEY");
        _httpClient = new HttpClient
        {
            Timeout = TimeSpan.FromSeconds(30)
        };

        if (!string.IsNullOrEmpty(_apiKey))
        {
            _httpClient.DefaultRequestHeaders.Add("x-apikey", _apiKey);
        }

        _cacheDirectory = cacheDirectory ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            "Uboot",
            "cache",
            "virustotal"
        );

        Directory.CreateDirectory(_cacheDirectory);
    }

    public async Task<VtHashReport?> LookupHashAsync(string hash, CancellationToken cancellationToken = default)
    {
        if (!IsConfigured)
        {
            Log.Warning("VirusTotal API key not configured");
            return null;
        }

        // Check cache
        var cached = GetCachedReport(hash);
        if (cached != null)
            return cached;

        try
        {
            var url = $"{ApiBaseUrl}/files/{hash}";
            var response = await _httpClient.GetAsync(url, cancellationToken);

            if (!response.IsSuccessStatusCode)
            {
                Log.Warning("VirusTotal hash lookup failed: {StatusCode}", response.StatusCode);
                return null;
            }

            var json = await response.Content.ReadAsStringAsync(cancellationToken);
            var vtResponse = JsonSerializer.Deserialize<VtResponse>(json);

            if (vtResponse?.Data?.Attributes == null)
                return null;

            var report = new VtHashReport
            {
                Hash = hash,
                LastAnalysisStats = vtResponse.Data.Attributes.LastAnalysisStats,
                Reputation = vtResponse.Data.Attributes.Reputation,
                QueriedAt = DateTimeOffset.Now
            };

            // Cache result
            CacheReport(hash, report);

            return report;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "VirusTotal API error for hash {Hash}", hash);
            return null;
        }
    }

    private VtHashReport? GetCachedReport(string hash)
    {
        var cachePath = Path.Combine(_cacheDirectory, $"{hash}.json");
        if (!File.Exists(cachePath))
            return null;

        try
        {
            var json = File.ReadAllText(cachePath);
            var report = JsonSerializer.Deserialize<VtHashReport>(json);

            // Cache valid for 7 days
            if (report != null && (DateTimeOffset.Now - report.QueriedAt).TotalDays < 7)
                return report;
        }
        catch
        {
            // Cache read error
        }

        return null;
    }

    private void CacheReport(string hash, VtHashReport report)
    {
        try
        {
            var cachePath = Path.Combine(_cacheDirectory, $"{hash}.json");
            var json = JsonSerializer.Serialize(report);
            File.WriteAllText(cachePath, json);
        }
        catch
        {
            // Cache write error - not critical
        }
    }

    private sealed class VtResponse
    {
        [JsonPropertyName("data")]
        public VtData? Data { get; set; }
    }

    private sealed class VtData
    {
        [JsonPropertyName("attributes")]
        public VtAttributes? Attributes { get; set; }
    }

    private sealed class VtAttributes
    {
        [JsonPropertyName("last_analysis_stats")]
        public VtStats? LastAnalysisStats { get; set; }

        [JsonPropertyName("reputation")]
        public int Reputation { get; set; }
    }

    public sealed class VtStats
    {
        [JsonPropertyName("malicious")]
        public int Malicious { get; set; }

        [JsonPropertyName("suspicious")]
        public int Suspicious { get; set; }

        [JsonPropertyName("undetected")]
        public int Undetected { get; set; }

        [JsonPropertyName("harmless")]
        public int Harmless { get; set; }
    }
}

public sealed class VtHashReport
{
    public required string Hash { get; init; }
    public VirusTotalService.VtStats? LastAnalysisStats { get; init; }
    public int Reputation { get; init; }
    public DateTimeOffset QueriedAt { get; init; }
}
