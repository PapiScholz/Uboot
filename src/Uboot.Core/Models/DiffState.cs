namespace Uboot.Core.Models;

/// <summary>
/// State of an entry when compared to a baseline
/// </summary>
public enum DiffState
{
    Unchanged,
    Added,
    Removed,
    Modified
}
