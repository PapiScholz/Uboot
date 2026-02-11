using System;
using Avalonia;
using Microsoft.Extensions.DependencyInjection;
using Serilog;
using Uboot.Core.Interfaces;
using Uboot.Core.Services;
using Uboot.Collectors.Windows;
using Uboot.Collectors.Windows.Collectors;
using Uboot.Analysis;
using Uboot.Online;
using Uboot.App.ViewModels;

namespace Uboot.App;

internal sealed class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
        // Configure Serilog
        Log.Logger = new LoggerConfiguration()
            .MinimumLevel.Information()
            .WriteTo.File(
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "Uboot", "logs", "app-.log"),
                rollingInterval: RollingInterval.Day,
                retainedFileCountLimit: 7)
            .WriteTo.Console()
            .CreateLogger();

        try
        {
            // Platform gate check
            var platformGate = new WindowsPlatformGate();
            var platformCheck = platformGate.CheckPlatform();

            if (!platformCheck.IsSupported || !platformCheck.IsAdmin)
            {
                Log.Fatal("Platform check failed: {Error}", platformCheck.ErrorMessage);
                Console.WriteLine($"ERROR: {platformCheck.ErrorMessage}");
                Console.WriteLine("\nPress any key to exit...");
                Console.ReadKey();
                Environment.Exit(1);
            }

            Log.Information("Platform check passed: Windows {Version} (Build {Build}), Admin: {IsAdmin}",
                platformCheck.OsVersion, platformCheck.OsBuild, platformCheck.IsAdmin);

            BuildAvaloniaApp()
                .StartWithClassicDesktopLifetime(args);
        }
        catch (Exception ex)
        {
            Log.Fatal(ex, "Application terminated unexpectedly");
        }
        finally
        {
            Log.CloseAndFlush();
        }
    }

    public static AppBuilder BuildAvaloniaApp()
    {
        // Setup DI container
        var services = new ServiceCollection();
        ConfigureServices(services);
        var serviceProvider = services.BuildServiceProvider();

        return AppBuilder.Configure(() => new App(serviceProvider))
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace();
    }

    private static void ConfigureServices(IServiceCollection services)
    {
        // Core services
        services.AddSingleton<IPlatformGate, WindowsPlatformGate>();
        services.AddSingleton<IHashService, HashService>();
        services.AddSingleton<ISignatureService, SignatureService>();
        services.AddSingleton<ISnapshotRepository, SnapshotRepository>();
        services.AddSingleton<IExportService, ExportService>();
        services.AddSingleton<IAnalysisEngine, AnalysisEngine>();

        // Collectors
        services.AddTransient<ICollector, RunRegistryCollector>();
        services.AddTransient<ICollector, StartupFolderCollector>();
        services.AddTransient<ICollector, ServicesCollector>();

        // Online services
        services.AddSingleton<VirusTotalService>();
        services.AddSingleton<WhoisService>();
        services.AddSingleton<DnsService>();

        // ViewModels
        services.AddTransient<MainWindowViewModel>();
    }
}
