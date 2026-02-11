using System.Text.Json.Serialization;

namespace Uboot.Core.Models;

/// <summary>
/// Difference between two snapshots
/// </summary>
public sealed class SnapshotDiff
{
    [JsonPropertyName("diff_id")]
    public required string DiffId { get; init; }

    [JsonPropertyName("baseline_id")]
    public required string BaselineId { get; init; }

    [JsonPropertyName("baseline_name")]
    public required string BaselineName { get; init; }

    [JsonPropertyName("current_id")]
    public required string CurrentId { get; init; }

    [JsonPropertyName("current_name")]
    public required string CurrentName { get; init; }

    [JsonPropertyName("created_at")]
    public required DateTimeOffset CreatedAt { get; init; }

    [JsonPropertyName("added")]
    public required List<Entry> Added { get; init; } = new();

    [JsonPropertyName("removed")]
    public required List<Entry> Removed { get; init; } = new();

    [JsonPropertyName("modified")]
    public required List<Entry> Modified { get; init; } = new();

    [JsonPropertyName("summary")]
    public required DiffSummary Summary { get; init; }
}

public sealed class DiffSummary
{
    [JsonPropertyName("total_added")]
    public required int TotalAdded { get; init; }

    [JsonPropertyName("total_removed")]
    public required int TotalRemoved { get; init; }

    [JsonPropertyName("total_modified")]
    public required int TotalModified { get; init; }

    [JsonPropertyName("total_unchanged")]
    public required int TotalUnchanged { get; init; }
}
