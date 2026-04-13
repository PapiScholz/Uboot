"""Local LLM remediation advisor integration."""
from __future__ import annotations

import json
import os
import subprocess
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional

from .evidence import EntryEvidence
from .scanner import Entry
from .scoring import ScoredEntry


ALLOWED_ACTIONS = (
    "no_action",
    "review_manually",
    "disable",
    "delete",
    "open_location",
    "jump_to_registry",
    "collect_more_evidence",
    "quarantine_candidate",
)
ALLOWED_ASSESSMENTS = ("clean", "suspicious", "malicious", "unknown")
ALLOWED_CONFIDENCE = ("low", "medium", "high")
ALLOWED_FALSE_POSITIVE_RISK = ("low", "medium", "high", "unknown")


class LlmMode(str, Enum):
    """Global LLM assistance modes."""

    OFF = "off"
    FAST = "fast"
    BETTER = "better"


@dataclass(frozen=True)
class LlmProfile:
    """Internal profile settings for a mode."""

    display_name: str
    description: str
    model_filename: str
    estimated_ram: str
    estimated_latency: str
    max_output_tokens: int
    timeout_seconds: float


@dataclass
class LlmAdvice:
    """Validated structured advisor output."""

    summary: str
    evidence: List[str]
    assessment: str
    recommended_action: str
    secondary_action: Optional[str]
    justification: str
    false_positive_risk: str
    confidence: str
    raw_response: Dict[str, Any] = field(default_factory=dict)


class LlmAdvisorError(RuntimeError):
    """Base error for local advisor failures."""


class ComponentUnavailableError(LlmAdvisorError):
    """Raised when llama.cpp runtime or model files are missing."""


class AdviceValidationError(LlmAdvisorError):
    """Raised when the model response does not satisfy the contract."""


class LlmAdvisor:
    """Run structured remediation advisory through a local llama.cpp runtime."""

    PROFILES: Dict[LlmMode, LlmProfile] = {
        LlmMode.FAST: LlmProfile(
            display_name="Fast",
            description="Lower RAM usage and faster responses.",
            model_filename="qwen3-0.6b-instruct-q4.gguf",
            estimated_ram="~0.5-0.8 GB extra RAM",
            estimated_latency="Faster, more conservative output",
            max_output_tokens=192,
            timeout_seconds=1.5,
        ),
        LlmMode.BETTER: LlmProfile(
            display_name="Better",
            description="Stronger justifications with a higher resource cost.",
            model_filename="qwen3-1.7b-instruct-q4.gguf",
            estimated_ram="~0.8-1.0 GB extra RAM",
            estimated_latency="Slightly slower, better recommendations",
            max_output_tokens=256,
            timeout_seconds=2.0,
        ),
    }

    def __init__(self, component_root: Optional[Path] = None):
        self.component_root = component_root or self._default_component_root()

    @staticmethod
    def _default_component_root() -> Path:
        return Path("llm")

    @staticmethod
    def parse_mode(value: Optional[str]) -> Optional[LlmMode]:
        """Parse persisted or UI strings into a valid LlmMode."""
        if not value:
            return None
        normalized = str(value).strip().lower()
        for mode in LlmMode:
            if mode.value == normalized:
                return mode
        return None

    def get_profile(self, mode: LlmMode) -> LlmProfile:
        """Return mode profile metadata."""
        if mode == LlmMode.OFF:
            raise ValueError("OFF mode does not have a runtime profile")
        return self.PROFILES[mode]

    def component_status(self, mode: LlmMode) -> Dict[str, Any]:
        """Return component availability and guidance for a mode."""
        if mode == LlmMode.OFF:
            return {
                "available": True,
                "status": "disabled",
                "message": "LLM assistance is disabled.",
            }

        cli_path = self._resolve_cli_path()
        model_path = self._resolve_model_path(mode)
        profile = self.get_profile(mode)
        available = cli_path is not None and model_path is not None

        missing_parts = []
        if cli_path is None:
            missing_parts.append("llama.cpp runtime")
        if model_path is None:
            missing_parts.append(profile.model_filename)

        return {
            "available": available,
            "status": "installed" if available else "missing",
            "message": (
                f"{profile.display_name} local advisory is ready."
                if available
                else f"Missing optional component: {', '.join(missing_parts)}"
            ),
            "runtime_path": str(cli_path) if cli_path else "",
            "model_path": str(model_path) if model_path else "",
            "component_root": str(self.component_root.resolve()),
        }

    def install_instructions(self, mode: LlmMode) -> str:
        """Human-readable install instructions for missing local components."""
        profile = self.get_profile(mode)
        root = self.component_root.resolve()
        return (
            f"{profile.display_name} requires the optional local AI component.\n\n"
            f"Expected runtime: {root}\\runtime\\llama-cli.exe\n"
            f"Expected model:   {root}\\models\\{profile.model_filename}\n\n"
            "Install it from the optional setup package, or place the files in those paths "
            "and retry Get Evidence."
        )

    def build_payload(
        self,
        entry: Entry,
        scored_entry: ScoredEntry,
        evidence: Optional[EntryEvidence],
        available_actions: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        """Build the compact, auditable request payload."""
        metadata = dict(entry.metadata or {})
        evidence_raw = dict((evidence.raw_evidence or {}) if evidence else {})
        file_info = dict((evidence.file_info or {}) if evidence else {})
        authenticode = dict((evidence.authenticode or {}) if evidence else {})

        publisher = (
            metadata.get("publisher")
            or authenticode.get("signer_subject")
            or ""
        )
        signature_status = (
            metadata.get("signature_status")
            or authenticode.get("status")
            or ""
        )
        hashes = evidence_raw.get("hashes") or metadata.get("hashes") or {}

        payload = {
            "name": entry.name,
            "source": entry.source,
            "entry_type": self._infer_entry_type(entry.source),
            "command": entry.command,
            "image_path": entry.image_path,
            "publisher": publisher,
            "signature_status": signature_status,
            "hashes": hashes,
            "file_exists": metadata.get(
                "file_exists",
                file_info.get("exists", metadata.get("target_exists")),
            ),
            "timestamp": (
                metadata.get("last_modified")
                or metadata.get("created")
                or file_info.get("last_write_time_utc")
                or ""
            ),
            "score": scored_entry.score,
            "risk_level": scored_entry.risk_level.value,
            "signals": list(scored_entry.signals),
            "rule_matches": list(scored_entry.rule_matches),
            "available_actions": available_actions or list(ALLOWED_ACTIONS),
            "metadata": self._compact_metadata(metadata, evidence_raw),
        }
        return payload

    def advise(
        self,
        entry: Entry,
        scored_entry: ScoredEntry,
        evidence: Optional[EntryEvidence],
        mode: LlmMode,
    ) -> LlmAdvice:
        """Request structured advice from the local runtime."""
        if mode == LlmMode.OFF:
            raise LlmAdvisorError("OFF mode does not produce LLM advice")

        status = self.component_status(mode)
        if not status["available"]:
            raise ComponentUnavailableError(status["message"])

        payload = self.build_payload(entry, scored_entry, evidence)
        prompt = self._build_prompt(payload)
        response = self._invoke_llama_cpp(
            prompt=prompt,
            model_path=Path(status["model_path"]),
            runtime_path=Path(status["runtime_path"]),
            profile=self.get_profile(mode),
        )
        data = self._extract_json(response)
        return self.validate_response(data)

    def validate_response(self, data: Dict[str, Any]) -> LlmAdvice:
        """Validate and normalize the LLM contract."""
        if not isinstance(data, dict):
            raise AdviceValidationError("Advisor output must be a JSON object")

        evidence = data.get("evidence")
        if not isinstance(evidence, list) or not all(isinstance(item, str) for item in evidence):
            raise AdviceValidationError("Advisor evidence must be an array of strings")

        assessment = str(data.get("assessment", "")).strip().lower()
        if assessment not in ALLOWED_ASSESSMENTS:
            raise AdviceValidationError(f"Invalid assessment: {assessment}")

        recommended_action = str(data.get("recommended_action", "")).strip()
        if recommended_action not in ALLOWED_ACTIONS:
            raise AdviceValidationError(f"Invalid recommended_action: {recommended_action}")

        secondary_action = data.get("secondary_action")
        if secondary_action is not None:
            secondary_action = str(secondary_action).strip()
            if secondary_action and secondary_action not in ALLOWED_ACTIONS:
                raise AdviceValidationError(f"Invalid secondary_action: {secondary_action}")
            if not secondary_action:
                secondary_action = None

        confidence = str(data.get("confidence", "")).strip().lower()
        if confidence not in ALLOWED_CONFIDENCE:
            raise AdviceValidationError(f"Invalid confidence: {confidence}")

        false_positive_risk = str(data.get("false_positive_risk", "")).strip().lower()
        if false_positive_risk not in ALLOWED_FALSE_POSITIVE_RISK:
            raise AdviceValidationError(
                f"Invalid false_positive_risk: {false_positive_risk}"
            )

        summary = str(data.get("summary", "")).strip()
        justification = str(data.get("justification", "")).strip()
        if not summary:
            raise AdviceValidationError("Advisor summary is required")
        if not justification:
            raise AdviceValidationError("Advisor justification is required")

        return LlmAdvice(
            summary=summary,
            evidence=[item.strip() for item in evidence if item.strip()],
            assessment=assessment,
            recommended_action=recommended_action,
            secondary_action=secondary_action,
            justification=justification,
            false_positive_risk=false_positive_risk,
            confidence=confidence,
            raw_response=data,
        )

    def _resolve_cli_path(self) -> Optional[Path]:
        override = os.environ.get("UBOOT_LLAMACPP_CLI", "").strip()
        candidates = [
            Path(override) if override else None,
            self.component_root / "runtime" / "llama-cli.exe",
            self.component_root / "bin" / "llama-cli.exe",
            Path("third_party/llama.cpp/llama-cli.exe"),
            Path("llama.cpp/build/bin/Release/llama-cli.exe"),
        ]
        for candidate in candidates:
            if candidate and candidate.exists():
                return candidate
        return None

    def _resolve_model_path(self, mode: LlmMode) -> Optional[Path]:
        profile = self.get_profile(mode)
        env_key = (
            "UBOOT_LLM_FAST_MODEL" if mode == LlmMode.FAST else "UBOOT_LLM_BETTER_MODEL"
        )
        override = os.environ.get(env_key, "").strip()
        candidates = [
            Path(override) if override else None,
            self.component_root / "models" / profile.model_filename,
            Path("models") / profile.model_filename,
        ]
        for candidate in candidates:
            if candidate and candidate.exists():
                return candidate
        return None

    @staticmethod
    def _infer_entry_type(source: str) -> str:
        value = (source or "").lower()
        if "service" in value:
            return "service"
        if "task" in value:
            return "scheduled_task"
        if "registry" in value or "run" in value:
            return "registry_autorun"
        if "startup" in value:
            return "startup_folder"
        if "winlogon" in value:
            return "winlogon"
        return "unknown"

    @staticmethod
    def _compact_metadata(metadata: Dict[str, Any], evidence_raw: Dict[str, Any]) -> Dict[str, Any]:
        """Keep only metadata useful for an auditable advisory."""
        keys = (
            "target_exists",
            "signed",
            "signature_valid",
            "location",
            "scope",
            "service_name",
            "task_name",
            "start_type",
            "file_size",
            "entropy",
        )
        compact = {
            key: metadata[key]
            for key in keys
            if key in metadata
        }
        if "version_info" in evidence_raw:
            compact["version_info"] = evidence_raw["version_info"]
        if "file_info" in evidence_raw:
            compact["file_info"] = evidence_raw["file_info"]
        return compact

    @staticmethod
    def _build_prompt(payload: Dict[str, Any]) -> str:
        """Build a short, strict prompt for deterministic JSON output."""
        payload_json = json.dumps(payload, separators=(",", ":"), ensure_ascii=True)
        return (
            "You are a Windows persistence remediation advisor.\n"
            "Rules:\n"
            "- Use only the evidence in the JSON payload.\n"
            "- Do not invent reputation, malware family names, online context, or missing facts.\n"
            "- Pick exactly one recommended_action from available_actions.\n"
            "- Prefer the least destructive reasonable action.\n"
            "- If evidence is insufficient, choose review_manually or collect_more_evidence.\n"
            "- Do not choose delete unless the evidence is strong and consistent.\n"
            "- Keep the response short and deterministic.\n"
            "- Return valid JSON only with keys: summary, evidence, assessment, recommended_action, "
            "secondary_action, justification, false_positive_risk, confidence.\n"
            f"Payload: {payload_json}"
        )

    @staticmethod
    def _invoke_llama_cpp(
        prompt: str,
        model_path: Path,
        runtime_path: Path,
        profile: LlmProfile,
    ) -> str:
        """Invoke llama.cpp CLI for a single short response."""
        cmd = [
            str(runtime_path),
            "-m",
            str(model_path),
            "-n",
            str(profile.max_output_tokens),
            "--temp",
            "0",
            "--top-k",
            "1",
            "--top-p",
            "0.1",
            "--seed",
            "42",
            "--ctx-size",
            "2048",
            "-p",
            prompt,
        ]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=profile.timeout_seconds,
            check=False,
        )
        if result.returncode != 0:
            raise LlmAdvisorError(
                f"llama.cpp exited with code {result.returncode}: {result.stderr.strip()}"
            )
        output = result.stdout.strip()
        if not output:
            raise LlmAdvisorError("llama.cpp returned an empty response")
        return output

    @staticmethod
    def _extract_json(text: str) -> Dict[str, Any]:
        """Extract the first JSON object from model output."""
        start = text.find("{")
        end = text.rfind("}")
        if start == -1 or end == -1 or end <= start:
            raise AdviceValidationError("Advisor output did not contain a JSON object")

        candidate = text[start : end + 1]
        try:
            return json.loads(candidate)
        except json.JSONDecodeError as exc:
            raise AdviceValidationError(f"Invalid advisor JSON: {exc}") from exc
