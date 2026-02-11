using Uboot.Core.Interfaces;
using Uboot.Core.Models;
using Serilog;

namespace Uboot.Analysis;

/// <summary>
/// Conservative analysis engine - Unknown-first approach
/// Rules: PathAnomaly, UnsignedBinary, RareLocation, LOLBINUsage, SnapshotDiff, SuspiciousArgument
/// NO evidence => Unknown. Confidence capped at 0.95.
/// </summary>
public sealed class AnalysisEngine : IAnalysisEngine
{
    public string EngineVersion => "1.0.0";

    private static readonly string[] TrustedPaths = new[]
    {
        @"C:\Windows\System32",
        @"C:\Windows\SysWOW64",
        @"C:\Program Files",
        @"C:\Program Files (x86)"
    };

    private static readonly string[] SuspiciousPaths = new[]
    {
        @"C:\Users\Public",
        @"C:\ProgramData",
        @"C:\Temp",
        @"C:\Windows\Temp",
        @"%TEMP%",
        @"%APPDATA%\Local\Temp"
    };

    private static readonly string[] LOLBins = new[]
    {
        "powershell.exe", "cmd.exe", "wscript.exe", "cscript.exe",
        "rundll32.exe", "regsvr32.exe", "mshta.exe", "certutil.exe",
        "bitsadmin.exe", "msiexec.exe", "installutil.exe"
    };

    private static readonly string[] SuspiciousArgs = new[]
    {
        "-enc", "-encodedcommand", "bypass", "hidden",
        "downloadstring", "downloadfile", "invoke-expression",
        "iex", "start-process", "net user", "net localgroup"
    };

    public Task<Finding> AnalyzeAsync(Entry entry, CancellationToken cancellationToken = default)
    {
        var evidence = new List<Evidence>();

        // Rule 1: File not found
        if (entry.FileExists == false)
        {
            evidence.Add(new Evidence
            {
                RuleId = "FILE_NOT_FOUND",
                Description = "Referenced file does not exist",
                Weight = 0.4
            });
        }

        // Rule 2: Unsigned binary
        if (entry.IsSigned == false && entry.ResolvedPath != null)
        {
            var isTrusted = TrustedPaths.Any(tp => entry.ResolvedPath.StartsWith(tp, StringComparison.OrdinalIgnoreCase));
            if (!isTrusted)
            {
                evidence.Add(new Evidence
                {
                    RuleId = "UNSIGNED_BINARY",
                    Description = "Binary is not digitally signed",
                    Weight = 0.3
                });
            }
        }

        // Rule 3: Invalid signature
        if (entry.IsSigned == true && entry.SignatureValid == false)
        {
            evidence.Add(new Evidence
            {
                RuleId = "INVALID_SIGNATURE",
                Description = "Digital signature is invalid",
                Weight = 0.6
            });
        }

        // Rule 4: Rare/suspicious location
        if (entry.ResolvedPath != null)
        {
            var inSuspiciousPath = SuspiciousPaths.Any(sp =>
                entry.ResolvedPath.IndexOf(sp, StringComparison.OrdinalIgnoreCase) >= 0);

            if (inSuspiciousPath)
            {
                evidence.Add(new Evidence
                {
                    RuleId = "RARE_LOCATION",
                    Description = "Binary located in uncommon directory",
                    Weight = 0.5
                });
            }
        }

        // Rule 5: LOLBIN usage
        if (entry.WrapperDetected != null)
        {
            var isLolbin = LOLBins.Any(lol =>
                entry.WrapperDetected.Contains(lol, StringComparison.OrdinalIgnoreCase));

            if (isLolbin)
            {
                evidence.Add(new Evidence
                {
                    RuleId = "LOLBIN_USAGE",
                    Description = $"Uses living-off-the-land binary: {entry.WrapperDetected}",
                    Weight = 0.4
                });
            }
        }

        // Rule 6: Suspicious arguments
        if (entry.Arguments != null || entry.Command != null)
        {
            var argsToCheck = (entry.Arguments ?? entry.Command).ToLowerInvariant();
            var foundSuspicious = SuspiciousArgs.Where(sa => argsToCheck.Contains(sa)).ToList();

            if (foundSuspicious.Count > 0)
            {
                evidence.Add(new Evidence
                {
                    RuleId = "SUSPICIOUS_ARGUMENT",
                    Description = $"Suspicious arguments detected: {string.Join(", ", foundSuspicious)}",
                    Weight = 0.5
                });
            }
        }

        // Rule 7: Snapshot diff - Added entry
        if (entry.DiffState == DiffState.Added)
        {
            evidence.Add(new Evidence
            {
                RuleId = "SNAPSHOT_DIFF_ADDED",
                Description = "Entry was added since baseline",
                Weight = 0.3
            });
        }

        // Rule 8: Known good indicators (negative weight)
        if (entry.IsSigned == true && entry.SignatureValid == true)
        {
            var trustedPublishers = new[] { "Microsoft Corporation", "Microsoft Windows" };
            if (entry.SignerSubject != null && trustedPublishers.Any(tp => entry.SignerSubject.Contains(tp)))
            {
                evidence.Add(new Evidence
                {
                    RuleId = "TRUSTED_PUBLISHER",
                    Description = "Signed by trusted publisher",
                    Weight = -0.6
                });
            }
        }

        if (entry.ResolvedPath != null && TrustedPaths.Any(tp => entry.ResolvedPath.StartsWith(tp, StringComparison.OrdinalIgnoreCase)))
        {
            evidence.Add(new Evidence
            {
                RuleId = "TRUSTED_LOCATION",
                Description = "Located in trusted system directory",
                Weight = -0.4
            });
        }

        // Compute classification and confidence
        var (classification, confidence) = ComputeClassification(evidence);

        // Generate suggested actions
        var actions = GenerateSuggestedActions(entry, classification);

        var finding = new Finding
        {
            EngineVersion = EngineVersion,
            Classification = classification,
            Confidence = Math.Min(confidence, 0.95), // Cap at 0.95
            Evidence = evidence,
            SuggestedActions = actions,
            AnalyzedAt = DateTimeOffset.Now
        };

        return Task.FromResult(finding);
    }

    public async Task<List<Finding>> AnalyzeBatchAsync(List<Entry> entries, CancellationToken cancellationToken = default)
    {
        var findings = new List<Finding>();
        foreach (var entry in entries)
        {
            var finding = await AnalyzeAsync(entry, cancellationToken);
            findings.Add(finding);
        }
        return findings;
    }

    private (Classification Classification, double Confidence) ComputeClassification(List<Evidence> evidence)
    {
        if (evidence.Count == 0)
            return (Classification.Unknown, 0.5);

        var totalWeight = evidence.Sum(e => e.Weight);
        var confidence = Math.Abs(totalWeight) * 0.5 + 0.3; // Base confidence

        Classification classification;
        if (totalWeight < -0.5)
            classification = Classification.Benign;
        else if (totalWeight < -0.2)
            classification = Classification.LikelyBenign;
        else if (totalWeight < 0.5)
            classification = Classification.Unknown;
        else if (totalWeight < 1.0)
            classification = Classification.Suspicious;
        else
            classification = Classification.NeedsReview;

        return (classification, Math.Min(confidence, 0.95));
    }

    private List<SuggestedAction>? GenerateSuggestedActions(Entry entry, Classification classification)
    {
        if (classification is Classification.Benign or Classification.LikelyBenign)
            return null;

        var actions = new List<SuggestedAction>();

        // Disable/remove based on source
        switch (entry.Source)
        {
            case EntrySource.RunRegistry:
            case EntrySource.RunOnceRegistry:
                if (classification >= Classification.Suspicious)
                {
                    actions.Add(new SuggestedAction
                    {
                        ActionType = ActionType.DisableRegistryValue,
                        Description = $"Disable registry autorun entry: {entry.EntryName}",
                        SafeToExecute = true,
                        RequiresConfirmation = true
                    });
                }
                break;

            case EntrySource.Service:
                if (classification >= Classification.Suspicious)
                {
                    actions.Add(new SuggestedAction
                    {
                        ActionType = ActionType.DisableService,
                        Description = $"Disable service: {entry.EntryName}",
                        SafeToExecute = true,
                        RequiresConfirmation = true
                    });
                }
                break;

            case EntrySource.ScheduledTask:
                if (classification >= Classification.Suspicious)
                {
                    actions.Add(new SuggestedAction
                    {
                        ActionType = ActionType.DisableScheduledTask,
                        Description = $"Disable scheduled task: {entry.EntryName}",
                        SafeToExecute = true,
                        RequiresConfirmation = true
                    });
                }
                break;

            case EntrySource.StartupFolder:
                if (classification >= Classification.NeedsReview)
                {
                    actions.Add(new SuggestedAction
                    {
                        ActionType = ActionType.QuarantineFile,
                        Description = $"Move to quarantine: {entry.EntryName}",
                        SafeToExecute = true,
                        RequiresConfirmation = true
                    });
                }
                break;

            case EntrySource.WmiEventConsumer:
            case EntrySource.WinlogonShell:
            case EntrySource.WinlogonUserinit:
            case EntrySource.IfeoDebugger:
                // Manual review only for these dangerous sources
                actions.Add(new SuggestedAction
                {
                    ActionType = ActionType.ManualReview,
                    Description = $"Manual review required for {entry.Source}",
                    SafeToExecute = false,
                    RequiresConfirmation = false
                });
                break;
        }

        return actions.Count > 0 ? actions : null;
    }
}
