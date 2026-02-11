namespace Uboot.Core.Interfaces;

/// <summary>
/// Export service for generating reports
/// </summary>
public interface IExportService
{
    Task ExportJsonAsync(string filePath, object data, CancellationToken cancellationToken = default);
    Task ExportCsvAsync(string filePath, object data, CancellationToken cancellationToken = default);
    Task ExportMarkdownAsync(string filePath, object data, CancellationToken cancellationToken = default);
}
