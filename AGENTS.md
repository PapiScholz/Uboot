# AGENTS

## Execution Checklist for This Repo

- Validate runtime policy first: before deep debugging, run `build-vs18/bin/Release/uboot-core.exe --help` (or equivalent) to detect App Control/WDAC blocking early.
- For remediation CLI, remember argparse global options must go before subcommand: `... --pretty plan ...` not `... plan ... --pretty`.
- Use the correct core binary path after CMake build: `build-vs18/bin/Release/uboot-core.exe`.
- Keep plan dry-run behavior non-destructive: plan must return data with `executed: false` and must not apply changes.
- Propagate operation result to process exit code in Python wrappers: if `applied`/`undone` is false, return non-zero.
- Before deleting ignored files, always run a dry-run cleanup first (`git clean -ndX`) and only remove clearly regenerable artifacts.

## Fast Gate Validation (Phase 3 Remediation)

1. Build core in Release.
2. Run remediation plan command with one entry id.
3. Confirm JSON includes `tx_id`, `operations`, and `executed: false`.
4. Run apply/undo in elevated shell for happy-path verification.
5. Confirm list includes the transaction id.
