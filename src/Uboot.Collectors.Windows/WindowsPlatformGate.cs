using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Security.Principal;
using Uboot.Core.Interfaces;

namespace Uboot.Collectors.Windows;

/// <summary>
/// Platform gate for Windows - validates OS version and admin rights
/// Requires: Windows 10 22H2 (build 19045+)
/// </summary>
[SupportedOSPlatform("windows10.0.19041.0")]
public sealed class WindowsPlatformGate : IPlatformGate
{
    private const int MinimumBuild = 19045; // Windows 10 22H2

    public PlatformCheck CheckPlatform()
    {
        if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            return new PlatformCheck(
                IsSupported: false,
                IsAdmin: false,
                OsVersion: "Non-Windows",
                OsBuild: 0,
                ErrorMessage: "This application requires Windows 10 22H2 (build 19045) or later."
            );
        }

        var osVersion = Environment.OSVersion;
        var build = osVersion.Version.Build;
        var isSupported = build >= MinimumBuild;

        var isAdmin = IsAdministrator();

        var errorMessage = "";
        if (!isSupported)
        {
            errorMessage = $"Unsupported Windows version. Found build {build}, requires build {MinimumBuild}+ (Windows 10 22H2 or later).";
        }
        else if (!isAdmin)
        {
            errorMessage = "Administrator privileges required. Please run as Administrator.";
        }

        return new PlatformCheck(
            IsSupported: isSupported,
            IsAdmin: isAdmin,
            OsVersion: osVersion.VersionString,
            OsBuild: build,
            ErrorMessage: errorMessage
        );
    }

    private bool IsAdministrator()
    {
        try
        {
            using var identity = WindowsIdentity.GetCurrent();
            var principal = new WindowsPrincipal(identity);
            return principal.IsInRole(WindowsBuiltInRole.Administrator);
        }
        catch
        {
            return false;
        }
    }
}
