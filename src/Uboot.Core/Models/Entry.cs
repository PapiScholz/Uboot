using System.Text.Json.Serialization;

namespace Uboot.Core.Models;

/// <summary>
/// Main entry representing an autorun item discovered on the system.
/// Fact-based only - no analysis here.
/// </summary>
public sealed record Entry
{
    [JsonPropertyName("id")]
    public required string Id { get; init; } // Hash of source+location+command for uniqueness

    [JsonPropertyName("source")]
    public required EntrySource Source { get; init; }

    [JsonPropertyName("scope")]
    public required EntryScope Scope { get; init; }

    [JsonPropertyName("location")]
    public required string Location { get; init; } // Registry path, folder path, service name, etc.

    [JsonPropertyName("entry_name")]
    public required string EntryName { get; init; } // Value name, file name, task name

    [JsonPropertyName("command")]
    public required string Command { get; init; } // Raw command/path as found

    [JsonPropertyName("normalized_command")]
    public string? NormalizedCommand { get; init; } // After env expansion, canonicalization

    [JsonPropertyName("resolved_path")]
    public string? ResolvedPath { get; init; } // Actual binary path after unwrapping

    [JsonPropertyName("arguments")]
    public string? Arguments { get; init; }

    [JsonPropertyName("is_link")]
    public bool IsLink { get; init; } // True if .lnk was resolved

    [JsonPropertyName("link_target")]
    public string? LinkTarget { get; init; }

    [JsonPropertyName("wrapper_detected")]
    public string? WrapperDetected { get; init; } // cmd, powershell, rundll32, etc.

    [JsonPropertyName("enabled")]
    public bool Enabled { get; init; } = true;

    [JsonPropertyName("collected_at")]
    public required DateTimeOffset CollectedAt { get; init; }

    [JsonPropertyName("first_seen")]
    public DateTimeOffset? FirstSeen { get; init; }

    [JsonPropertyName("last_seen")]
    public DateTimeOffset? LastSeen { get; init; }

    [JsonPropertyName("diff_state")]
    public DiffState DiffState { get; init; } = DiffState.Unchanged;

    [JsonPropertyName("fields_changed")]
    public List<string>? FieldsChanged { get; init; }

    // File metadata (if resolved_path exists and accessible)
    [JsonPropertyName("file_exists")]
    public bool? FileExists { get; init; }

    [JsonPropertyName("file_version")]
    public string? FileVersion { get; init; }

    [JsonPropertyName("file_description")]
    public string? FileDescription { get; init; }

    [JsonPropertyName("file_company")]
    public string? FileCompany { get; init; }

    [JsonPropertyName("md5")]
    public string? MD5 { get; init; }

    [JsonPropertyName("sha1")]
    public string? SHA1 { get; init; }

    [JsonPropertyName("sha256")]
    public string? SHA256 { get; init; }

    [JsonPropertyName("sha512")]
    public string? SHA512 { get; init; } // Optional, behind toggle

    // Authenticode signature (offline validation only)
    [JsonPropertyName("is_signed")]
    public bool? IsSigned { get; init; }

    [JsonPropertyName("signature_valid")]
    public bool? SignatureValid { get; init; } // Chain validation, NO CRL/OCSP

    [JsonPropertyName("signer_subject")]
    public string? SignerSubject { get; init; }

    [JsonPropertyName("signer_issuer")]
    public string? SignerIssuer { get; init; }

    [JsonPropertyName("signature_timestamp")]
    public DateTimeOffset? SignatureTimestamp { get; init; }

    [JsonPropertyName("file_size")]
    public long? FileSize { get; init; }

    // Analysis result (separate from facts)
    [JsonPropertyName("finding")]
    public Finding? Finding { get; init; }
}
