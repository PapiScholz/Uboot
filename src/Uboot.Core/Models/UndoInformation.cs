using System.Text.Json.Serialization;

namespace Uboot.Core.Models;

/// <summary>
/// Information to undo an executed action
/// </summary>
public sealed class UndoInformation
{
    [JsonPropertyName("action_type")]
    public required ActionType ActionType { get; init; }

    [JsonPropertyName("original_state")]
    public required Dictionary<string, object> OriginalState { get; init; }

    [JsonPropertyName("executed_at")]
    public required DateTimeOffset ExecutedAt { get; init; }

    [JsonPropertyName("backup_path")]
    public string? BackupPath { get; init; } // For quarantined files
}
