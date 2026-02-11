using Uboot.Core.Models;

namespace Uboot.Core.Interfaces;

/// <summary>
/// Snapshot storage and management
/// </summary>
public interface ISnapshotRepository
{
    Task<Snapshot> SaveAsync(Snapshot snapshot, CancellationToken cancellationToken = default);
    Task<Snapshot?> GetAsync(string id, CancellationToken cancellationToken = default);
    Task<Snapshot?> GetByNameAsync(string name, CancellationToken cancellationToken = default);
    Task<List<Snapshot>> ListAsync(bool baselinesOnly = false, CancellationToken cancellationToken = default);
    Task<bool> DeleteAsync(string id, CancellationToken cancellationToken = default);
    Task<SnapshotDiff> ComputeDiffAsync(string baselineId, string currentId, CancellationToken cancellationToken = default);
}
