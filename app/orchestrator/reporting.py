"""Forensic HTML report generation."""
from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from html import escape
from pathlib import Path
from typing import Any, Dict, List, Optional

from .llm_advisor import LlmAdvice


@dataclass
class ForensicEntryRecord:
    """Structured entry data consumed by the HTML report generator."""

    entry_id: str
    name: str
    source: str
    command: str
    image_path: str
    status: str
    score: int
    risk_level: str
    explanation: str
    signals: List[str] = field(default_factory=list)
    rule_matches: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    evidence: Optional[Dict[str, Any]] = None
    advice: Optional[LlmAdvice] = None
    change_kind: str = "current"
    changed_fields: List[str] = field(default_factory=list)
    previous_entry: Optional[Dict[str, Any]] = None


@dataclass
class ForensicReportData:
    """Normalized report payload."""

    title: str
    source_type: str
    generated_at: str
    summary: Dict[str, Any]
    entries: List[ForensicEntryRecord] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    timeline: List[Dict[str, Any]] = field(default_factory=list)


class HtmlReportGenerator:
    """Render self-contained HTML forensic reports."""

    def generate_html(self, report: ForensicReportData) -> str:
        """Render a complete HTML document."""
        cards = "".join(
            f"<div class='metric'><span>{escape(str(key))}</span><strong>{escape(str(value))}</strong></div>"
            for key, value in report.summary.items()
        )
        timeline_rows = "".join(
            "<tr>"
            f"<td>{escape(str(item.get('timestamp', '')))}</td>"
            f"<td>{escape(str(item.get('label', '')))}</td>"
            f"<td>{escape(str(item.get('entry_count', '')))}</td>"
            f"<td>{escape(str(item.get('error_count', '')))}</td>"
            "</tr>"
            for item in report.timeline
        )
        entry_rows = "".join(self._render_entry(entry) for entry in report.entries)

        return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>{escape(report.title)}</title>
  <style>
    body {{ font-family: Segoe UI, Arial, sans-serif; margin: 24px; color: #1f2937; background: #f8fafc; }}
    h1, h2, h3 {{ margin-bottom: 8px; }}
    .meta, .summary, .entry {{ background: white; border: 1px solid #dbe2ea; border-radius: 10px; padding: 16px; margin-bottom: 16px; }}
    .metrics {{ display: flex; flex-wrap: wrap; gap: 12px; }}
    .metric {{ border: 1px solid #e5e7eb; border-radius: 8px; padding: 10px 12px; min-width: 140px; background: #f9fafb; }}
    .metric span {{ display: block; font-size: 12px; color: #6b7280; }}
    .metric strong {{ display: block; margin-top: 4px; font-size: 18px; }}
    table {{ width: 100%; border-collapse: collapse; }}
    th, td {{ text-align: left; padding: 8px; border-bottom: 1px solid #e5e7eb; vertical-align: top; }}
    .tag {{ display: inline-block; border-radius: 999px; background: #e5eef9; color: #1d4ed8; padding: 2px 8px; font-size: 12px; margin-right: 6px; }}
    .risk-clean {{ color: #166534; }}
    .risk-suspicious {{ color: #b45309; }}
    .risk-malicious {{ color: #b91c1c; }}
    .muted {{ color: #6b7280; }}
    pre {{ white-space: pre-wrap; word-break: break-word; background: #f8fafc; padding: 12px; border-radius: 8px; border: 1px solid #e5e7eb; }}
  </style>
</head>
<body>
  <h1>{escape(report.title)}</h1>
  <div class="meta">
    <p><strong>Generated:</strong> {escape(report.generated_at)}</p>
    <p><strong>Source Type:</strong> {escape(report.source_type)}</p>
  </div>
  <div class="summary">
    <h2>Summary</h2>
    <div class="metrics">{cards}</div>
  </div>
  <div class="summary">
    <h2>Context</h2>
    <pre>{escape(self._pretty_metadata(report.metadata))}</pre>
  </div>
  <div class="summary">
    <h2>Timeline</h2>
    <table>
      <thead><tr><th>Timestamp</th><th>Label</th><th>Entries</th><th>Errors</th></tr></thead>
      <tbody>{timeline_rows or "<tr><td colspan='4' class='muted'>No related snapshots.</td></tr>"}</tbody>
    </table>
  </div>
  <div class="summary">
    <h2>Entries</h2>
    {entry_rows or "<p class='muted'>No entries available.</p>"}
  </div>
</body>
</html>
"""

    def write_html(self, report: ForensicReportData, output_path: Path) -> Path:
        """Write HTML report to disk."""
        output_path.write_text(self.generate_html(report), encoding="utf-8")
        return output_path

    def _render_entry(self, entry: ForensicEntryRecord) -> str:
        """Render a single entry section."""
        risk_class = f"risk-{escape(entry.risk_level)}"
        tags = "".join(
            f"<span class='tag'>{escape(value)}</span>"
            for value in ([entry.change_kind] + entry.changed_fields)
        )
        evidence_html = "<p class='muted'>No evidence cached.</p>"
        if entry.evidence:
            evidence_html = f"<pre>{escape(self._pretty_metadata(entry.evidence))}</pre>"

        advice_html = "<p class='muted'>No LLM advisory cached.</p>"
        if entry.advice:
            evidence_lines = "".join(
                f"<li>{escape(item)}</li>" for item in entry.advice.evidence
            ) or "<li>(none)</li>"
            advice_html = (
                f"<p><strong>Summary:</strong> {escape(entry.advice.summary)}</p>"
                f"<p><strong>Assessment:</strong> {escape(entry.advice.assessment)}</p>"
                f"<p><strong>Recommended Action:</strong> {escape(entry.advice.recommended_action)}</p>"
                f"<p><strong>Secondary Action:</strong> {escape(entry.advice.secondary_action or 'none')}</p>"
                f"<p><strong>Confidence:</strong> {escape(entry.advice.confidence)}</p>"
                f"<p><strong>False Positive Risk:</strong> {escape(entry.advice.false_positive_risk)}</p>"
                f"<p><strong>Justification:</strong> {escape(entry.advice.justification)}</p>"
                f"<p><strong>Evidence:</strong></p><ul>{evidence_lines}</ul>"
                "<p class='muted'>Advisory is assisted guidance only and does not execute remediation automatically.</p>"
            )

        previous_entry_html = ""
        if entry.previous_entry:
            previous_entry_html = (
                "<h3>Previous Entry</h3>"
                f"<pre>{escape(self._pretty_metadata(entry.previous_entry))}</pre>"
            )

        return f"""
<div class="entry">
  <h3>{escape(entry.name)}</h3>
  <p>{tags}</p>
  <table>
    <tr><th>ID</th><td>{escape(entry.entry_id)}</td></tr>
    <tr><th>Source</th><td>{escape(entry.source)}</td></tr>
    <tr><th>Command</th><td>{escape(entry.command)}</td></tr>
    <tr><th>Image Path</th><td>{escape(entry.image_path)}</td></tr>
    <tr><th>Status</th><td>{escape(entry.status)}</td></tr>
    <tr><th>Score</th><td>{entry.score}</td></tr>
    <tr><th>Risk</th><td class="{risk_class}">{escape(entry.risk_level)}</td></tr>
    <tr><th>Explanation</th><td>{escape(entry.explanation)}</td></tr>
    <tr><th>Signals</th><td>{escape(', '.join(entry.signals) or '(none)')}</td></tr>
    <tr><th>Rule Matches</th><td>{escape(', '.join(entry.rule_matches) or '(none)')}</td></tr>
  </table>
  <h3>Metadata</h3>
  <pre>{escape(self._pretty_metadata(entry.metadata))}</pre>
  {previous_entry_html}
  <h3>Evidence</h3>
  {evidence_html}
  <h3>LLM Advisory</h3>
  {advice_html}
</div>
"""

    @staticmethod
    def _pretty_metadata(value: Any) -> str:
        """Pretty stringify nested structures."""
        if value is None:
            return "null"
        if isinstance(value, str):
            return value
        try:
            import json

            return json.dumps(value, indent=2, ensure_ascii=False)
        except TypeError:
            return str(value)
