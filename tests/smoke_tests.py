"""Smoke tests for Uboot Python components."""
import sys
import json
import hashlib
import os
import zipfile
from tempfile import TemporaryDirectory
from pathlib import Path

# Add repo root to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from app.orchestrator.scanner import Entry, ScanResult, CollectorError, Scanner
from app.orchestrator.evidence import EntryEvidence
from app.orchestrator.scoring import Scorer, RiskLevel
from app.orchestrator.snapshot import SnapshotManager, Snapshot
from app.orchestrator.llm_advisor import AdviceValidationError, LlmAdvisor, LlmMode
from app.orchestrator.reporting import ForensicEntryRecord, ForensicReportData, HtmlReportGenerator
from app.settings import AppSettings, SettingsStore
from tests.fixtures.fixtures import (
    CLEAN_ENTRY, SUSPICIOUS_ENTRY, MALICIOUS_ENTRY,
    SCAN_RESULT_FIXTURE, TX_PLAN_FIXTURE
)


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def test_scanner_output_structure():
    """Verify Scanner parses JSON correctly."""
    print("Testing Scanner output structure...")
    
    # Create test entries
    entries = [
        Entry(
            entry_id=CLEAN_ENTRY["entry_id"],
            name=CLEAN_ENTRY["name"],
            command=CLEAN_ENTRY["command"],
            image_path="C:\\Windows\\System32\\svchost.exe",
            source=CLEAN_ENTRY["source"],
            status=CLEAN_ENTRY["status"],
            metadata=CLEAN_ENTRY["metadata"]
        ),
        Entry(
            entry_id=SUSPICIOUS_ENTRY["entry_id"],
            name=SUSPICIOUS_ENTRY["name"],
            command=SUSPICIOUS_ENTRY["command"],
            image_path="C:\\Users\\Admin\\AppData\\Local\\Temp\\mystery.exe",
            source=SUSPICIOUS_ENTRY["source"],
            status=SUSPICIOUS_ENTRY["status"],
            metadata=SUSPICIOUS_ENTRY["metadata"]
        )
    ]
    
    errors = [
        CollectorError(
            collector="WmiPersistenceCollector",
            error="Access denied"
        )
    ]
    
    result = ScanResult(entries=entries, errors=errors, schema_version="1.1")
    
    # Verify structure
    assert result.schema_version == "1.1", "Schema version mismatch"
    assert len(result.entries) == 2, f"Expected 2 entries, got {len(result.entries)}"
    assert len(result.errors) == 1, f"Expected 1 error, got {len(result.errors)}"
    assert result.entries[0].name == CLEAN_ENTRY["name"], "Entry name mismatch"
    
    print("  ✓ Scanner structure verified")


def test_scorer_classifies_correctly():
    """Verify Scorer produces correct classifications."""
    print("Testing Scorer classification...")
    
    # Create entries (without full config, just structure)
    entries = [
        Entry(
            entry_id="test-clean",
            name="Windows Update",
            command="C:\\Windows\\System32\\svchost.exe",
            image_path="C:\\Windows\\System32\\svchost.exe",
            source="services",
            status="running"
        ),
        Entry(
            entry_id="test-malicious",
            name="Unknown Malware",
            command="C:\\Users\\User\\AppData\\Local\\Temp\\evil.exe",
            image_path="C:\\Users\\User\\AppData\\Local\\Temp\\evil.exe",
            source="registry",
            status="active"
        )
    ]
    
    # Score them (will use default rules if available)
    scorer = Scorer()
    scored = scorer.score(entries)
    
    # Verify scoring structure
    assert len(scored) == 2, f"Expected 2 scored entries, got {len(scored)}"
    for scored_entry in scored:
        assert 0 <= scored_entry.score <= 100, f"Score out of range: {scored_entry.score}"
        assert scored_entry.risk_level in RiskLevel, f"Invalid risk level: {scored_entry.risk_level}"
        assert isinstance(scored_entry.explanation, str), "Explanation must be string"
    
    print(f"  ✓ Clean entry score: {scored[0].score} ({scored[0].risk_level.value})")
    print(f"  ✓ Malicious entry score: {scored[1].score} ({scored[1].risk_level.value})")


def test_scanner_parses_core_schema_variant():
    """Verify parser supports real core fields (key/imagePath/displayName)."""
    print("Testing Scanner parsing for core schema variant...")

    payload = {
        "schema_version": "1.1",
        "entries": [
            {
                "source": "RunRegistry",
                "scope": "Machine",
                "key": "Riot Vanguard",
                "location": "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                "arguments": "",
                "imagePath": "C:\\Program Files\\Riot Vanguard\\vgtray.exe",
                "keyName": "Riot Vanguard"
            },
            {
                "source": "Services",
                "scope": "Machine",
                "key": "ADPSvc",
                "arguments": "-k LocalServiceAndNoImpersonation -p",
                "imagePath": "C:\\Windows\\system32\\svchost.exe",
                "displayName": "Aggregated Data Platform Service"
            }
        ],
        "errors": []
    }

    result = Scanner._parse_scan_result(payload)

    assert len(result.entries) == 2
    assert result.entries[0].name == "Riot Vanguard"
    assert result.entries[0].command == "C:\\Program Files\\Riot Vanguard\\vgtray.exe"
    assert result.entries[0].image_path == "C:\\Program Files\\Riot Vanguard\\vgtray.exe"
    assert result.entries[0].entry_id != "unknown"
    assert result.entries[1].name == "Aggregated Data Platform Service"
    assert "svchost.exe" in result.entries[1].command
    assert "LocalServiceAndNoImpersonation" in result.entries[1].command

    print("  ✓ Core schema variant parsed correctly")


def test_evidence_metadata_updates():
    """Verify evidence maps Authenticode results into scoring metadata."""
    print("Testing evidence metadata mapping...")

    evidence = EntryEvidence(
        entry_id="test-entry",
        file_info={"exists": True},
        authenticode={
            "signed": True,
            "status": "Valid",
            "signer_subject": "CN=Microsoft Windows, O=Microsoft Corporation"
        }
    )

    updates = evidence.to_metadata_updates()

    assert updates["file_exists"] is True
    assert updates["target_exists"] is True
    assert updates["signed"] is True
    assert updates["signature_status"] == "Valid"
    assert updates["signature_valid"] is True
    assert "Microsoft Corporation" in updates["publisher"]

    print("  ✓ Evidence metadata mapping verified")


def test_snapshot_persistence():
    """Verify SnapshotManager saves and loads snapshots."""
    print("Testing Snapshot persistence...")
    
    manager = SnapshotManager(Path("tests/fixtures/snapshots"))
    
    # Create test scan result
    entries = [
        Entry(
            entry_id="test-entry",
            name="Test",
            command="test.exe",
            image_path="test.exe",
            source="registry",
            status="active"
        )
    ]
    
    scan_result = ScanResult(entries=entries)
    
    # Save snapshot
    snapshot = manager.save(scan_result, label="test")
    assert snapshot.entry_count == 1, f"Expected 1 entry, got {snapshot.entry_count}"
    assert Path(manager.snapshot_dir).exists(), "Snapshot dir not created"
    
    # Verify file exists
    snapshot_files = list(Path(manager.snapshot_dir).glob("snapshot_*.json"))
    assert len(snapshot_files) > 0, "No snapshot files created"
    
    print(f"  ✓ Snapshot saved: {snapshot.timestamp}")
    print(f"  ✓ Snapshot files: {len(snapshot_files)}")


def test_snapshot_diff_detection():
    """Verify SnapshotManager detects changes correctly."""
    print("Testing Snapshot diff detection...")
    
    manager = SnapshotManager(Path("tests/fixtures/snapshots"))
    
    # Create first snapshot (entry1, entry2)
    snapshot1 = Snapshot(
        timestamp="2026-04-13T10:00:00",
        entries=[
            Entry(entry_id="e1", name="Entry1", command="cmd1.exe", image_path="cmd1.exe", source="registry", status="active"),
            Entry(entry_id="e2", name="Entry2", command="cmd2.exe", image_path="cmd2.exe", source="service", status="active"),
        ],
        entry_count=2
    )
    
    # Create second snapshot (entry2 modified, entry3 added, entry1 removed)
    snapshot2 = Snapshot(
        timestamp="2026-04-13T11:00:00",
        entries=[
            Entry(entry_id="e2", name="Entry2", command="cmd2_modified.exe", image_path="cmd2_modified.exe", source="service", status="disabled"),
            Entry(entry_id="e3", name="Entry3", command="cmd3.exe", image_path="cmd3.exe", source="task", status="active"),
        ],
        entry_count=2
    )
    
    # Compute diff (without loading from disk, use provided previous snapshot)
    diff = manager.diff(snapshot2, snapshot1)
    
    # Verify diff results
    assert len(diff.new_entries) == 1, f"Expected 1 new entry, got {len(diff.new_entries)}"
    assert len(diff.removed_entries) == 1, f"Expected 1 removed entry, got {len(diff.removed_entries)}"
    assert len(diff.changed_entries) == 1, f"Expected 1 changed entry, got {len(diff.changed_entries)}"
    assert "command" in diff.changed_entries[0].changed_fields, "Changed command not detected"
    assert "status" in diff.changed_entries[0].changed_fields, "Changed status not detected"
    assert diff.unchanged_entry_count == 0, f"Expected 0 unchanged entries, got {diff.unchanged_entry_count}"
    
    print(f"  ✓ New entries: {len(diff.new_entries)}")
    print(f"  ✓ Removed entries: {len(diff.removed_entries)}")
    print(f"  ✓ Changed entries: {len(diff.changed_entries)}")
    print(f"  ✓ Unchanged entries: {diff.unchanged_entry_count}")


def test_snapshot_listing_and_loading():
    """Verify snapshot summaries can be listed and loaded back."""
    print("Testing snapshot listing...")

    with TemporaryDirectory() as temp_dir:
        manager = SnapshotManager(Path(temp_dir))
        scan_result = ScanResult(
            entries=[
                Entry(
                    entry_id="entry-1",
                    name="Test Entry",
                    command="test.exe",
                    image_path="test.exe",
                    source="registry",
                    status="active",
                )
            ]
        )
        saved = manager.save(scan_result, label="history-test")
        summaries = manager.list_snapshots()
        loaded = manager.load_snapshot(summaries[0].path)

        assert len(summaries) == 1
        assert summaries[0].label == "history-test"
        assert loaded.entries[0].image_path == "test.exe"
        assert loaded.timestamp == saved.timestamp

    print("  ✓ Snapshot summaries verified")


def test_llm_advisor_payload_and_validation():
    """Verify advisor payload structure and strict output validation."""
    print("Testing LLM advisor payload and validation...")

    advisor = LlmAdvisor(component_root=Path("tests/fixtures/fake-llm"))
    scorer = Scorer()
    entry = Entry(
        entry_id=SUSPICIOUS_ENTRY["entry_id"],
        name=SUSPICIOUS_ENTRY["name"],
        command=SUSPICIOUS_ENTRY["command"],
        image_path="C:\\Users\\Admin\\AppData\\Local\\Temp\\mystery.exe",
        source=SUSPICIOUS_ENTRY["source"],
        status=SUSPICIOUS_ENTRY["status"],
        metadata={
            **SUSPICIOUS_ENTRY["metadata"],
            "signature_status": "Unsigned",
            "target_exists": True,
        },
    )
    scored_entry = scorer.score([entry])[0]
    evidence = EntryEvidence(
        entry_id=entry.entry_id,
        file_info={"exists": True, "last_write_time_utc": "2026-04-12T10:30:00Z"},
        authenticode={"signed": False, "status": "Unsigned"},
        raw_evidence={"hashes": {"sha256": "abc123"}},
    )

    payload = advisor.build_payload(entry, scored_entry, evidence)
    assert payload["name"] == entry.name
    assert payload["entry_type"] == "registry_autorun"
    assert payload["available_actions"][0] == "no_action"
    assert "signals" in payload and isinstance(payload["signals"], list)

    advice = advisor.validate_response(
        {
            "summary": "Unsigned autorun from a user-writable path.",
            "evidence": [
                "unsigned binary in user-writable path",
                "registry autorun persistence",
            ],
            "assessment": "suspicious",
            "recommended_action": "disable",
            "secondary_action": "open_location",
            "justification": "Disable is safer than delete until more evidence is collected.",
            "false_positive_risk": "medium",
            "confidence": "medium",
        }
    )
    assert advice.recommended_action == "disable"
    assert advice.assessment == "suspicious"

    try:
        advisor._enforce_policy(
            {"score": 20, "risk_level": "clean"},
            advisor.validate_response(
                {
                    "summary": "Safe entry.",
                    "evidence": ["signed microsoft binary"],
                    "assessment": "clean",
                    "recommended_action": "delete",
                    "secondary_action": None,
                    "justification": "Delete it anyway.",
                    "false_positive_risk": "high",
                    "confidence": "low",
                }
            ),
        )
        raise AssertionError("Expected destructive policy rejection")
    except AdviceValidationError:
        pass

    print("  ✓ Advisor payload and validation verified")


def test_settings_persistence():
    """Verify settings persist the global LLM mode and component state."""
    print("Testing settings persistence...")

    with TemporaryDirectory() as temp_dir:
        settings_path = Path(temp_dir) / "settings.json"
        store = SettingsStore(settings_path)
        expected = AppSettings(
            llm_mode="better",
            llm_mode_configured=True,
            llm_component_status="installed",
            llm_component_error="",
            llm_installed_runtime_version="b8779",
            llm_fast_installed=True,
            llm_better_installed=True,
        )

        store.save(expected)
        loaded = store.load()

        assert loaded.llm_mode == "better"
        assert loaded.llm_mode_configured is True
        assert loaded.llm_component_status == "installed"
        assert loaded.llm_installed_runtime_version == "b8779"
        assert loaded.llm_fast_installed is True
        assert loaded.llm_better_installed is True

    print("  ✓ Settings persistence verified")


def test_llm_component_installation_flow():
    """Verify local AI components install from URLs, persist, and upgrade by mode."""
    print("Testing local AI component installation flow...")

    with TemporaryDirectory() as temp_dir:
        temp_root = Path(temp_dir)
        downloads_dir = temp_root / "downloads"
        downloads_dir.mkdir(parents=True, exist_ok=True)

        runtime_zip = downloads_dir / "llama-runtime.zip"
        with zipfile.ZipFile(runtime_zip, "w") as archive:
            archive.writestr("llama-bin/llama-completion.exe", "fake-llama-completion")
            archive.writestr("llama-bin/llama-cli.exe", "fake-llama-cli")
            archive.writestr("llama-bin/ggml-base.dll", "fake-dll")

        fast_model = downloads_dir / "qwen3-0.6b-q4_k_m.gguf"
        fast_model.write_text("fast-model", encoding="utf-8")
        better_model = downloads_dir / "qwen3-1.7b-q4_k_m.gguf"
        better_model.write_text("better-model", encoding="utf-8")

        env_updates = {
            "UBOOT_LLAMA_RUNTIME_URL": runtime_zip.resolve().as_uri(),
            "UBOOT_LLAMA_RUNTIME_SHA256": _sha256_file(runtime_zip),
            "UBOOT_LLAMA_RUNTIME_VERSION": "test-runtime",
            "UBOOT_LLM_FAST_MODEL_URL": fast_model.resolve().as_uri(),
            "UBOOT_LLM_FAST_MODEL_SHA256": _sha256_file(fast_model),
            "UBOOT_LLM_FAST_MODEL_VERSION": "test-fast",
            "UBOOT_LLM_FAST_MODEL_BYTES": str(fast_model.stat().st_size),
            "UBOOT_LLM_BETTER_MODEL_URL": better_model.resolve().as_uri(),
            "UBOOT_LLM_BETTER_MODEL_SHA256": _sha256_file(better_model),
            "UBOOT_LLM_BETTER_MODEL_VERSION": "test-better",
            "UBOOT_LLM_BETTER_MODEL_BYTES": str(better_model.stat().st_size),
        }
        previous_values = {key: os.environ.get(key) for key in env_updates}
        os.environ.update(env_updates)
        try:
            advisor = LlmAdvisor(component_root=temp_root / "llm")
            fast_result = advisor.ensure_component(LlmMode.FAST)
            fast_status = advisor.component_status(LlmMode.FAST)
            better_status_before = advisor.component_status(LlmMode.BETTER)

            assert fast_result.runtime_path.exists(), "Runtime was not installed"
            assert fast_result.model_path.exists(), "Fast model was not installed"
            assert fast_status["available"] is True
            assert better_status_before["status"] in {"missing", "partial"}
            assert (advisor.component_root / "manifest.json").exists(), "Manifest missing"

            better_result = advisor.ensure_component(LlmMode.BETTER)
            better_status = advisor.component_status(LlmMode.BETTER)

            assert better_result.model_path.exists(), "Better model was not installed"
            assert better_status["available"] is True
            assert (advisor.component_root / "runtime" / "ggml-base.dll").exists(), "Runtime DLL missing"
        finally:
            for key, value in previous_values.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value

    print("  ✓ Local AI component installation verified")


def test_html_report_generation():
    """Verify forensic HTML export renders required sections."""
    print("Testing forensic HTML generation...")

    generator = HtmlReportGenerator()
    report = ForensicReportData(
        title="Test Report",
        source_type="snapshot",
        generated_at="2026-04-13 20:00:00",
        summary={"visible_entries": 1, "cached_evidence": 1},
        metadata={"view_title": "Snapshot Test"},
        timeline=[{"timestamp": "2026-04-13T20:00:00Z", "label": "ui-scan", "entry_count": 1, "error_count": 0}],
        entries=[
            ForensicEntryRecord(
                entry_id="entry-1",
                name="Test Entry",
                source="registry",
                command="test.exe",
                image_path="test.exe",
                status="active",
                score=50,
                risk_level="suspicious",
                explanation="Suspicious test entry",
                signals=["unsigned_binary"],
                rule_matches=["unsigned_binary"],
                metadata={"signed": False},
                evidence={"hashes": {"sha256": "abc123"}},
            )
        ],
    )
    html = generator.generate_html(report)

    assert "Test Report" in html
    assert "LLM Advisory" in html
    assert "unsigned_binary" in html
    assert "ui-scan" in html

    print("  ✓ HTML report generation verified")


def run_all_tests():
    """Run all smoke tests."""
    print("\n" + "="*60)
    print("Uboot Smoke Tests")
    print("="*60 + "\n")
    
    tests = [
        test_scanner_output_structure,
        test_scanner_parses_core_schema_variant,
        test_evidence_metadata_updates,
        test_scorer_classifies_correctly,
        test_llm_advisor_payload_and_validation,
        test_settings_persistence,
        test_llm_component_installation_flow,
        test_snapshot_persistence,
        test_snapshot_listing_and_loading,
        test_snapshot_diff_detection,
        test_html_report_generation,
    ]
    
    passed = 0
    failed = 0
    
    for test in tests:
        try:
            test()
            passed += 1
        except Exception as e:
            print(f"  ✗ FAILED: {e}")
            failed += 1
    
    print("\n" + "="*60)
    print(f"Results: {passed} passed, {failed} failed")
    print("="*60 + "\n")
    
    return failed == 0


if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
