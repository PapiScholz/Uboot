using System.Text.Json.Serialization;

namespace Uboot.Core.Models;

/// <summary>
/// Individual piece of evidence contributing to classification
/// </summary>
public sealed class Evidence
{
    [JsonPropertyName("rule_id")]
    public required string RuleId { get; init; } // e.g., "UNSIGNED_BINARY", "PATH_ANOMALY"

    [JsonPropertyName("description")]
    public required string Description { get; init; } // Human-readable, ES/EN

    [JsonPropertyName("weight")]
    public required double Weight { get; init; } // -1.0 (benign) to +1.0 (suspicious)

    [JsonPropertyName("details")]
    public Dictionary<string, object>? Details { get; init; } // Extra context (JSON-serializable)
}
