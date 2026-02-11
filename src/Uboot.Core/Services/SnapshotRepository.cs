using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using Uboot.Core.Interfaces;
using Uboot.Core.Models;

namespace Uboot.Core.Services;

/// <summary>
/// Snapshot repository with JSON.gz storage
/// Default storage: %ProgramData%\Uboot\snapshots\
/// </summary>
public sealed class SnapshotRepository : ISnapshotRepository
{
    private readonly string _storageDirectory;
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = false,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public SnapshotRepository(string? storageDirectory = null)
    {
        _storageDirectory = storageDirectory ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            "Uboot",
            "snapshots"
        );

        Directory.CreateDirectory(_storageDirectory);
    }

    public async Task<Snapshot> SaveAsync(Snapshot snapshot, CancellationToken cancellationToken = default)
    {
        var fileName = snapshot.IsBaseline
            ? $"baseline_{SanitizeName(snapshot.Name)}.json.gz"
            : $"scan_{snapshot.Id}.json.gz";

        var filePath = Path.Combine(_storageDirectory, fileName);

        var json = JsonSerializer.Serialize(snapshot, JsonOptions);
        var bytes = Encoding.UTF8.GetBytes(json);

        await using var fileStream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None);
        await using var gzipStream = new GZipStream(fileStream, CompressionLevel.Optimal);
        await gzipStream.WriteAsync(bytes, cancellationToken);

        return snapshot;
    }

    public async Task<Snapshot?> GetAsync(string id, CancellationToken cancellationToken = default)
    {
        var pattern = $"*{id}*.json.gz";
        var files = Directory.GetFiles(_storageDirectory, pattern);
        
        if (files.Length == 0)
            return null;

        return await LoadSnapshotAsync(files[0], cancellationToken);
    }

    public async Task<Snapshot?> GetByNameAsync(string name, CancellationToken cancellationToken = default)
    {
        var sanitized = SanitizeName(name);
        var pattern = $"*{sanitized}*.json.gz";
        var files = Directory.GetFiles(_storageDirectory, pattern);

        if (files.Length == 0)
            return null;

        return await LoadSnapshotAsync(files[0], cancellationToken);
    }

    public async Task<List<Snapshot>> ListAsync(bool baselinesOnly = false, CancellationToken cancellationToken = default)
    {
        var pattern = baselinesOnly ? "baseline_*.json.gz" : "*.json.gz";
        var files = Directory.GetFiles(_storageDirectory, pattern);

        var snapshots = new List<Snapshot>();
        foreach (var file in files)
        {
            var snapshot = await LoadSnapshotAsync(file, cancellationToken);
            if (snapshot != null)
                snapshots.Add(snapshot);
        }

        return snapshots.OrderByDescending(s => s.CreatedAt).ToList();
    }

    public Task<bool> DeleteAsync(string id, CancellationToken cancellationToken = default)
    {
        var pattern = $"*{id}*.json.gz";
        var files = Directory.GetFiles(_storageDirectory, pattern);

        if (files.Length == 0)
            return Task.FromResult(false);

        File.Delete(files[0]);
        return Task.FromResult(true);
    }

    public async Task<SnapshotDiff> ComputeDiffAsync(string baselineId, string currentId, CancellationToken cancellationToken = default)
    {
        var baseline = await GetAsync(baselineId, cancellationToken) 
            ?? throw new InvalidOperationException($"Baseline {baselineId} not found");
        
        var current = await GetAsync(currentId, cancellationToken)
            ?? throw new InvalidOperationException($"Snapshot {currentId} not found");

        var baselineDict = baseline.Entries.ToDictionary(e => e.Id);
        var currentDict = current.Entries.ToDictionary(e => e.Id);

        var added = new List<Entry>();
        var removed = new List<Entry>();
        var modified = new List<Entry>();

        // Find added and modified
        foreach (var entry in current.Entries)
        {
            if (!baselineDict.TryGetValue(entry.Id, out var baselineEntry))
            {
                added.Add(entry with { DiffState = DiffState.Added });
            }
            else if (HasChanged(baselineEntry, entry, out var fieldsChanged))
            {
                modified.Add(entry with { DiffState = DiffState.Modified, FieldsChanged = fieldsChanged });
            }
        }

        // Find removed
        foreach (var entry in baseline.Entries)
        {
            if (!currentDict.ContainsKey(entry.Id))
            {
                removed.Add(entry with { DiffState = DiffState.Removed });
            }
        }

        var diff = new SnapshotDiff
        {
            DiffId = Guid.NewGuid().ToString(),
            BaselineId = baseline.Id,
            BaselineName = baseline.Name,
            CurrentId = current.Id,
            CurrentName = current.Name,
            CreatedAt = DateTimeOffset.Now,
            Added = added,
            Removed = removed,
            Modified = modified,
            Summary = new DiffSummary
            {
                TotalAdded = added.Count,
                TotalRemoved = removed.Count,
                TotalModified = modified.Count,
                TotalUnchanged = current.Entries.Count - added.Count - modified.Count
            }
        };

        // Save diff
        var diffFileName = $"diff_{SanitizeName(baseline.Name)}_vs_{current.Id}.json.gz";
        var diffPath = Path.Combine(_storageDirectory, diffFileName);
        var json = JsonSerializer.Serialize(diff, JsonOptions);
        var bytes = Encoding.UTF8.GetBytes(json);

        await using var fileStream = new FileStream(diffPath, FileMode.Create, FileAccess.Write, FileShare.None);
        await using var gzipStream = new GZipStream(fileStream, CompressionLevel.Optimal);
        await gzipStream.WriteAsync(bytes, cancellationToken);

        return diff;
    }

    private async Task<Snapshot?> LoadSnapshotAsync(string filePath, CancellationToken cancellationToken)
    {
        try
        {
            await using var fileStream = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
            await using var gzipStream = new GZipStream(fileStream, CompressionMode.Decompress);
            return await JsonSerializer.DeserializeAsync<Snapshot>(gzipStream, JsonOptions, cancellationToken);
        }
        catch
        {
            return null;
        }
    }

    private bool HasChanged(Entry baseline, Entry current, out List<string> fieldsChanged)
    {
        fieldsChanged = new List<string>();

        if (baseline.Command != current.Command) fieldsChanged.Add("command");
        if (baseline.Enabled != current.Enabled) fieldsChanged.Add("enabled");
        if (baseline.MD5 != current.MD5) fieldsChanged.Add("md5");
        if (baseline.SHA256 != current.SHA256) fieldsChanged.Add("sha256");
        if (baseline.IsSigned != current.IsSigned) fieldsChanged.Add("is_signed");
        if (baseline.SignatureValid != current.SignatureValid) fieldsChanged.Add("signature_valid");

        return fieldsChanged.Count > 0;
    }

    private string SanitizeName(string name)
    {
        var invalid = Path.GetInvalidFileNameChars();
        return string.Concat(name.Select(c => invalid.Contains(c) ? '_' : c));
    }
}
