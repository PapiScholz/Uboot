"""Uboot GUI and orchestration main entry point."""
import sys
from pathlib import Path

from app.orchestrator import Scanner, Scorer, Evidence, Remediation, SnapshotManager


def main():
    """Main orchestration workflow."""
    from app.orchestrator.scoring import ScoredEntry

    # Initialize components
    scanner = Scanner()
    scorer = Scorer()
    evidence_gatherer = Evidence()
    remediator = Remediation()
    snapshot_manager = SnapshotManager()

    try:
        # Phase 1: Scan system
        print("🔍 Scanning system...")
        scan_result = scanner.scan(sources=["all"])
        print(f"   ✓ Found {len(scan_result.entries)} entries, {len(scan_result.errors)} errors")

        # Phase 2: Save snapshot
        snapshot = snapshot_manager.save(scan_result, label="before-scan")
        print(f"💾 Saved snapshot: {snapshot.timestamp}")

        # Phase 3: Score entries
        print("🎯 Scoring entries...")
        scored_entries = scorer.score(scan_result.entries)
        print(f"   ✓ Scored {len(scored_entries)} entries")

        # Report summary
        malicious = sum(1 for e in scored_entries if e.score > 70)
        suspicious = sum(1 for e in scored_entries if 30 < e.score <= 70)
        clean = sum(1 for e in scored_entries if e.score <= 30)

        print(f"\n📊 Results:")
        print(f"   Clean:      {clean} entries (score ≤ 30)")
        print(f"   Suspicious: {suspicious} entries (31-70)")
        print(f"   Malicious:  {malicious} entries (score > 70)")

        # Show top suspicious entries
        if suspicious > 0 or malicious > 0:
            print(f"\n⚠️  High-risk entries:")
            risky = sorted(
                [e for e in scored_entries if e.score > 30],
                key=lambda x: x.score,
                reverse=True
            )[:5]

            for entry in risky:
                print(f"   [{entry.score:3d}] {entry.entry.name}")
                print(f"         → {entry.explanation}")

        print("\n✅ Orchestration complete")

    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
