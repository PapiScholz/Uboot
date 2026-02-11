using System.Runtime.Versioning;
using System.Text;
using System.Text.RegularExpressions;

namespace Uboot.Collectors.Windows.Helpers;

/// <summary>
/// Command normalization helper - 4A approach:
/// 1. Expand environment variables
/// 2. Canonize paths
/// 3. Detect wrappers
/// 4. Extract resolved_path for hashing
/// </summary>
[SupportedOSPlatform("windows10.0.19041.0")]
public static partial class CommandNormalizer
{
    private static readonly string[] KnownWrappers = new[]
    {
        "cmd.exe", "cmd",
        "powershell.exe", "powershell", "pwsh.exe", "pwsh",
        "wscript.exe", "wscript",
        "cscript.exe", "cscript",
        "rundll32.exe", "rundll32",
        "mshta.exe", "mshta",
        "regsvr32.exe", "regsvr32"
    };

    public static (string NormalizedCommand, string? ResolvedPath, string? Arguments, string? WrapperDetected) Normalize(string rawCommand)
    {
        if (string.IsNullOrWhiteSpace(rawCommand))
            return (rawCommand, null, null, null);

        // Step 1: Expand environment variables
        var expanded = Environment.ExpandEnvironmentVariables(rawCommand);

        // Step 2: Parse command and arguments
        var (commandPart, args) = SplitCommandAndArgs(expanded);

        // Step 3: Canonize path
        string resolvedPath;
        try
        {
            resolvedPath = Path.GetFullPath(commandPart);
        }
        catch
        {
            resolvedPath = commandPart;
        }

        // Step 4: Detect wrappers
        var fileName = Path.GetFileName(resolvedPath).ToLowerInvariant();
        var wrapper = KnownWrappers.FirstOrDefault(w => fileName.Contains(w));

        // If wrapper detected, try to extract actual binary from args
        if (wrapper != null && !string.IsNullOrEmpty(args))
        {
            var innerCommand = ExtractInnerCommand(args, wrapper);
            if (innerCommand != null)
            {
                resolvedPath = innerCommand;
            }
        }

        return (expanded, resolvedPath, args, wrapper);
    }

    private static (string Command, string? Arguments) SplitCommandAndArgs(string fullCommand)
    {
        fullCommand = fullCommand.Trim();

        // Handle quoted paths
        if (fullCommand.StartsWith("\""))
        {
            var endQuote = fullCommand.IndexOf("\"", 1);
            if (endQuote > 0)
            {
                var command = fullCommand.Substring(1, endQuote - 1);
                var args = endQuote + 1 < fullCommand.Length ? fullCommand.Substring(endQuote + 2).Trim() : null;
                return (command, args);
            }
        }

        // No quotes - split at first space
        var spaceIndex = fullCommand.IndexOf(' ');
        if (spaceIndex > 0)
        {
            return (fullCommand.Substring(0, spaceIndex), fullCommand.Substring(spaceIndex + 1).Trim());
        }

        return (fullCommand, null);
    }

    private static string? ExtractInnerCommand(string args, string wrapper)
    {
        // Simple extraction for common patterns
        // cmd /c "program.exe" -> program.exe
        // powershell.exe -File script.ps1 -> script.ps1
        // rundll32 dll,function -> dll

        if (wrapper.Contains("cmd"))
        {
            var match = CmdPattern().Match(args);
            if (match.Success)
                return match.Groups[1].Value;
        }
        else if (wrapper.Contains("powershell") || wrapper.Contains("pwsh"))
        {
            var match = PowerShellPattern().Match(args);
            if (match.Success)
                return match.Groups[1].Value;
        }
        else if (wrapper.Contains("rundll32"))
        {
            var match = Rundll32Pattern().Match(args);
            if (match.Success)
                return match.Groups[1].Value;
        }

        return null;
    }

    [GeneratedRegex(@"/c\s+[""']?([^""']+)[""']?", RegexOptions.IgnoreCase)]
    private static partial Regex CmdPattern();

    [GeneratedRegex(@"-(?:File|Command)\s+[""']?([^""']+)[""']?", RegexOptions.IgnoreCase)]
    private static partial Regex PowerShellPattern();

    [GeneratedRegex(@"([^,\s]+\.dll)", RegexOptions.IgnoreCase)]
    private static partial Regex Rundll32Pattern();
}
