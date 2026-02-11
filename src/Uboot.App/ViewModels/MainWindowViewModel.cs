using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reactive;
using System.Threading.Tasks;
using Uboot.Core.Interfaces;
using Uboot.Core.Models;
using ReactiveUI;
using Serilog;

namespace Uboot.App.ViewModels;

public sealed class MainWindowViewModel : ReactiveObject
{
    private readonly IEnumerable<ICollector> _collectors;
    private readonly IAnalysisEngine _analysisEngine;
    private readonly ISnapshotRepository _snapshotRepository;
    private readonly IExportService _exportService;

    private ObservableCollection<Entry> _entries = new();
    private Entry? _selectedEntry;
    private bool _isScanning;
    private bool _onlineEnabled;
    private bool _includeSha512;
    private string _statusText = "Ready";
    private string _currentLanguage = "EN";

    public ObservableCollection<Entry> Entries
    {
        get => _entries;
        set => this.RaiseAndSetIfChanged(ref _entries, value);
    }

    public Entry? SelectedEntry
    {
        get => _selectedEntry;
        set => this.RaiseAndSetIfChanged(ref _selectedEntry, value);
    }

    public bool IsScanning
    {
        get => _isScanning;
        set => this.RaiseAndSetIfChanged(ref _isScanning, value);
    }

    public bool OnlineEnabled
    {
        get => _onlineEnabled;
        set => this.RaiseAndSetIfChanged(ref _onlineEnabled, value);
    }

    public bool IncludeSha512
    {
        get => _includeSha512;
        set => this.RaiseAndSetIfChanged(ref _includeSha512, value);
    }

    public string StatusText
    {
        get => _statusText;
        set => this.RaiseAndSetIfChanged(ref _statusText, value);
    }

    public string CurrentLanguage
    {
        get => _currentLanguage;
        set => this.RaiseAndSetIfChanged(ref _currentLanguage, value);
    }

    public ReactiveCommand<Unit, Unit> ScanCommand { get; }
    public ReactiveCommand<Unit, Unit> ExportJsonCommand { get; }
    public ReactiveCommand<Unit, Unit> ExportCsvCommand { get; }
    public ReactiveCommand<Unit, Unit> ExportMarkdownCommand { get; }
    public ReactiveCommand<Unit, Unit> CreateBaselineCommand { get; }
    public ReactiveCommand<Unit, Unit> ToggleLanguageCommand { get; }

    public MainWindowViewModel(
        IEnumerable<ICollector> collectors,
        IAnalysisEngine analysisEngine,
        ISnapshotRepository snapshotRepository,
        IExportService exportService)
    {
        _collectors = collectors;
        _analysisEngine = analysisEngine;
        _snapshotRepository = snapshotRepository;
        _exportService = exportService;

        ScanCommand = ReactiveCommand.CreateFromTask(ScanAsync);
        ExportJsonCommand = ReactiveCommand.CreateFromTask(ExportJsonAsync);
        ExportCsvCommand = ReactiveCommand.CreateFromTask(ExportCsvAsync);
        ExportMarkdownCommand = ReactiveCommand.CreateFromTask(ExportMarkdownAsync);
        CreateBaselineCommand = ReactiveCommand.CreateFromTask(CreateBaselineAsync);
        ToggleLanguageCommand = ReactiveCommand.Create(ToggleLanguage);
    }

    private async Task ScanAsync()
    {
        IsScanning = true;
        StatusText = CurrentLanguage == "EN" ? "Scanning..." : "Escaneando...";

        try
        {
            var allEntries = new List<Entry>();

            foreach (var collector in _collectors)
            {
                Log.Information("Running collector: {Source}", collector.Source);
                var entries = await collector.CollectAsync();
                allEntries.AddRange(entries);
            }

            Log.Information("Collected {Count} entries, running analysis...", allEntries.Count);

            // Run analysis
            var analyzedEntries = new List<Entry>();
            foreach (var entry in allEntries)
            {
                var finding = await _analysisEngine.AnalyzeAsync(entry);
                analyzedEntries.Add(entry with { Finding = finding });
            }

            Entries = new ObservableCollection<Entry>(analyzedEntries);
            StatusText = CurrentLanguage == "EN" 
                ? $"Scan complete: {allEntries.Count} entries" 
                : $"Escaneo completo: {allEntries.Count} entradas";

            Log.Information("Scan and analysis complete: {Count} entries", allEntries.Count);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Scan failed");
            StatusText = CurrentLanguage == "EN" ? "Scan failed" : "Escaneo fallido";
        }
        finally
        {
            IsScanning = false;
        }
    }

    private async Task ExportJsonAsync()
    {
        if (!Entries.Any())
            return;

        try
        {
            var fileName = $"autoruns_export_{DateTimeOffset.Now:yyyyMMdd_HHmmss}.json";
            var filePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Desktop), fileName);
            await _exportService.ExportJsonAsync(filePath, Entries.ToList());
            StatusText = $"Exported to {fileName}";
            Log.Information("Exported JSON to {Path}", filePath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Export JSON failed");
            StatusText = "Export failed";
        }
    }

    private async Task ExportCsvAsync()
    {
        if (!Entries.Any())
            return;

        try
        {
            var fileName = $"autoruns_export_{DateTimeOffset.Now:yyyyMMdd_HHmmss}.csv";
            var filePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Desktop), fileName);
            await _exportService.ExportCsvAsync(filePath, Entries.ToList());
            StatusText = $"Exported to {fileName}";
            Log.Information("Exported CSV to {Path}", filePath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Export CSV failed");
            StatusText = "Export failed";
        }
    }

    private async Task ExportMarkdownAsync()
    {
        if (!Entries.Any())
            return;

        try
        {
            var fileName = $"autoruns_report_{DateTimeOffset.Now:yyyyMMdd_HHmmss}.md";
            var filePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Desktop), fileName);
            await _exportService.ExportMarkdownAsync(filePath, Entries.ToList());
            StatusText = $"Exported to {fileName}";
            Log.Information("Exported Markdown to {Path}", filePath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Export Markdown failed");
            StatusText = "Export failed";
        }
    }

    private async Task CreateBaselineAsync()
    {
        if (!Entries.Any())
        {
            StatusText = CurrentLanguage == "EN" ? "No entries to save" : "No hay entradas para guardar";
            return;
        }

        try
        {
            var snapshot = new Snapshot
            {
                Id = Guid.NewGuid().ToString(),
                Name = "baseline_clean",
                IsBaseline = true,
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
                Entries = Entries.ToList(),
                EngineVersion = _analysisEngine.EngineVersion
            };

            await _snapshotRepository.SaveAsync(snapshot);
            StatusText = CurrentLanguage == "EN" ? "Baseline created" : "Baseline creado";
            Log.Information("Baseline snapshot created: {Id}", snapshot.Id);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Create baseline failed");
            StatusText = CurrentLanguage == "EN" ? "Baseline creation failed" : "Creación de baseline fallida";
        }
    }

    private void ToggleLanguage()
    {
        CurrentLanguage = CurrentLanguage == "EN" ? "ES" : "EN";
        StatusText = CurrentLanguage == "EN" ? "Ready" : "Listo";
    }
}
