using System.Text.Json.Serialization;

namespace Uboot.Core.Models;

/// <summary>
/// Analysis finding for an entry - versioned, with evidence
/// </summary>
public sealed class Finding
{
    [JsonPropertyName("engine_version")]
    public required string EngineVersion { get; init; } // e.g., "1.0.0"

    [JsonPropertyName("classification")]
    public required Classification Classification { get; init; }

    [JsonPropertyName("confidence")]
    public required double Confidence { get; init; } // 0.0 - 0.95 (never 1.00)

    [JsonPropertyName("evidence")]
    public required List<Evidence> Evidence { get; init; } = new();

    [JsonPropertyName("suggested_actions")]
    public List<SuggestedAction>? SuggestedActions { get; init; }

    [JsonPropertyName("analyzed_at")]
    public required DateTimeOffset AnalyzedAt { get; init; }

    [JsonPropertyName("notes")]
    public string? Notes { get; init; }
}
