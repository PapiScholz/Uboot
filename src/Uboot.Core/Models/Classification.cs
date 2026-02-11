namespace Uboot.Core.Models;

/// <summary>
/// Classification level - ultra-conservative, Unknown-first
/// NEVER use "Malware" or definitive terms
/// </summary>
public enum Classification
{
    Benign,
    LikelyBenign,
    Unknown,
    Suspicious,
    NeedsReview
}
