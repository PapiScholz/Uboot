"""Local LLM remediation advisor integration and optional component installer."""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

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
ProgressCallback = Optional[Callable[[str, int], None]]


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
    minimum_ram: str
    recommended_ram: str
    download_size: str
    benefit_summary: str
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

    def to_display_sections(self) -> Dict[str, Any]:
        """Return GUI/report-friendly structured sections."""
        return {
            "summary": self.summary,
            "assessment": self.assessment,
            "recommended_action": self.recommended_action,
            "secondary_action": self.secondary_action or "none",
            "confidence": self.confidence,
            "false_positive_risk": self.false_positive_risk,
            "evidence": list(self.evidence),
            "justification": self.justification,
        }


@dataclass(frozen=True)
class LlmAsset:
    """Downloadable asset required for a local LLM profile."""

    asset_id: str
    kind: str
    filename: str
    url: str
    version: str
    sha256: str
    size_bytes: int
    target_path: Path
    is_archive: bool = False


@dataclass
class LlmInstallResult:
    """Result of ensuring a local runtime/model profile."""

    mode: LlmMode
    runtime_path: Path
    model_path: Path
    runtime_installed: bool
    model_installed: bool
    manifest_path: Path


class LlmAdvisorError(RuntimeError):
    """Base error for local advisor failures."""


class ComponentUnavailableError(LlmAdvisorError):
    """Raised when llama.cpp runtime or model files are missing."""


class AdviceValidationError(LlmAdvisorError):
    """Raised when the model response does not satisfy the contract."""


class LlmInstallError(LlmAdvisorError):
    """Raised when the optional local AI component cannot be installed."""


class LlmInstaller:
    """Download and install the optional local LLM runtime and models."""

    def __init__(self, component_root: Path):
        self.component_root = component_root
        self.runtime_dir = self.component_root / "runtime"
        self.models_dir = self.component_root / "models"
        self.cache_dir = self.component_root / "cache"
        self.manifest_path = self.component_root / "manifest.json"
        self.runtime_version = os.environ.get("UBOOT_LLAMA_RUNTIME_VERSION", "b8779")
        self.runtime_filename = f"llama-{self.runtime_version}-bin-win-cpu-x64.zip"
        self.runtime_url = os.environ.get(
            "UBOOT_LLAMA_RUNTIME_URL",
            f"https://github.com/ggml-org/llama.cpp/releases/download/{self.runtime_version}/{self.runtime_filename}",
        )
        self.runtime_sha256 = os.environ.get("UBOOT_LLAMA_RUNTIME_SHA256", "").strip()
        self.model_specs = {
            LlmMode.FAST: {
                "filename": "qwen3-0.6b-q4_k_m.gguf",
                "url": os.environ.get(
                    "UBOOT_LLM_FAST_MODEL_URL",
                    "https://huggingface.co/rippertnt/Qwen3-0.6B-Q4_K_M-GGUF/resolve/main/qwen3-0.6b-q4_k_m.gguf?download=true",
                ),
                "sha256": os.environ.get("UBOOT_LLM_FAST_MODEL_SHA256", "").strip(),
                "version": os.environ.get("UBOOT_LLM_FAST_MODEL_VERSION", "Q4_K_M"),
                "size_bytes": int(os.environ.get("UBOOT_LLM_FAST_MODEL_BYTES", str(484 * 1024 * 1024))),
            },
            LlmMode.BETTER: {
                "filename": "qwen3-1.7b-q4_k_m.gguf",
                "url": os.environ.get(
                    "UBOOT_LLM_BETTER_MODEL_URL",
                    "https://huggingface.co/sabafallah/Qwen3-1.7B-Q4_K_M-GGUF/resolve/main/qwen3-1.7b-q4_k_m.gguf?download=true",
                ),
                "sha256": os.environ.get("UBOOT_LLM_BETTER_MODEL_SHA256", "").strip(),
                "version": os.environ.get("UBOOT_LLM_BETTER_MODEL_VERSION", "Q4_K_M"),
                "size_bytes": int(os.environ.get("UBOOT_LLM_BETTER_MODEL_BYTES", str(1100 * 1024 * 1024))),
            },
        }

    def runtime_path(self) -> Path:
        return self.runtime_dir / "llama-completion.exe"

    def model_path(self, mode: LlmMode) -> Path:
        return self.models_dir / self.model_specs[mode]["filename"]

    def required_assets(self, mode: LlmMode) -> List[LlmAsset]:
        if mode == LlmMode.OFF:
            return []
        return [
            LlmAsset(
                asset_id="runtime",
                kind="runtime",
                filename=self.runtime_filename,
                url=self.runtime_url,
                version=self.runtime_version,
                sha256=self.runtime_sha256,
                size_bytes=44 * 1024 * 1024,
                target_path=self.runtime_path(),
                is_archive=True,
            ),
            self._build_model_asset(mode),
        ]

    def missing_assets(self, mode: LlmMode) -> List[LlmAsset]:
        return [asset for asset in self.required_assets(mode) if not asset.target_path.exists()]

    def ensure_component(self, mode: LlmMode, progress: ProgressCallback = None) -> LlmInstallResult:
        if mode == LlmMode.OFF:
            raise LlmInstallError("OFF mode does not require installation")

        self.component_root.mkdir(parents=True, exist_ok=True)
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.models_dir.mkdir(parents=True, exist_ok=True)

        runtime_installed = self.runtime_path().exists()
        model_asset = self._build_model_asset(mode)
        model_installed = model_asset.target_path.exists()

        if not runtime_installed:
            self._install_runtime(progress)
            runtime_installed = True

        if not model_installed:
            self._install_model(model_asset, progress)
            model_installed = True

        result = LlmInstallResult(
            mode=mode,
            runtime_path=self.runtime_path(),
            model_path=model_asset.target_path,
            runtime_installed=runtime_installed,
            model_installed=model_installed,
            manifest_path=self.manifest_path,
        )
        self._write_manifest()
        return result

    def _build_model_asset(self, mode: LlmMode) -> LlmAsset:
        spec = self.model_specs[mode]
        return LlmAsset(
            asset_id=f"model_{mode.value}",
            kind="model",
            filename=spec["filename"],
            url=spec["url"],
            version=spec["version"],
            sha256=spec["sha256"],
            size_bytes=spec["size_bytes"],
            target_path=self.models_dir / spec["filename"],
            is_archive=False,
        )

    def _install_runtime(self, progress: ProgressCallback) -> None:
        asset = self.required_assets(LlmMode.FAST)[0]
        archive_path = self._download_asset(asset, progress)
        extraction_root = self.cache_dir / f"{asset.asset_id}-{asset.version}.extract"
        if extraction_root.exists():
            shutil.rmtree(extraction_root)
        extraction_root.mkdir(parents=True, exist_ok=True)

        self._emit(progress, "Extracting local runtime...", 70)
        with zipfile.ZipFile(archive_path, "r") as archive:
            archive.extractall(extraction_root)

        runtime_source = self._find_runtime_dir(extraction_root)
        staging_dir = self.component_root / "runtime.__new__"
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
        shutil.copytree(runtime_source, staging_dir)

        if self.runtime_dir.exists():
            shutil.rmtree(self.runtime_dir)
        staging_dir.replace(self.runtime_dir)
        self._emit(progress, "Local runtime installed.", 90)

    def _install_model(self, asset: LlmAsset, progress: ProgressCallback) -> None:
        downloaded = self._download_asset(asset, progress)
        self.models_dir.mkdir(parents=True, exist_ok=True)
        temp_target = asset.target_path.with_suffix(asset.target_path.suffix + ".new")
        if temp_target.exists():
            temp_target.unlink()
        shutil.copy2(downloaded, temp_target)
        temp_target.replace(asset.target_path)
        self._emit(progress, f"{asset.filename} installed.", 95)

    def _download_asset(self, asset: LlmAsset, progress: ProgressCallback) -> Path:
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        destination = self.cache_dir / asset.filename
        temp_destination = destination.with_suffix(destination.suffix + ".download")
        if temp_destination.exists():
            temp_destination.unlink()

        self._emit(progress, f"Downloading {asset.filename}...", 10)
        request = urllib.request.Request(asset.url, headers={"User-Agent": "Uboot/1.0"})
        with urllib.request.urlopen(request, timeout=60) as response, temp_destination.open("wb") as handle:
            total = int(response.headers.get("Content-Length") or 0)
            downloaded = 0
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                handle.write(chunk)
                downloaded += len(chunk)
                percent = 10
                if total > 0:
                    percent = 10 + int((downloaded / total) * 50)
                self._emit(progress, f"Downloading {asset.filename}...", min(percent, 60))

        if asset.sha256:
            self._emit(progress, f"Verifying {asset.filename}...", 65)
            digest = self._sha256_file(temp_destination)
            if digest.lower() != asset.sha256.lower():
                temp_destination.unlink(missing_ok=True)
                raise LlmInstallError(
                    f"Checksum mismatch for {asset.filename}. Expected {asset.sha256}, got {digest}."
                )

        temp_destination.replace(destination)
        return destination

    @staticmethod
    def _find_runtime_dir(extraction_root: Path) -> Path:
        matches = list(extraction_root.rglob("llama-completion.exe"))
        if not matches:
            raise LlmInstallError("The downloaded runtime archive did not contain llama-completion.exe.")
        return matches[0].parent

    def _write_manifest(self) -> None:
        runtime_path = self.runtime_path()
        runtime_sha = self._sha256_file(runtime_path) if runtime_path.exists() else ""
        payload = {
            "runtime": {
                "version": self.runtime_version,
                "url": self.runtime_url,
                "path": str(runtime_path),
                "sha256": runtime_sha,
                "installed": runtime_path.exists(),
            },
            "models": {
                mode.value: {
                    "filename": spec["filename"],
                    "version": spec["version"],
                    "url": spec["url"],
                    "path": str(self.models_dir / spec["filename"]),
                    "installed": (self.models_dir / spec["filename"]).exists(),
                    "sha256": self._sha256_file(self.models_dir / spec["filename"])
                    if (self.models_dir / spec["filename"]).exists()
                    else "",
                }
                for mode, spec in self.model_specs.items()
            },
        }
        self.manifest_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    @staticmethod
    def _sha256_file(path: Path) -> str:
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
        return digest.hexdigest()

    @staticmethod
    def _emit(progress: ProgressCallback, message: str, percent: int) -> None:
        if progress is not None:
            progress(message, max(0, min(percent, 100)))


class LlmAdvisor:
    """Run structured remediation advisory through a local llama.cpp runtime."""

    ADVISOR_OUTPUT_VERSION = "2026-04-13-quality-1"

    PROFILES: Dict[LlmMode, LlmProfile] = {
        LlmMode.FAST: LlmProfile(
            display_name="Fast",
            description="Lower RAM usage and faster responses.",
            model_filename="qwen3-0.6b-q4_k_m.gguf",
            estimated_ram="~0.5-0.8 GB extra RAM",
            estimated_latency="Faster, more conservative output",
            minimum_ram="4 GB minimum",
            recommended_ram="8 GB recommended",
            download_size="~450 MB first install",
            benefit_summary="Best for quick explanations on modest hardware.",
            max_output_tokens=220,
            timeout_seconds=15.0,
        ),
        LlmMode.BETTER: LlmProfile(
            display_name="Better",
            description="Stronger justifications with a higher resource cost.",
            model_filename="qwen3-1.7b-q4_k_m.gguf",
            estimated_ram="~0.9-1.3 GB extra RAM",
            estimated_latency="Slower, stronger recommendations",
            minimum_ram="8 GB minimum",
            recommended_ram="8-16 GB recommended",
            download_size="~1.2 GB first install",
            benefit_summary="Best for richer remediation justification.",
            max_output_tokens=256,
            timeout_seconds=24.0,
        ),
    }

    def __init__(self, component_root: Optional[Path] = None):
        self.component_root = component_root or self._default_component_root()
        self.installer = LlmInstaller(self.component_root)

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

    def ensure_component(self, mode: LlmMode, progress: ProgressCallback = None) -> LlmInstallResult:
        """Download and install the local runtime and model for the selected mode."""
        return self.installer.ensure_component(mode, progress)

    def cache_fingerprint(
        self,
        entry: Entry,
        scored_entry: ScoredEntry,
        evidence: Optional[EntryEvidence],
        mode: LlmMode,
    ) -> str:
        """Return a stable cache fingerprint for a specific payload + mode."""
        payload = self.build_payload(entry, scored_entry, evidence)
        cache_payload = {
            "advisor_version": self.ADVISOR_OUTPUT_VERSION,
            "mode": mode.value,
            "model": self.get_profile(mode).model_filename,
            "payload": payload,
        }
        digest = hashlib.sha256(
            json.dumps(
                cache_payload,
                sort_keys=True,
                separators=(",", ":"),
                ensure_ascii=True,
            ).encode("utf-8")
        ).hexdigest()
        return digest[:16]

    def component_status(self, mode: LlmMode) -> Dict[str, Any]:
        """Return component availability and guidance for a mode."""
        if mode == LlmMode.OFF:
            return {
                "available": True,
                "status": "disabled",
                "message": "LLM assistance is disabled.",
                "runtime_installed": False,
                "model_installed": False,
                "missing_assets": [],
                "estimated_download_bytes": 0,
                "estimated_download": "0 MB",
            }

        profile = self.get_profile(mode)
        runtime_path = self._resolve_cli_path()
        model_path = self._resolve_model_path(mode)
        runtime_installed = runtime_path is not None and runtime_path.exists()
        model_installed = model_path is not None and model_path.exists()
        missing_assets = self.installer.missing_assets(mode)
        available = runtime_installed and model_installed

        if available:
            status = "installed"
            message = f"{profile.display_name} local advisory is ready."
        elif runtime_installed or model_installed:
            status = "partial"
            message = f"{profile.display_name} local advisory is partially installed."
        else:
            status = "missing"
            message = f"{profile.display_name} local advisory is not installed yet."

        total_missing_bytes = sum(asset.size_bytes for asset in missing_assets)
        return {
            "available": available,
            "status": status,
            "message": message,
            "runtime_path": str(runtime_path) if runtime_path else str(self.installer.runtime_path()),
            "model_path": str(model_path) if model_path else str(self.installer.model_path(mode)),
            "component_root": str(self.component_root.resolve()),
            "runtime_installed": runtime_installed,
            "model_installed": model_installed,
            "missing_assets": [asset.filename for asset in missing_assets],
            "estimated_download_bytes": total_missing_bytes,
            "estimated_download": self._format_size(total_missing_bytes),
        }

    def install_instructions(self, mode: LlmMode) -> str:
        """Human-readable install instructions for missing local components."""
        profile = self.get_profile(mode)
        root = self.component_root.resolve()
        status = self.component_status(mode)
        return (
            f"{profile.display_name} requires the optional local AI component.\n\n"
            f"Expected runtime: {root}\\runtime\\llama-completion.exe\n"
            f"Expected model:   {root}\\models\\{profile.model_filename}\n"
            f"Estimated download: {status['estimated_download']}\n\n"
            "Uboot can download and link the component automatically on first use."
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

        publisher = metadata.get("publisher") or authenticode.get("signer_subject") or ""
        signature_status = metadata.get("signature_status") or authenticode.get("status") or ""
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
        advice = self.validate_response(data)
        self._stabilize_advice(payload, advice)
        self._enforce_policy(payload, advice)
        return advice

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

    @staticmethod
    def _enforce_policy(payload: Dict[str, Any], advice: LlmAdvice) -> None:
        """Reject advice that violates product safety expectations."""
        score = int(payload.get("score", 0))
        risk_level = str(payload.get("risk_level", "")).strip().lower()
        if risk_level in {"suspicious", "malicious"} and advice.recommended_action == "no_action":
            raise AdviceValidationError(
                "No-action recommendation rejected for suspicious or malicious heuristic classification"
            )
        if advice.recommended_action == "delete":
            if score < 70 or risk_level != "malicious" or advice.confidence != "high":
                raise AdviceValidationError(
                    "Delete recommendation rejected due to insufficient evidence strength"
                )
        if risk_level == "clean" and advice.recommended_action in {
            "delete",
            "disable",
            "quarantine_candidate",
        }:
            raise AdviceValidationError(
                "Destructive recommendation rejected for clean heuristic classification"
            )
        banned_evidence = {"hashes", "sha256", "signature_status", "metadata", "signals", "rule_matches"}
        if any(item.strip().lower() in banned_evidence for item in advice.evidence):
            raise AdviceValidationError("Evidence items must be concrete observations, not payload field names")

    def _stabilize_advice(self, payload: Dict[str, Any], advice: LlmAdvice) -> None:
        """Normalize small-model output into concrete, auditable UI text."""
        self._coerce_low_signal_recommendation(payload, advice)
        canonical_evidence = self._canonical_evidence_items(payload)
        advice.evidence = canonical_evidence[:3] or self._normalize_evidence_items(payload, advice.evidence)
        advice.summary = self._compose_summary(payload, advice)
        advice.justification = self._compose_justification(payload, advice)
        advice.raw_response = {
            **advice.raw_response,
            "_normalized_by": self.ADVISOR_OUTPUT_VERSION,
        }

    @staticmethod
    def _coerce_low_signal_recommendation(payload: Dict[str, Any], advice: LlmAdvice) -> None:
        """Dampen overconfident recommendations for weak heuristic cases."""
        signals = {
            str(item).strip().lower()
            for item in [*payload.get("signals", []), *payload.get("rule_matches", [])]
            if str(item).strip()
        }
        strong_indicators = {
            "unsigned_binary",
            "invalid_signature",
            "missing_target",
            "obfuscated_command",
            "relative_path",
        }
        score = int(payload.get("score", 0))
        weak_case = (
            score < 50
            and len(payload.get("rule_matches", [])) <= 1
            and not any(signal in strong_indicators for signal in signals)
        )
        if not weak_case:
            return

        if advice.assessment == "malicious":
            advice.assessment = "suspicious"
        if advice.confidence == "high":
            advice.confidence = "medium"
        if advice.false_positive_risk == "low":
            advice.false_positive_risk = "medium"
        if advice.recommended_action in {"disable", "delete", "quarantine_candidate"}:
            advice.recommended_action = "review_manually"
        if advice.secondary_action in {None, "disable", "delete", "quarantine_candidate"}:
            advice.secondary_action = "open_location"

    def _resolve_cli_path(self) -> Optional[Path]:
        override = os.environ.get("UBOOT_LLAMACPP_CLI", "").strip()
        candidates = [
            Path(override) if override else None,
            self.installer.runtime_path(),
            self.component_root / "runtime" / "llama-cli.exe",
            self.component_root / "bin" / "llama-completion.exe",
            self.component_root / "bin" / "llama-cli.exe",
            Path("third_party/llama.cpp/llama-completion.exe"),
            Path("third_party/llama.cpp/llama-cli.exe"),
            Path("llama.cpp/build/bin/Release/llama-completion.exe"),
            Path("llama.cpp/build/bin/Release/llama-cli.exe"),
        ]
        for candidate in candidates:
            if candidate and candidate.exists():
                return candidate
        return None

    def _resolve_model_path(self, mode: LlmMode) -> Optional[Path]:
        profile = self.get_profile(mode)
        env_key = "UBOOT_LLM_FAST_MODEL" if mode == LlmMode.FAST else "UBOOT_LLM_BETTER_MODEL"
        override = os.environ.get(env_key, "").strip()
        candidates = [
            Path(override) if override else None,
            self.installer.model_path(mode),
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
        compact = {key: metadata[key] for key in keys if key in metadata}
        if "version_info" in evidence_raw:
            compact["version_info"] = evidence_raw["version_info"]
        if "file_info" in evidence_raw:
            compact["file_info"] = evidence_raw["file_info"]
        return compact

    @staticmethod
    def _normalize_evidence_items(payload: Dict[str, Any], evidence_items: List[str]) -> List[str]:
        """Turn terse signal labels into concrete, auditable evidence strings."""
        mapping = {
            "unsigned_binary": "unsigned binary",
            "invalid_signature": "invalid or missing signature",
            "user_writable_location": "binary in a user-writable path",
            "sha256": f"sha256 available: {payload.get('hashes', {}).get('sha256', '')}".strip(": "),
            "signature_status": f"signature status: {payload.get('signature_status', 'unknown')}",
            "hashes": "file hash was collected",
        }
        normalized: List[str] = []
        for item in evidence_items:
            key = item.strip().lower()
            candidate = mapping.get(key, item.strip())
            if candidate and candidate not in normalized:
                normalized.append(candidate)

        if not normalized:
            for signal in payload.get("signals", [])[:3]:
                candidate = mapping.get(str(signal).strip().lower(), str(signal))
                if candidate and candidate not in normalized:
                    normalized.append(candidate)
        return normalized[:3]

    @staticmethod
    def _signal_set(payload: Dict[str, Any]) -> set[str]:
        return {
            str(item).strip().lower()
            for item in [*payload.get("signals", []), *payload.get("rule_matches", [])]
            if str(item).strip()
        }

    @classmethod
    def _canonical_evidence_items(cls, payload: Dict[str, Any]) -> List[str]:
        """Build concrete evidence lines from the structured payload."""
        facts: List[str] = []
        signals = cls._signal_set(payload)
        entry_type = str(payload.get("entry_type", "")).strip().lower()
        source = str(payload.get("source", "")).strip()
        image_path = str(payload.get("image_path") or payload.get("command") or "").strip()
        publisher = str(payload.get("publisher", "")).strip()
        signature_status = str(payload.get("signature_status", "")).strip()
        signature_l = signature_status.lower()
        file_exists = payload.get("file_exists")

        persistence_map = {
            "service": "Persistence is configured as a Windows service",
            "scheduled_task": "Persistence is configured as a scheduled task",
            "registry_autorun": "Persistence is configured through a registry autorun",
            "startup_folder": "Persistence is configured through the Startup folder",
            "winlogon": "Persistence is configured through Winlogon",
        }
        if entry_type in persistence_map:
            facts.append(persistence_map[entry_type])
        elif source:
            facts.append(f"Persistence source: {source}")

        if "user_writable_location" in signals:
            facts.append("The target path is under a user-writable location")
        if "unsigned_binary" in signals or signature_l == "unsigned":
            facts.append("The file is unsigned")
        elif "invalid_signature" in signals:
            facts.append("The file signature is missing or invalid")
        elif signature_status and signature_l not in {"valid", "ok", "trusted", "success", "unknown"}:
            facts.append(f"Signature status: {signature_status}")
        elif signature_l == "unknown":
            facts.append("Signature status could not be verified from the collected evidence")

        if "missing_target" in signals or file_exists is False:
            facts.append("The target file is missing on disk")
        if "obfuscated_command" in signals:
            facts.append("The command line includes obfuscation indicators")
        if "relative_path" in signals:
            facts.append("The command uses a relative executable path")
        if publisher:
            facts.append(f"Publisher: {publisher}")
        if image_path:
            facts.append(f"Target path: {image_path}")

        normalized: List[str] = []
        for fact in facts:
            candidate = str(fact).strip()
            if not candidate or candidate in normalized:
                continue
            normalized.append(candidate)
        return normalized[:5]

    @classmethod
    def _subject_phrase(cls, payload: Dict[str, Any], definite: bool = False) -> str:
        entry_type = str(payload.get("entry_type", "")).strip().lower()
        mapping = {
            "service": ("This service", "the service"),
            "scheduled_task": ("This scheduled task", "the scheduled task"),
            "registry_autorun": ("This registry autorun", "the registry autorun"),
            "startup_folder": ("This startup entry", "the startup entry"),
            "winlogon": ("This Winlogon entry", "the Winlogon entry"),
        }
        this_phrase, the_phrase = mapping.get(entry_type, ("This persistence entry", "the persistence entry"))
        return the_phrase if definite else this_phrase

    @classmethod
    def _concern_phrases(cls, payload: Dict[str, Any]) -> List[str]:
        signals = cls._signal_set(payload)
        signature_status = str(payload.get("signature_status", "")).strip().lower()
        concerns: List[str] = []
        if "user_writable_location" in signals:
            concerns.append("points to a user-writable path")
        if (
            "unsigned_binary" in signals
            or "invalid_signature" in signals
            or signature_status in {"unsigned", "invalid", "untrusted", "error", "revoked"}
        ):
            concerns.append("does not have a trusted signature")
        if "missing_target" in signals or payload.get("file_exists") is False:
            concerns.append("references a target that is missing on disk")
        if "obfuscated_command" in signals:
            concerns.append("uses an obfuscated command line")
        if "relative_path" in signals:
            concerns.append("uses a relative executable path")
        return concerns

    @staticmethod
    def _join_phrases(parts: List[str]) -> str:
        if not parts:
            return ""
        if len(parts) == 1:
            return parts[0]
        if len(parts) == 2:
            return f"{parts[0]} and {parts[1]}"
        return f"{', '.join(parts[:-1])}, and {parts[-1]}"

    @classmethod
    def _compose_summary(cls, payload: Dict[str, Any], advice: LlmAdvice) -> str:
        subject = cls._subject_phrase(payload)
        concerns = cls._concern_phrases(payload)
        if advice.assessment == "clean":
            return f"{subject} does not show suspicious persistence indicators in the current evidence."
        if concerns:
            reason_text = cls._join_phrases(concerns[:2])
            if advice.recommended_action in {"review_manually", "collect_more_evidence", "open_location"}:
                return f"{subject} {reason_text}, but the current evidence is still limited enough to require review."
            return f"{subject} {reason_text}, which supports a {advice.assessment} assessment."
        return f"{subject} is assessed as {advice.assessment} from the current structured evidence."

    @classmethod
    def _compose_justification(cls, payload: Dict[str, Any], advice: LlmAdvice) -> str:
        subject = cls._subject_phrase(payload, definite=True)
        concerns = cls._concern_phrases(payload)
        reason_text = cls._join_phrases(concerns[:2]) or "is backed by the current persistence evidence"
        if advice.recommended_action == "disable":
            return (
                f"Disable is preferred because {subject} {reason_text}; it stops the persistence point "
                "without taking a permanent action like delete."
            )
        if advice.recommended_action == "review_manually":
            return (
                f"Manual review is preferred because {subject} {reason_text}, but the current evidence "
                "is not strong enough for a destructive remediation step."
            )
        if advice.recommended_action == "collect_more_evidence":
            return (
                f"Collecting more evidence is safer because {subject} {reason_text}, and the current "
                "record is still too thin for confident remediation."
            )
        if advice.recommended_action == "open_location":
            return (
                f"Open Location is the safest next step because {subject} {reason_text}, and inspecting "
                "the target on disk will clarify whether remediation is justified."
            )
        if advice.recommended_action == "jump_to_registry":
            return (
                f"Jump to Registry is the safest next step because {subject} {reason_text}, and reviewing "
                "the persistence value in context comes before any stronger action."
            )
        if advice.recommended_action == "quarantine_candidate":
            return (
                f"Quarantine is reasonable because {subject} {reason_text}, while still keeping the "
                "response more reversible than delete."
            )
        if advice.recommended_action == "delete":
            return (
                f"Delete is only justified when {subject} is supported by multiple strong indicators of "
                "malicious persistence; if that confidence drops, prefer disable instead."
            )
        return (
            f"No immediate action is recommended because the current evidence around {subject} does not "
            "establish a strong reason for remediation."
        )

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
            "- assessment should usually align with risk_level unless the payload clearly supports a weaker view.\n"
            "- For high-risk unsigned persistence in a user-writable path, prefer disable or quarantine_candidate.\n"
            "- Do not use no_action for clearly suspicious or malicious persistence.\n"
            "- evidence items must be concrete observed facts, not field names or generic labels.\n"
            "- Keep the response short and deterministic.\n"
            "- summary must be one short sentence, max 160 characters.\n"
            "- evidence must contain 1 to 3 short strings, each under 80 characters.\n"
            "- justification must be one short sentence, max 220 characters.\n"
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
        json_schema = json.dumps(
            {
                "type": "object",
                "properties": {
                    "summary": {"type": "string", "maxLength": 160},
                    "evidence": {
                        "type": "array",
                        "minItems": 1,
                        "maxItems": 3,
                        "items": {"type": "string", "maxLength": 80},
                    },
                    "assessment": {"type": "string", "enum": list(ALLOWED_ASSESSMENTS)},
                    "recommended_action": {"type": "string", "enum": list(ALLOWED_ACTIONS)},
                    "secondary_action": {"type": ["string", "null"], "enum": list(ALLOWED_ACTIONS) + [None]},
                    "justification": {"type": "string", "maxLength": 220},
                    "false_positive_risk": {"type": "string", "enum": list(ALLOWED_FALSE_POSITIVE_RISK)},
                    "confidence": {"type": "string", "enum": list(ALLOWED_CONFIDENCE)},
                },
                "required": [
                    "summary",
                    "evidence",
                    "assessment",
                    "recommended_action",
                    "secondary_action",
                    "justification",
                    "false_positive_risk",
                    "confidence",
                ],
                "additionalProperties": False,
            },
            separators=(",", ":"),
        )
        cmd = [
            str(runtime_path),
            "-m",
            str(model_path),
            "-n",
            str(profile.max_output_tokens),
            "--threads",
            "4",
            "--temp",
            "0",
            "--top-k",
            "1",
            "--top-p",
            "0.1",
            "--seed",
            "42",
            "--ctx-size",
            "1024",
            "--no-warmup",
            "--simple-io",
            "--no-display-prompt",
            "--no-conversation",
            "--reasoning",
            "off",
            "--json-schema",
            json_schema,
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

    @staticmethod
    def _format_size(size_bytes: int) -> str:
        if size_bytes <= 0:
            return "0 MB"
        if size_bytes >= 1024 * 1024 * 1024:
            return f"~{size_bytes / (1024 * 1024 * 1024):.1f} GB"
        return f"~{size_bytes / (1024 * 1024):.0f} MB"
