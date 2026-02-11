using Uboot.Core.Models;

namespace Uboot.Core.Interfaces;

/// <summary>
/// Analysis engine interface
/// </summary>
public interface IAnalysisEngine
{
    string EngineVersion { get; }
    Task<Finding> AnalyzeAsync(Entry entry, CancellationToken cancellationToken = default);
    Task<List<Finding>> AnalyzeBatchAsync(List<Entry> entries, CancellationToken cancellationToken = default);
}
