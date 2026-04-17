"""Scoring: applies rules and classifies entries."""
import json
import os
import re
from pathlib import Path
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass, field
from enum import Enum

from app.runtime_paths import app_root

from .scanner import Entry


class RiskLevel(str, Enum):
    """Risk classification levels."""
    CLEAN = "clean"
    SUSPICIOUS = "suspicious"
    MALICIOUS = "malicious"


@dataclass
class ScoredEntry:
    """An entry with scoring results."""
    entry: Entry
    score: int  # 0-100
    risk_level: RiskLevel
    signals: List[str] = field(default_factory=list)
    explanation: str = ""
    rule_matches: List[str] = field(default_factory=list)


class Scorer:
    """Applies heuristic rules and produces scoring."""

    def __init__(self, rules_path: Optional[Path] = None):
        """
        Initialize scorer.

        Args:
            rules_path: Path to rules_v1.json. If None, searches common locations.
        """
        self.rules_path = rules_path or self._find_rules()
        self.rules_config = self._load_rules()
        self.threshold_clean, self.threshold_suspicious = self._load_thresholds()
        self.rules = self.rules_config.get("rules", [])
        self.signal_weights = {
            signal.get("id", ""): int(signal.get("weight", 0))
            for signal in self.rules_config.get("signals", [])
            if signal.get("id")
        }
        self.lolbins = {
            item.lower()
            for item in self.rules_config.get("lolbins", [])
            if isinstance(item, str)
        }

    def _load_thresholds(self) -> tuple:
        thresholds = {t["label"]: t["min_score"]
                      for t in self.rules_config.get("thresholds", [])
                      if "label" in t and "min_score" in t}
        clean = int(thresholds.get("Warning", 30))
        suspicious = int(thresholds.get("High Risk", 70))
        return clean, suspicious

    def score(self, entries: List[Entry]) -> List[ScoredEntry]:
        """
        Score a list of entries.
        
        Args:
            entries: List of Entry objects from Scanner
            
        Returns:
            List of ScoredEntry with scores and risk levels
        """
        return [self._score_entry(entry) for entry in entries]

    def _score_entry(self, entry: Entry) -> ScoredEntry:
        """Score a single entry."""
        score = 0
        signals = []
        rule_matches = []

        # Preferred mode: weighted signals from rules_v1.json (current schema).
        if self.signal_weights:
            fired_signals = self._evaluate_signal_ids(entry)
            for signal_id in fired_signals:
                score += self.signal_weights.get(signal_id, 0)
                signals.append(signal_id)
                rule_matches.append(signal_id)

        # Backward-compatibility mode: legacy explicit rules list.
        for rule in self.rules:
            if self._rule_matches(entry, rule):
                score += int(rule.get("weight", 10))
                signals.extend(rule.get("signals", []))
                rule_matches.append(rule.get("id", "unknown"))

        # Keep score in [0, 100].
        score = max(0, min(score, 100))

        # Determine risk level
        if score <= self.threshold_clean:
            risk_level = RiskLevel.CLEAN
        elif score < self.threshold_suspicious:
            risk_level = RiskLevel.SUSPICIOUS
        else:
            risk_level = RiskLevel.MALICIOUS

        explanation = self._generate_explanation(entry, score, rule_matches, self.threshold_clean, self.threshold_suspicious)

        return ScoredEntry(
            entry=entry,
            score=score,
            risk_level=risk_level,
            signals=list(set(signals)),  # Remove duplicates
            explanation=explanation,
            rule_matches=rule_matches
        )

    @staticmethod
    def _rule_matches(entry: Entry, rule: dict) -> bool:
        """Check if a rule matches an entry."""
        # Check entry source
        if "sources" in rule:
            if entry.source not in rule["sources"]:
                return False

        # Check command patterns
        if "command_patterns" in rule:
            for pattern in rule["command_patterns"]:
                if pattern.lower() in entry.command.lower():
                    return True
            return False

        # Check name patterns
        if "name_patterns" in rule:
            for pattern in rule["name_patterns"]:
                if pattern.lower() in entry.name.lower():
                    return True
            return False

        return True

    def _evaluate_signal_ids(self, entry: Entry) -> List[str]:
        """Evaluate built-in signal ids against an entry."""
        fired: List[str] = []

        command = (entry.command or "").strip()
        command_l = command.lower()
        metadata = entry.metadata or {}

        # missing_target
        if metadata.get("target_exists") is False or metadata.get("file_exists") is False:
            fired.append("missing_target")

        # unsigned_binary
        if metadata.get("signed") is False:
            fired.append("unsigned_binary")

        # invalid_signature
        if metadata.get("signature_valid") is False:
            fired.append("invalid_signature")
        sig_status = str(metadata.get("signature_status", "")).lower()
        if sig_status and sig_status not in {"valid", "ok", "trusted", "success"}:
            fired.append("invalid_signature")

        # user_writable_location (common suspicious paths)
        if any(
            marker in command_l for marker in [
                "\\users\\",
                "\\appdata\\",
                "\\temp\\",
                "\\programdata\\",
                "%appdata%",
                "%temp%",
            ]
        ):
            fired.append("user_writable_location")

        # lolbin_autorun
        exe_name = self._extract_executable_name(command_l)
        if exe_name and exe_name in self.lolbins:
            fired.append("lolbin_autorun")

        # obfuscated_command
        if self._looks_obfuscated(command_l):
            fired.append("obfuscated_command")

        # relative_path
        if command and self._looks_relative_command(command):
            fired.append("relative_path")

        # system32_legit_signed (negative weight in rules)
        publisher = str(metadata.get("publisher", "")).lower()
        signed = metadata.get("signed") is True or str(metadata.get("signature_status", "")).lower() in {
            "valid", "ok", "trusted", "success"
        }
        if "\\windows\\system32\\" in command_l and signed and "microsoft" in publisher:
            fired.append("system32_legit_signed")

        # De-duplicate while preserving order
        seen = set()
        ordered = []
        for signal_id in fired:
            if signal_id in seen:
                continue
            seen.add(signal_id)
            ordered.append(signal_id)
        return ordered

    @staticmethod
    def _extract_executable_name(command_l: str) -> str:
        """Extract executable filename from command line."""
        if not command_l:
            return ""

        text = command_l.strip().strip('"')
        first = text.split()[0] if text.split() else ""
        if not first:
            return ""
        return first.split("\\")[-1]

    @staticmethod
    def _looks_obfuscated(command_l: str) -> bool:
        """Detect common obfuscation indicators in command line."""
        indicators = [
            " -enc ",
            " -encodedcommand",
            "frombase64string(",
            "iex(",
            "invoke-expression",
            "^",
            "%comspec%",
        ]
        if any(token in command_l for token in indicators):
            return True

        # Long base64-like blobs
        if re.search(r"[a-z0-9+/]{60,}={0,2}", command_l):
            return True

        return False

    @staticmethod
    def _looks_relative_command(command: str) -> bool:
        """Detect relative executable path patterns."""
        text = command.strip().strip('"')
        if not text:
            return False

        # Absolute Windows paths: C:\... or \\server\share
        if re.match(r"^[a-zA-Z]:\\", text) or text.startswith("\\\\"):
            return False

        # Environment variable at start can still be absolute-ish; do not flag hard.
        if text.startswith("%"):
            return False

        # Explicit relative markers or bare executable names.
        if text.startswith(".\\") or text.startswith("..\\"):
            return True

        first = text.split()[0] if text.split() else text
        has_dir_separator = "\\" in first or "/" in first
        return not has_dir_separator

    @staticmethod
    def _generate_explanation(entry: Entry, score: int, rule_matches: List[str],
                              threshold_clean: int = 30, threshold_suspicious: int = 70) -> str:
        """Generate human-readable explanation of score."""
        parts = []

        if score <= threshold_clean:
            parts.append(f"Clean ({score}): ")
        elif score < threshold_suspicious:
            parts.append(f"Suspicious ({score}): ")
        else:
            parts.append(f"Malicious ({score}): ")

        if rule_matches:
            parts.append(f"Matched {len(rule_matches)} rule(s): {', '.join(rule_matches)}")
        else:
            parts.append("No suspicious patterns detected.")

        return "".join(parts)

    @staticmethod
    def _find_rules() -> Path:
        """Find rules_v1.json in common locations."""
        override = os.environ.get("UBOOT_RULES_PATH", "").strip()
        if override:
            candidate = Path(override)
            if candidate.exists():
                return candidate

        root = app_root()
        repo_root = Path(__file__).resolve().parent.parent.parent
        candidates = [
            root / "rules" / "rules_v1.json",
            repo_root / "rules" / "rules_v1.json",
            Path("rules/rules_v1.json"),
            Path.cwd() / "rules" / "rules_v1.json",
        ]
        for path in candidates:
            if path.exists():
                return path
        raise FileNotFoundError("rules_v1.json not found")

    def _load_rules(self) -> dict:
        """Load and parse rules_v1.json."""
        with open(self.rules_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data
