using Uboot.Core.Models;

namespace Uboot.Core.Interfaces;

/// <summary>
/// Collector interface - each source implements this
/// </summary>
public interface ICollector
{
    EntrySource Source { get; }
    Task<List<Entry>> CollectAsync(CancellationToken cancellationToken = default);
}
