namespace Uboot.Core.Interfaces;

/// <summary>
/// Authenticode signature verification (offline only)
/// </summary>
public interface ISignatureService
{
    Task<SignatureInfo> VerifySignatureAsync(string filePath, CancellationToken cancellationToken = default);
}

public sealed record SignatureInfo(
    bool IsSigned,
    bool? IsValid, // null if not signed
    string? Subject,
    string? Issuer,
    DateTimeOffset? Timestamp
);
