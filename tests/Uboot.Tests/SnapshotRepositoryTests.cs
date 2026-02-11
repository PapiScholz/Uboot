using Uboot.Core.Models;
using Uboot.Core.Services;
using Xunit;

namespace Uboot.Tests;

public class SnapshotRepositoryTests
{
    [Fact]
    public async Task SaveAndGet_Snapshot_Success()
    {
        // Arrange
        var tempDir = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString());
        var repo = new SnapshotRepository(tempDir);

        var snapshot = new Snapshot
        {
            Id = Guid.NewGuid().ToString(),
            Name = "test_snapshot",
            IsBaseline = false,
            CreatedAt = DateTimeOffset.Now,
            SystemInfo = new SystemInfo
            {
                Hostname = "test-host",
                OsVersion = "Windows 10",
                OsBuild = "19045",
                Architecture = "x64",
                Username = "test-user",
                IsAdmin = true
            },
            Entries = new List<Entry>
            {
                new Entry
                {
                    Id = "test-entry-1",
                    Source = EntrySource.RunRegistry,
                    Scope = EntryScope.System,
                    Location = @"HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run",
                    EntryName = "TestApp",
                    Command = "C:\\test\\app.exe",
                    CollectedAt = DateTimeOffset.Now
                }
            },
            EngineVersion = "1.0.0"
        };

        // Act
        await repo.SaveAsync(snapshot);
        var retrieved = await repo.GetAsync(snapshot.Id);

        // Assert
        Assert.NotNull(retrieved);
        Assert.Equal(snapshot.Id, retrieved.Id);
        Assert.Equal(snapshot.Name, retrieved.Name);
        Assert.Single(retrieved.Entries);

        // Cleanup
        Directory.Delete(tempDir, true);
    }

    [Fact]
    public async Task ComputeDiff_DetectsChanges()
    {
        // Arrange
        var tempDir = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString());
        var repo = new SnapshotRepository(tempDir);

        var baseline = new Snapshot
        {
            Id = "baseline-1",
            Name = "baseline",
            IsBaseline = true,
            CreatedAt = DateTimeOffset.Now,
            SystemInfo = CreateTestSystemInfo(),
            Entries = new List<Entry>
            {
                new Entry
                {
                    Id = "entry-1",
                    Source = EntrySource.RunRegistry,
                    Scope = EntryScope.System,
                    Location = "Test",
                    EntryName = "App1",
                    Command = "app1.exe",
                    CollectedAt = DateTimeOffset.Now
                }
            },
            EngineVersion = "1.0.0"
        };

        var current = new Snapshot
        {
            Id = "scan-1",
            Name = "current",
            IsBaseline = false,
            CreatedAt = DateTimeOffset.Now,
            SystemInfo = CreateTestSystemInfo(),
            Entries = new List<Entry>
            {
                new Entry
                {
                    Id = "entry-1",
                    Source = EntrySource.RunRegistry,
                    Scope = EntryScope.System,
                    Location = "Test",
                    EntryName = "App1",
                    Command = "app1_modified.exe", // Modified
                    CollectedAt = DateTimeOffset.Now
                },
                new Entry
                {
                    Id = "entry-2",
                    Source = EntrySource.RunRegistry,
                    Scope = EntryScope.System,
                    Location = "Test",
                    EntryName = "App2",
                    Command = "app2.exe", // Added
                    CollectedAt = DateTimeOffset.Now
                }
            },
            EngineVersion = "1.0.0"
        };

        await repo.SaveAsync(baseline);
        await repo.SaveAsync(current);

        // Act
        var diff = await repo.ComputeDiffAsync(baseline.Id, current.Id);

        // Assert
        Assert.Equal(1, diff.Summary.TotalAdded);
        Assert.Equal(0, diff.Summary.TotalRemoved);
        Assert.Equal(1, diff.Summary.TotalModified);

        // Cleanup
        Directory.Delete(tempDir, true);
    }

    private SystemInfo CreateTestSystemInfo() => new SystemInfo
    {
        Hostname = "test-host",
        OsVersion = "Windows 10",
        OsBuild = "19045",
        Architecture = "x64",
        Username = "test-user",
        IsAdmin = true
    };
}
