# OSS Trust Policy

This policy documents project intent and release constraints used for open-source code-signing eligibility.

## Project intent

Uboot is a defensive Windows persistence auditor for visibility, evidence collection, and reversible remediation planning.

The project is not intended to provide stealth, evasion, payload delivery, persistence implanting, or offensive malware tooling.

## Distribution and user trust

- Public source repository on GitHub.
- OSS license in root (`MIT`).
- Release artifact naming is stable and product-branded (`Uboot-Setup-<version>.exe`).
- Version string is kept consistent across app, CMake, and installer metadata.
- Signed release artifacts are required for distribution.

## Build reproducibility controls

- CI validates version and branding consistency.
- CI validates repository visibility (public) in GitHub context.
- CI runs smoke tests and builds `uboot-core.exe` on Windows.

## Non-deceptive behavior

Uboot must present transparent UX and explicit user-confirmed remediation actions.

The project must not:

- misrepresent files as trustworthy
- hide remediation side effects
- execute destructive changes without explicit user action
