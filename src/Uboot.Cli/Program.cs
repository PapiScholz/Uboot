using System;
using System.CommandLine;
using System.IO;
using Microsoft.Extensions.DependencyInjection;
using Serilog;
using Uboot.Core.Interfaces;
using Uboot.Core.Services;
using Uboot.Core.Models;
using Uboot.Collectors.Windows;
using Uboot.Collectors.Windows.Collectors;
using Uboot.Analysis;

namespace Uboot.Cli;

internal sealed class Program
{
    static async Task<int> Main(string[] args)
    {
        Log.Logger = new LoggerConfiguration()
            .MinimumLevel.Information()
            .WriteTo.Console()
            .CreateLogger();

        var rootCommand = new RootCommand("Uboot CLI - Headless scanner");

        var scanCommand = new Command("scan", "Perform a scan and save snapshot");
        var nameOption = new Option<string>("--name", () => $"scan_{DateTimeOffset.Now:yyyyMMdd_HHmm}", "Snapshot name");
        var outputOption = new Option<string?>("--output", "Export path (JSON/CSV/MD)");
        var baselineOption = new Option<bool>("--baseline", "Save as baseline");
        scanCommand.AddOption(nameOption);
        scanCommand.AddOption(outputOption);
        scanCommand.AddOption(baselineOption);

        scanCommand.SetHandler(async (name, output, baseline) =>
        {
            await ScanAsync(name, output, baseline);
        }, nameOption, outputOption, baselineOption);

        rootCommand.AddCommand(scanCommand);

        return await rootCommand.InvokeAsync(args);
    }

    private static async Task ScanAsync(string name, string? output, bool baseline)
    {
        // Platform check
        var platformGate = new WindowsPlatformGate();
        var check = platformGate.CheckPlatform();

        if (!check.IsSupported || !check.IsAdmin)
        {
            Log.Fatal("Platform check failed: {Error}", check.ErrorMessage);
            return;
        }

        Log.Information("Starting scan: {Name}", name);

        var services = new ServiceCollection();
        ConfigureServices(services);
        var serviceProvider = services.BuildServiceProvider();

        var collectors = serviceProvider.GetServices<ICollector>();
        var analysisEngine = serviceProvider.GetRequiredService<IAnalysisEngine>();
        var snapshotRepo = serviceProvider.GetRequiredService<ISnapshotRepository>();
        var exportService = serviceProvider.GetRequiredService<IExportService>();

        var allEntries = new List<Entry>();

        foreach (var collector in collectors)
        {
            Log.Information("Running {Source}...", collector.Source);
            var entries = await collector.CollectAsync();
            allEntries.AddRange(entries);
        }

        Log.Information("Collected {Count} entries, analyzing...", allEntries.Count);

        var analyzedEntries = new List<Entry>();
        foreach (var entry in allEntries)
        {
            var finding = await analysisEngine.AnalyzeAsync(entry);
            analyzedEntries.Add(entry with { Finding = finding });
        }
        allEntries = analyzedEntries;

        var snapshot = new Snapshot
        {
            Id = Guid.NewGuid().ToString(),
            Name = name,
            IsBaseline = baseline,
            CreatedAt = DateTimeOffset.Now,
            SystemInfo = new SystemInfo
            {
                Hostname = Environment.MachineName,
                OsVersion = Environment.OSVersion.VersionString,
                OsBuild = Environment.OSVersion.Version.Build.ToString(),
                Architecture = Environment.Is64BitOperatingSystem ? "x64" : "x86",
                Username = Environment.UserName,
                IsAdmin = true
            },
            Entries = allEntries,
            EngineVersion = analysisEngine.EngineVersion
        };

        await snapshotRepo.SaveAsync(snapshot);
        Log.Information("Snapshot saved: {Id}", snapshot.Id);

        if (!string.IsNullOrEmpty(output))
        {
            var ext = Path.GetExtension(output).ToLowerInvariant();
            if (ext == ".json")
                await exportService.ExportJsonAsync(output, allEntries);
            else if (ext == ".csv")
                await exportService.ExportCsvAsync(output, allEntries);
            else if (ext == ".md")
                await exportService.ExportMarkdownAsync(output, allEntries);

            Log.Information("Exported to {Output}", output);
        }

        Log.Information("Scan complete!");
    }

    private static void ConfigureServices(IServiceCollection services)
    {
        services.AddSingleton<IPlatformGate, WindowsPlatformGate>();
        services.AddSingleton<IHashService, HashService>();
        services.AddSingleton<ISignatureService, SignatureService>();
        services.AddSingleton<ISnapshotRepository, SnapshotRepository>();
        services.AddSingleton<IExportService, ExportService>();
        services.AddSingleton<IAnalysisEngine, AnalysisEngine>();

        services.AddTransient<ICollector, RunRegistryCollector>();
        services.AddTransient<ICollector, StartupFolderCollector>();
        services.AddTransient<ICollector, ServicesCollector>();
    }
}
