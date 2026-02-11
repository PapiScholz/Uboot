using System.Text.Json.Serialization;

namespace Uboot.Core.Models;

/// <summary>
/// Snapshot of system state at a point in time
/// </summary>
public sealed class Snapshot
{
    [JsonPropertyName("id")]
    public required string Id { get; init; } // GUID or timestamp-based

    [JsonPropertyName("name")]
    public required string Name { get; init; } // e.g., "baseline_clean", "scan_20260210_1230"

    [JsonPropertyName("is_baseline")]
    public bool IsBaseline { get; init; }

    [JsonPropertyName("created_at")]
    public required DateTimeOffset CreatedAt { get; init; }

    [JsonPropertyName("system_info")]
    public required SystemInfo SystemInfo { get; init; }

    [JsonPropertyName("entries")]
    public required List<Entry> Entries { get; init; } = new();

    [JsonPropertyName("engine_version")]
    public required string EngineVersion { get; init; }

    [JsonPropertyName("metadata")]
    public Dictionary<string, object>? Metadata { get; init; }
}

public sealed class SystemInfo
{
    [JsonPropertyName("hostname")]
    public required string Hostname { get; init; }

    [JsonPropertyName("os_version")]
    public required string OsVersion { get; init; }

    [JsonPropertyName("os_build")]
    public required string OsBuild { get; init; }

    [JsonPropertyName("architecture")]
    public required string Architecture { get; init; }

    [JsonPropertyName("username")]
    public required string Username { get; init; }

    [JsonPropertyName("is_admin")]
    public required bool IsAdmin { get; init; }
}
