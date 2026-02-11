namespace Uboot.Core.Interfaces;

/// <summary>
/// Platform gate - validates OS version and admin rights
/// </summary>
public interface IPlatformGate
{
    PlatformCheck CheckPlatform();
}

public sealed record PlatformCheck(
    bool IsSupported,
    bool IsAdmin,
    string OsVersion,
    int OsBuild,
    string ErrorMessage // Empty if IsSupported && IsAdmin
);
