namespace Uboot.Core.Models;

/// <summary>
/// Source/collector that discovered this entry
/// </summary>
public enum EntrySource
{
    RunRegistry,
    RunOnceRegistry,
    StartupFolder,
    Service,
    ScheduledTask,
    WmiEventConsumer,
    WinlogonShell,
    WinlogonUserinit,
    IfeoDebugger
}
