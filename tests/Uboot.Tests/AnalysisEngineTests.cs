using Uboot.Core.Models;
using Uboot.Analysis;
using Xunit;

namespace Uboot.Tests;

public class AnalysisEngineTests
{
    [Fact]
    public async Task AnalyzeAsync_UnsignedBinary_ReturnsSuspicious()
    {
        // Arrange
        var engine = new AnalysisEngine();
        var entry = new Entry
        {
            Id = "test-1",
            Source = EntrySource.RunRegistry,
            Scope = EntryScope.User,
            Location = "Test",
            EntryName = "TestApp",
            Command = @"C:\Users\Test\suspicious.exe",
            ResolvedPath = @"C:\Users\Test\suspicious.exe",
            IsSigned = false, // Unsigned
            FileExists = true,
            CollectedAt = DateTimeOffset.Now
        };

        // Act
        var finding = await engine.AnalyzeAsync(entry);

        // Assert
        Assert.NotNull(finding);
        Assert.True(finding.Classification >= Classification.Unknown);
        Assert.Contains(finding.Evidence, e => e.RuleId == "UNSIGNED_BINARY");
        Assert.InRange(finding.Confidence, 0.0, 0.95);
    }

    [Fact]
    public async Task AnalyzeAsync_TrustedMicrosoft_ReturnsBenign()
    {
        // Arrange
        var engine = new AnalysisEngine();
        var entry = new Entry
        {
            Id = "test-2",
            Source = EntrySource.Service,
            Scope = EntryScope.System,
            Location = "Test",
            EntryName = "Windows Defender",
            Command = @"C:\Program Files\Windows Defender\MsMpEng.exe",
            ResolvedPath = @"C:\Program Files\Windows Defender\MsMpEng.exe",
            IsSigned = true,
            SignatureValid = true,
            SignerSubject = "CN=Microsoft Corporation",
            FileExists = true,
            CollectedAt = DateTimeOffset.Now
        };

        // Act
        var finding = await engine.AnalyzeAsync(entry);

        // Assert
        Assert.NotNull(finding);
        Assert.Contains(finding.Evidence, e => e.RuleId == "TRUSTED_PUBLISHER");
        Assert.Contains(finding.Evidence, e => e.Weight < 0); // Negative weight for trusted
    }

    [Fact]
    public async Task AnalyzeAsync_NoEvidence_ReturnsUnknown()
    {
        // Arrange
        var engine = new AnalysisEngine();
        var entry = new Entry
        {
            Id = "test-3",
            Source = EntrySource.RunRegistry,
            Scope = EntryScope.User,
            Location = "Test",
            EntryName = "NeutralApp",
            Command = "app.exe",
            CollectedAt = DateTimeOffset.Now
        };

        // Act
        var finding = await engine.AnalyzeAsync(entry);

        // Assert
        Assert.NotNull(finding);
        Assert.Equal(Classification.Unknown, finding.Classification);
        Assert.Equal("1.0.0", finding.EngineVersion);
    }

    [Fact]
    public async Task AnalyzeAsync_ConfidenceNeverExceedsMax()
    {
        // Arrange
        var engine = new AnalysisEngine();
        var entry = new Entry
        {
            Id = "test-4",
            Source = EntrySource.RunRegistry,
            Scope = EntryScope.User,
            Location = @"C:\Users\Public",
            EntryName = "Evil",
            Command = @"C:\Users\Public\malware.exe -enc AAECAw==",
            ResolvedPath = @"C:\Users\Public\malware.exe",
            Arguments = "-enc AAECAw==",
            IsSigned = false,
            SignatureValid = false,
            FileExists = true,
            WrapperDetected = "powershell.exe",
            DiffState = DiffState.Added,
            CollectedAt = DateTimeOffset.Now
        };

        // Act
        var finding = await engine.AnalyzeAsync(entry);

        // Assert
        Assert.InRange(finding.Confidence, 0.0, 0.95);
        Assert.True(finding.Confidence <= 0.95, "Confidence should never exceed 0.95");
    }
}
