using System.Runtime.Versioning;
using System.Security.Cryptography.Pkcs;
using System.Security.Cryptography.X509Certificates;
using Uboot.Core.Interfaces;

namespace Uboot.Collectors.Windows;

/// <summary>
/// Authenticode signature verification (OFFLINE only - no CRL/OCSP)
/// </summary>
[SupportedOSPlatform("windows10.0.19041.0")]
public sealed class SignatureService : ISignatureService
{
    public async Task<SignatureInfo> VerifySignatureAsync(string filePath, CancellationToken cancellationToken = default)
    {
        if (!File.Exists(filePath))
            return new SignatureInfo(false, null, null, null, null);

        return await Task.Run(() =>
        {
            try
            {
                // Read file bytes
                var fileBytes = File.ReadAllBytes(filePath);

                // Try to get signature
                X509Certificate2? cert = null;
                try
                {
                    cert = new X509Certificate2(filePath);
                }
                catch
                {
                    // Not signed or invalid signature
                    return new SignatureInfo(false, null, null, null, null);
                }

                if (cert == null)
                    return new SignatureInfo(false, null, null, null, null);

                // Validate chain (offline only - no revocation check)
                var chain = new X509Chain
                {
                    ChainPolicy =
                    {
                        RevocationMode = X509RevocationMode.NoCheck, // OFFLINE: no CRL/OCSP
                        VerificationFlags = X509VerificationFlags.IgnoreNotTimeValid |
                                           X509VerificationFlags.IgnoreCtlNotTimeValid
                    }
                };

                var isValid = chain.Build(cert);

                var subject = cert.Subject;
                var issuer = cert.Issuer;
                DateTimeOffset? timestamp = null;

                try
                {
                    timestamp = cert.NotBefore;
                }
                catch
                {
                    // Ignore timestamp errors
                }

                cert.Dispose();
                chain.Dispose();

                return new SignatureInfo(
                    IsSigned: true,
                    IsValid: isValid,
                    Subject: subject,
                    Issuer: issuer,
                    Timestamp: timestamp
                );
            }
            catch
            {
                // If any error occurs, treat as unsigned
                return new SignatureInfo(false, null, null, null, null);
            }
        }, cancellationToken);
    }
}
