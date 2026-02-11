namespace Uboot.Core.Models;

/// <summary>
/// Type of suggested action
/// </summary>
public enum ActionType
{
    DisableRegistryValue,
    RemoveRegistryValue,
    DisableScheduledTask,
    DisableService,
    StopService,
    QuarantineFile,
    ManualReview // For WMI/IFEO/Winlogon - suggest only, no auto-execute
}
