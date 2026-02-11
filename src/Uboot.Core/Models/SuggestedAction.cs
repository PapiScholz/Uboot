using System.Text.Json.Serialization;

namespace Uboot.Core.Models;

/// <summary>
/// Action suggested by analysis engine
/// </summary>
public sealed class SuggestedAction
{
    [JsonPropertyName("action_type")]
    public required ActionType ActionType { get; init; }

    [JsonPropertyName("description")]
    public required string Description { get; init; } // ES/EN

    [JsonPropertyName("safe_to_execute")]
    public required bool SafeToExecute { get; init; } // False for ManualReview

    [JsonPropertyName("requires_confirmation")]
    public bool RequiresConfirmation { get; init; } = true;

    [JsonPropertyName("undo_information")]
    public UndoInformation? UndoInformation { get; init; }
}
