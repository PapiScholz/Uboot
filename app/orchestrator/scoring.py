"""Scoring: applies rules and classifies entries."""
import json
from pathlib import Path
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass, field
from enum import Enum

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

    # Default thresholds for risk classification
    THRESHOLD_CLEAN = 30
    THRESHOLD_SUSPICIOUS = 70

    def __init__(self, rules_path: Optional[Path] = None):
        """
        Initialize scorer.
        
        Args:
            rules_path: Path to rules_v1.json. If None, searches common locations.
        """
        self.rules_path = rules_path or self._find_rules()
        self.rules = self._load_rules()

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

        # Check against rules
        for rule in self.rules:
            if self._rule_matches(entry, rule):
                score += rule.get("weight", 10)
                signals.extend(rule.get("signals", []))
                rule_matches.append(rule.get("id", "unknown"))

        # Cap score at 100
        score = min(score, 100)

        # Determine risk level
        if score <= self.THRESHOLD_CLEAN:
            risk_level = RiskLevel.CLEAN
        elif score < self.THRESHOLD_SUSPICIOUS:
            risk_level = RiskLevel.SUSPICIOUS
        else:
            risk_level = RiskLevel.MALICIOUS

        explanation = self._generate_explanation(entry, score, rule_matches)

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

    @staticmethod
    def _generate_explanation(entry: Entry, score: int, rule_matches: List[str]) -> str:
        """Generate human-readable explanation of score."""
        parts = []
        
        if score <= Scorer.THRESHOLD_CLEAN:
            parts.append(f"Clean ({score}): ")
        elif score < Scorer.THRESHOLD_SUSPICIOUS:
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
        candidates = [
            Path("rules/rules_v1.json"),
            Path.cwd() / "rules" / "rules_v1.json",
            Path(__file__).parent.parent.parent / "rules" / "rules_v1.json",
        ]
        for path in candidates:
            if path.exists():
                return path
        raise FileNotFoundError("rules_v1.json not found")

    def _load_rules(self) -> List[dict]:
        """Load and parse rules_v1.json."""
        with open(self.rules_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data.get("rules", [])
