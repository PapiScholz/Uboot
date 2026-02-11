using System.Text;
using System.Text.Json;
using Uboot.Core.Interfaces;
using Uboot.Core.Models;

namespace Uboot.Core.Services;

/// <summary>
/// Export service for JSON/CSV/Markdown reports
/// </summary>
public sealed class ExportService : IExportService
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase
    };

    public async Task ExportJsonAsync(string filePath, object data, CancellationToken cancellationToken = default)
    {
        var json = JsonSerializer.Serialize(data, JsonOptions);
        await File.WriteAllTextAsync(filePath, json, cancellationToken);
    }

    public async Task ExportCsvAsync(string filePath, object data, CancellationToken cancellationToken = default)
    {
        var csv = new StringBuilder();

        if (data is List<Entry> entries)
        {
            // CSV header
            csv.AppendLine("Source,Scope,EntryName,Command,ResolvedPath,MD5,SHA256,IsSigned,Classification,Confidence,DiffState");

            foreach (var entry in entries)
            {
                csv.AppendLine(string.Join(",",
                    Escape(entry.Source.ToString()),
                    Escape(entry.Scope.ToString()),
                    Escape(entry.EntryName),
                    Escape(entry.Command),
                    Escape(entry.ResolvedPath ?? ""),
                    Escape(entry.MD5 ?? ""),
                    Escape(entry.SHA256 ?? ""),
                    entry.IsSigned?.ToString() ?? "",
                    Escape(entry.Finding?.Classification.ToString() ?? "Unknown"),
                    entry.Finding?.Confidence.ToString("F2") ?? "",
                    Escape(entry.DiffState.ToString())
                ));
            }
        }
        else if (data is SnapshotDiff diff)
        {
            csv.AppendLine($"# Diff: {diff.BaselineName} vs {diff.CurrentName}");
            csv.AppendLine($"# Created: {diff.CreatedAt:yyyy-MM-dd HH:mm:ss}");
            csv.AppendLine($"# Added: {diff.Summary.TotalAdded}, Removed: {diff.Summary.TotalRemoved}, Modified: {diff.Summary.TotalModified}");
            csv.AppendLine();
            csv.AppendLine("DiffState,Source,Scope,EntryName,Command,FieldsChanged");

            foreach (var entry in diff.Added.Concat(diff.Removed).Concat(diff.Modified))
            {
                csv.AppendLine(string.Join(",",
                    Escape(entry.DiffState.ToString()),
                    Escape(entry.Source.ToString()),
                    Escape(entry.Scope.ToString()),
                    Escape(entry.EntryName),
                    Escape(entry.Command),
                    Escape(entry.FieldsChanged != null ? string.Join(";", entry.FieldsChanged) : "")
                ));
            }
        }

        await File.WriteAllTextAsync(filePath, csv.ToString(), cancellationToken);
    }

    public async Task ExportMarkdownAsync(string filePath, object data, CancellationToken cancellationToken = default)
    {
        var md = new StringBuilder();

        if (data is List<Entry> entries)
        {
            md.AppendLine("# Autoruns Scan Report");
            md.AppendLine($"Generated: {DateTimeOffset.Now:yyyy-MM-dd HH:mm:ss}");
            md.AppendLine();
            md.AppendLine($"**Total Entries:** {entries.Count}");
            md.AppendLine();

            var grouped = entries.GroupBy(e => e.Finding?.Classification ?? Classification.Unknown);
            foreach (var group in grouped.OrderBy(g => g.Key))
            {
                md.AppendLine($"## {group.Key} ({group.Count()})");
                md.AppendLine();
                md.AppendLine("| Source | Entry Name | Command | Signed | Confidence |");
                md.AppendLine("|--------|-----------|---------|--------|-----------|");

                foreach (var entry in group.Take(50)) // Limit for readability
                {
                    md.AppendLine($"| {entry.Source} | {EscapeMd(entry.EntryName)} | {EscapeMd(Truncate(entry.Command, 50))} | {entry.IsSigned?.ToString() ?? "?"} | {entry.Finding?.Confidence:F2} |");
                }

                if (group.Count() > 50)
                    md.AppendLine($"\n*... and {group.Count() - 50} more*\n");

                md.AppendLine();
            }
        }
        else if (data is SnapshotDiff diff)
        {
            md.AppendLine("# Snapshot Diff Report");
            md.AppendLine($"**Baseline:** {diff.BaselineName}");
            md.AppendLine($"**Current:** {diff.CurrentName}");
            md.AppendLine($"**Generated:** {diff.CreatedAt:yyyy-MM-dd HH:mm:ss}");
            md.AppendLine();
            md.AppendLine("## Summary");
            md.AppendLine($"- **Added:** {diff.Summary.TotalAdded}");
            md.AppendLine($"- **Removed:** {diff.Summary.TotalRemoved}");
            md.AppendLine($"- **Modified:** {diff.Summary.TotalModified}");
            md.AppendLine($"- **Unchanged:** {diff.Summary.TotalUnchanged}");
            md.AppendLine();

            if (diff.Added.Count > 0)
            {
                md.AppendLine("## Added Entries");
                md.AppendLine("| Source | Entry Name | Command |");
                md.AppendLine("|--------|-----------|---------|");
                foreach (var entry in diff.Added.Take(30))
                {
                    md.AppendLine($"| {entry.Source} | {EscapeMd(entry.EntryName)} | {EscapeMd(Truncate(entry.Command, 60))} |");
                }
                md.AppendLine();
            }

            if (diff.Removed.Count > 0)
            {
                md.AppendLine("## Removed Entries");
                md.AppendLine("| Source | Entry Name | Command |");
                md.AppendLine("|--------|-----------|---------|");
                foreach (var entry in diff.Removed.Take(30))
                {
                    md.AppendLine($"| {entry.Source} | {EscapeMd(entry.EntryName)} | {EscapeMd(Truncate(entry.Command, 60))} |");
                }
                md.AppendLine();
            }

            if (diff.Modified.Count > 0)
            {
                md.AppendLine("## Modified Entries");
                md.AppendLine("| Source | Entry Name | Fields Changed |");
                md.AppendLine("|--------|-----------|---------------|");
                foreach (var entry in diff.Modified.Take(30))
                {
                    md.AppendLine($"| {entry.Source} | {EscapeMd(entry.EntryName)} | {string.Join(", ", entry.FieldsChanged ?? new List<string>())} |");
                }
                md.AppendLine();
            }
        }

        await File.WriteAllTextAsync(filePath, md.ToString(), cancellationToken);
    }

    private string Escape(string? value) => 
        value == null ? "" : $"\"{value.Replace("\"", "\"\"")}\"";

    private string EscapeMd(string? value) =>
        value?.Replace("|", "\\|").Replace("\n", " ").Replace("\r", "") ?? "";

    private string Truncate(string value, int maxLength) =>
        value.Length <= maxLength ? value : value.Substring(0, maxLength - 3) + "...";
}
