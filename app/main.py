"""Uboot GUI main application."""
from __future__ import annotations

import json
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtGui import QActionGroup, QColor
from PySide6.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QButtonGroup,
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFileDialog,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QRadioButton,
    QSizePolicy,
    QSplitter,
    QTableWidget,
    QTableWidgetItem,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from app.orchestrator import (
    Evidence,
    ForensicEntryRecord,
    ForensicReportData,
    HtmlReportGenerator,
    LlmAdvice,
    LlmAdvisor,
    LlmInstallResult,
    LlmMode,
    Remediation,
    Scanner,
    Scorer,
    SnapshotManager,
)
from app.orchestrator.llm_advisor import ComponentUnavailableError
from app.orchestrator.scanner import Entry, ScanResult
from app.orchestrator.scoring import RiskLevel, ScoredEntry
from app.orchestrator.snapshot import Snapshot, SnapshotDiff, SnapshotEntryChange, SnapshotSummary
from app.settings import SettingsStore


@dataclass
class RowContext:
    """A visible table row backed by a scored entry and optional diff metadata."""

    row_id: str
    scored_entry: ScoredEntry
    kind: str = "current"
    change: Optional[SnapshotEntryChange] = None


class ScanWorker(QThread):
    """Background worker for scanning."""

    progress = Signal(str)
    finished = Signal(ScanResult)
    error = Signal(str)

    def __init__(self, scanner: Scanner, sources: Optional[List[str]] = None):
        super().__init__()
        self.scanner = scanner
        self.sources = sources or ["all"]

    def run(self):
        """Run scan in background."""
        try:
            self.progress.emit("Starting scan...")
            result = self.scanner.scan(self.sources)
            self.finished.emit(result)
        except Exception as exc:
            self.error.emit(f"Scan failed: {exc}")


class LlmAdvisorWorker(QThread):
    """Background worker for local LLM advisory."""

    finished = Signal(object)
    unavailable = Signal(str)
    error = Signal(str)

    def __init__(self, advisor: LlmAdvisor, entry: Entry, scored_entry: ScoredEntry, evidence, mode: LlmMode):
        super().__init__()
        self.advisor = advisor
        self.entry = entry
        self.scored_entry = scored_entry
        self.evidence = evidence
        self.mode = mode

    def run(self):
        """Run local advisory in background."""
        try:
            advice = self.advisor.advise(
                entry=self.entry,
                scored_entry=self.scored_entry,
                evidence=self.evidence,
                mode=self.mode,
            )
            self.finished.emit(advice)
        except ComponentUnavailableError as exc:
            self.unavailable.emit(str(exc))
        except Exception as exc:
            self.error.emit(str(exc))


class LlmInstallWorker(QThread):
    """Background worker for downloading and installing local AI components."""

    progress = Signal(str, int)
    finished = Signal(object)
    error = Signal(str)

    def __init__(self, advisor: LlmAdvisor, mode: LlmMode):
        super().__init__()
        self.advisor = advisor
        self.mode = mode

    def run(self):
        """Install the local runtime and model for the selected mode."""
        try:
            result = self.advisor.ensure_component(self.mode, progress=self._emit_progress)
            self.finished.emit(result)
        except Exception as exc:
            self.error.emit(str(exc))

    def _emit_progress(self, message: str, percent: int):
        self.progress.emit(message, percent)


class LlmModeDialog(QDialog):
    """Short global LLM selection dialog shown from Get Evidence."""

    def __init__(self, advisor: LlmAdvisor, parent=None):
        super().__init__(parent)
        self.setWindowTitle("LLM Assistance")
        self.resize(520, 340)

        layout = QVBoxLayout(self)
        intro = QLabel(
            "Local LLM assistance can improve the evidence explanation and help justify "
            "a recommended remediation. Recommendations remain assisted: you keep control "
            "over the action."
        )
        intro.setWordWrap(True)
        layout.addWidget(intro)

        better_profile = advisor.get_profile(LlmMode.BETTER)
        info = QLabel(
            "Off\n"
            "Solo heurística local, sin asistencia LLM.\n\n"
            "Better\n"
            f"{better_profile.benefit_summary}\n"
            f"Hardware: {better_profile.minimum_ram} / {better_profile.recommended_ram}\n"
            f"Impact: {better_profile.estimated_ram} | {better_profile.estimated_latency}\n"
            f"Download: {better_profile.download_size}\n\n"
            "If the component is missing, Uboot will download and link it automatically in the background."
        )
        info.setWordWrap(True)
        layout.addWidget(info)

        self.button_group = QButtonGroup(self)
        self.off_radio = QRadioButton("Off")
        self.better_radio = QRadioButton("Better")
        self.off_radio.setChecked(True)
        for radio in (self.off_radio, self.better_radio):
            self.button_group.addButton(radio)
            layout.addWidget(radio)

        self.remember_checkbox = QCheckBox("Remember this choice")
        layout.addWidget(self.remember_checkbox)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def selected_mode(self) -> LlmMode:
        """Return the chosen LLM mode."""
        if self.better_radio.isChecked():
            return LlmMode.BETTER
        return LlmMode.OFF

    def remember_choice(self) -> bool:
        """Return whether the choice should be persisted."""
        return self.remember_checkbox.isChecked()


class LlmPreparationDialog(QDialog):
    """Non-modal banner/dialog shown while the local AI component is being prepared."""

    def __init__(self, advisor: LlmAdvisor, mode: LlmMode, parent=None):
        super().__init__(parent)
        profile = advisor.get_profile(mode)
        status = advisor.component_status(mode)

        self.setWindowTitle(f"Preparing {profile.display_name} Local AI")
        self.setModal(False)
        self.resize(560, 280)

        layout = QVBoxLayout(self)
        summary = QLabel(
            f"<strong>{profile.display_name}</strong><br>"
            f"{profile.benefit_summary}<br><br>"
            f"Minimum RAM: {profile.minimum_ram}<br>"
            f"Recommended RAM: {profile.recommended_ram}<br>"
            f"Expected latency: {profile.estimated_latency}<br>"
            f"Estimated extra RAM: {profile.estimated_ram}<br>"
            f"Estimated download: {status['estimated_download'] or profile.download_size}<br><br>"
            "Recommendations remain assisted. Uboot will not execute remediation automatically."
        )
        summary.setWordWrap(True)
        layout.addWidget(summary)

        self.progress_label = QLabel("Preparing local AI component...")
        layout.addWidget(self.progress_label)

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        layout.addWidget(self.progress_bar)

        self.detail_label = QLabel("")
        self.detail_label.setWordWrap(True)
        layout.addWidget(self.detail_label)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        buttons.accepted.connect(self.close)
        buttons.rejected.connect(self.close)
        layout.addWidget(buttons)

    def update_progress(self, message: str, percent: int):
        self.progress_label.setText(message)
        self.progress_bar.setValue(max(0, min(percent, 100)))

    def mark_ready(self, runtime_path: Path, model_path: Path):
        self.progress_label.setText("Local AI component ready.")
        self.progress_bar.setValue(100)
        self.detail_label.setText(f"Runtime: {runtime_path}\nModel: {model_path}")

    def mark_error(self, message: str):
        self.progress_label.setText("Local AI component failed to install.")
        self.detail_label.setText(message)


class SnapshotTimelineDialog(QDialog):
    """Modal timeline/history view for snapshot browsing and diff selection."""

    def __init__(self, snapshots: List[SnapshotSummary], parent=None):
        super().__init__(parent)
        self.setWindowTitle("Snapshots / Timeline")
        self.resize(840, 520)
        self.snapshots = snapshots
        self.selected_action: Optional[str] = None

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Available snapshots"))

        self.table = QTableWidget()
        self.table.setColumnCount(4)
        self.table.setHorizontalHeaderLabels(["Timestamp", "Label", "Entries", "Errors"])
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.setRowCount(len(snapshots))
        for row, item in enumerate(snapshots):
            self.table.setItem(row, 0, QTableWidgetItem(item.timestamp))
            self.table.setItem(row, 1, QTableWidgetItem(item.label or "(none)"))
            self.table.setItem(row, 2, QTableWidgetItem(str(item.entry_count)))
            self.table.setItem(row, 3, QTableWidgetItem(str(item.error_count)))
        self.table.itemSelectionChanged.connect(self._sync_from_table)
        layout.addWidget(self.table)

        selectors = QHBoxLayout()
        selectors.addWidget(QLabel("Current:"))
        self.current_combo = QComboBox()
        selectors.addWidget(self.current_combo)
        selectors.addWidget(QLabel("Previous:"))
        self.previous_combo = QComboBox()
        selectors.addWidget(self.previous_combo)
        layout.addLayout(selectors)

        for snapshot in snapshots:
            label = f"{snapshot.timestamp} | {snapshot.label or 'snapshot'}"
            self.current_combo.addItem(label, snapshot.path)
            self.previous_combo.addItem(label, snapshot.path)

        buttons = QDialogButtonBox()
        self.open_button = buttons.addButton("Open Snapshot", QDialogButtonBox.ButtonRole.AcceptRole)
        self.compare_button = buttons.addButton("Compare Snapshots", QDialogButtonBox.ButtonRole.ActionRole)
        self.cancel_button = buttons.addButton(QDialogButtonBox.StandardButton.Cancel)
        self.open_button.clicked.connect(self._accept_open)
        self.compare_button.clicked.connect(self._accept_compare)
        self.cancel_button.clicked.connect(self.reject)
        layout.addWidget(buttons)

        if snapshots:
            self.table.selectRow(0)
            self.previous_combo.setCurrentIndex(min(1, len(snapshots) - 1))

    def _sync_from_table(self):
        row = self.table.currentRow()
        if row >= 0:
            self.current_combo.setCurrentIndex(row)

    def _accept_open(self):
        self.selected_action = "open"
        self.accept()

    def _accept_compare(self):
        self.selected_action = "compare"
        self.accept()

    def selection(self) -> Optional[Tuple[str, Path, Optional[Path]]]:
        if self.selected_action is None:
            return None
        current_path = Path(self.current_combo.currentData())
        previous_path = Path(self.previous_combo.currentData()) if self.selected_action == "compare" else None
        return self.selected_action, current_path, previous_path


class MainWindow(QMainWindow):
    """Uboot main application window."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Uboot — Startup & Persistence Analyzer")
        self.setGeometry(100, 100, 1460, 940)

        self.scanner = Scanner()
        self.scorer = Scorer()
        self.evidence_gatherer = Evidence(self.scanner.uboot_core_exe)
        self.remediator = Remediation(self.scanner.uboot_core_exe)
        self.snapshot_manager = SnapshotManager()
        self.settings_store = SettingsStore()
        self.app_settings = self.settings_store.load()
        self.llm_advisor = LlmAdvisor()
        self.report_generator = HtmlReportGenerator()

        self.current_scan_result: Optional[ScanResult] = None
        self.current_snapshot: Optional[Snapshot] = None
        self.current_diff: Optional[SnapshotDiff] = None
        self.current_view_mode: str = "live_scan"
        self.current_view_title: str = "Live Scan"
        self.row_contexts: List[RowContext] = []
        self.scored_entries: List[ScoredEntry] = []
        self.selected_row_id: Optional[str] = None
        self.current_tx_id: Optional[str] = None
        self.current_evidence = None
        self.current_advice: Optional[LlmAdvice] = None
        self.pending_llm_row_id: Optional[str] = None
        self.pending_advice_request: Optional[Tuple[str, object, LlmMode]] = None
        self.evidence_cache: Dict[str, object] = {}
        self.advice_cache: Dict[Tuple[str, str], LlmAdvice] = {}
        self.session_llm_mode: Optional[LlmMode] = None
        self.llm_mode = self._load_configured_llm_mode()
        self.llm_mode_is_configured = bool(self.app_settings.llm_mode_configured)
        self._updating_llm_actions = False
        self.llm_install_worker: Optional[LlmInstallWorker] = None
        self.llm_install_mode: Optional[LlmMode] = None
        self.queued_install_mode: Optional[LlmMode] = None
        self.llm_prep_dialog: Optional[LlmPreparationDialog] = None

        self._setup_menu()
        self._setup_toolbar()
        self._setup_central_widget()
        self._setup_status_bar()
        self._connect_signals()
        self._sync_llm_component_settings()

    def _setup_menu(self):
        """Setup menu bar."""
        menubar = self.menuBar()

        file_menu = menubar.addMenu("&File")
        open_snapshot_action = file_menu.addAction("&Open Snapshot...")
        open_snapshot_action.triggered.connect(self._on_open_snapshot)
        timeline_action = file_menu.addAction("&Timeline...")
        timeline_action.triggered.connect(self._on_open_timeline)
        export_action = file_menu.addAction("Export Forensic &HTML...")
        export_action.triggered.connect(self._on_export_forensic_html)
        file_menu.addSeparator()
        exit_action = file_menu.addAction("E&xit")
        exit_action.triggered.connect(self.close)

        scan_menu = menubar.addMenu("&Scan")
        full_scan_action = scan_menu.addAction("&Full System Scan")
        full_scan_action.triggered.connect(lambda: self._on_start_scan(["all"]))
        registry_scan_action = scan_menu.addAction("&Registry Only")
        registry_scan_action.triggered.connect(lambda: self._on_start_scan(["RunRegistry"]))

        remediation_menu = menubar.addMenu("&Remediation")
        plan_action = remediation_menu.addAction("&Plan...")
        plan_action.triggered.connect(self._on_plan_remediation)
        apply_action = remediation_menu.addAction("&Apply")
        apply_action.triggered.connect(self._on_apply_remediation)

        llm_menu = menubar.addMenu("LLM Assistance")
        self.llm_action_group = QActionGroup(self)
        self.llm_action_group.setExclusive(True)
        self.llm_actions = {}
        for label, mode in (("Off", LlmMode.OFF), ("Better", LlmMode.BETTER)):
            action = llm_menu.addAction(label)
            action.setCheckable(True)
            action.triggered.connect(
                lambda checked=False, selected_mode=mode: self._on_llm_mode_selected(
                    selected_mode, persist=True, user_initiated=True
                )
            )
            self.llm_action_group.addAction(action)
            self.llm_actions[mode] = action
        self._sync_llm_menu_actions()

        help_menu = menubar.addMenu("&Help")
        about_action = help_menu.addAction("&About")
        about_action.triggered.connect(self._on_about)

    def _setup_toolbar(self):
        """Setup toolbar."""
        toolbar = self.addToolBar("Main")
        toolbar.setObjectName("MainToolbar")

        toolbar.addWidget(QLabel("Filter:"))
        self.source_combo = QComboBox()
        self.source_combo.addItems(["All", "Registry", "Services", "Tasks", "Logon"])
        toolbar.addWidget(self.source_combo)

        toolbar.addWidget(QLabel(" Name:"))
        self.name_filter = QLineEdit()
        self.name_filter.setPlaceholderText("Type to filter...")
        self.name_filter.setMaximumWidth(160)
        toolbar.addWidget(self.name_filter)

        toolbar.addWidget(QLabel(" Change:"))
        self.change_filter_combo = QComboBox()
        self.change_filter_combo.addItems(["All", "Current", "New", "Removed", "Changed"])
        toolbar.addWidget(self.change_filter_combo)

        toolbar.addSeparator()

        scan_btn = QPushButton("Scan")
        scan_btn.clicked.connect(lambda: self._on_start_scan(["all"]))
        toolbar.addWidget(scan_btn)

        details_btn = QPushButton("Details")
        details_btn.clicked.connect(self._on_show_details)
        toolbar.addWidget(details_btn)

        export_btn = QPushButton("Export HTML")
        export_btn.clicked.connect(self._on_export_forensic_html)
        toolbar.addWidget(export_btn)

        toolbar.addSeparator()

        remove_btn = QPushButton("Plan Removal")
        remove_btn.clicked.connect(self._on_plan_remediation)
        toolbar.addWidget(remove_btn)

        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        toolbar.addWidget(spacer)

    def _setup_central_widget(self):
        """Setup central widget with three panels."""
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(5, 5, 5, 5)

        left_layout = QVBoxLayout()
        self.entries_title = QLabel("Entries")
        left_layout.addWidget(self.entries_title)

        self.entries_table = QTableWidget()
        self.entries_table.setColumnCount(6)
        self.entries_table.setHorizontalHeaderLabels(["Name", "Score", "Risk", "Source", "Signed", "Change"])
        self.entries_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.entries_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.entries_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.entries_table.setColumnWidth(0, 250)
        self.entries_table.setColumnWidth(1, 60)
        self.entries_table.setColumnWidth(2, 100)
        self.entries_table.setColumnWidth(3, 220)
        self.entries_table.setColumnWidth(4, 70)
        self.entries_table.setColumnWidth(5, 90)
        self.entries_table.itemSelectionChanged.connect(self._on_entry_selected)
        self.entries_table.cellClicked.connect(lambda *_: self._on_entry_selected())
        left_layout.addWidget(self.entries_table)

        right_layout = QVBoxLayout()
        right_layout.addWidget(QLabel("Entry Details"))
        self.details_text = QTextEdit()
        self.details_text.setReadOnly(True)
        self.details_text.setMaximumHeight(300)
        self.details_text.setText(
            "Select an entry to view summary. Use Details for full context and Get Evidence for forensic output."
        )
        right_layout.addWidget(self.details_text)

        right_layout.addWidget(QLabel("LLM Advisory"))
        self.advisor_text = QTextEdit()
        self.advisor_text.setReadOnly(True)
        self.advisor_text.setMaximumHeight(260)
        right_layout.addWidget(self.advisor_text)
        self.advisor_progress_label = QLabel("")
        self.advisor_progress_label.setVisible(False)
        right_layout.addWidget(self.advisor_progress_label)
        self.advisor_progress_bar = QProgressBar()
        self.advisor_progress_bar.setVisible(False)
        self.advisor_progress_bar.setRange(0, 100)
        right_layout.addWidget(self.advisor_progress_bar)
        self._reset_advisor_panel()

        right_layout.addWidget(QLabel("Actions"))
        actions_layout = QHBoxLayout()
        evidence_btn = QPushButton("Get Evidence")
        evidence_btn.clicked.connect(self._on_get_evidence)
        actions_layout.addWidget(evidence_btn)
        remove_selected_btn = QPushButton("Plan Removal")
        remove_selected_btn.clicked.connect(self._on_plan_remediation)
        actions_layout.addWidget(remove_selected_btn)
        undo_btn = QPushButton("Undo")
        undo_btn.clicked.connect(self._on_undo_remediation)
        actions_layout.addWidget(undo_btn)
        right_layout.addLayout(actions_layout)

        right_layout.addWidget(QLabel("Progress"))
        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        right_layout.addWidget(self.progress_bar)
        right_layout.addStretch()

        left_widget = QWidget()
        left_widget.setLayout(left_layout)
        right_widget = QWidget()
        right_widget.setLayout(right_layout)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.addWidget(left_widget)
        splitter.addWidget(right_widget)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 2)
        main_layout.addWidget(splitter)

    def _setup_status_bar(self):
        self.statusBar().showMessage("Ready")

    def _connect_signals(self):
        self.name_filter.textChanged.connect(self._on_filter_entries)
        self.source_combo.currentTextChanged.connect(self._on_filter_entries)
        self.change_filter_combo.currentTextChanged.connect(self._on_filter_entries)

    def _load_configured_llm_mode(self) -> LlmMode:
        return self.llm_advisor.parse_mode(self.app_settings.llm_mode) or LlmMode.OFF

    def _persist_settings(self):
        self.settings_store.save(self.app_settings)

    def _sync_llm_component_settings(self):
        better_status = self.llm_advisor.component_status(LlmMode.BETTER)
        self.app_settings.llm_better_installed = bool(better_status["model_installed"])
        runtime_ready = bool(better_status["runtime_installed"])
        self.app_settings.llm_installed_runtime_version = (
            self.llm_advisor.installer.runtime_version if runtime_ready else ""
        )
        effective_mode = self._effective_llm_mode()
        if effective_mode == LlmMode.OFF:
            self.app_settings.llm_component_status = "unknown"
            self.app_settings.llm_component_error = ""
        else:
            status = self.llm_advisor.component_status(effective_mode)
            self.app_settings.llm_component_status = status["status"]
            self.app_settings.llm_component_error = "" if status["available"] else status["message"]
        self._persist_settings()

    def _set_advisor_progress(self, visible: bool, message: str = "", percent: int = 0):
        self.advisor_progress_label.setVisible(visible)
        self.advisor_progress_bar.setVisible(visible)
        if visible:
            self.advisor_progress_label.setText(message)
            self.advisor_progress_bar.setValue(max(0, min(percent, 100)))

    def _show_llm_preparation_dialog(self, mode: LlmMode):
        if self.llm_prep_dialog is None or self.llm_install_mode != mode:
            if self.llm_prep_dialog is not None:
                self.llm_prep_dialog.close()
            self.llm_prep_dialog = LlmPreparationDialog(self.llm_advisor, mode, self)
        self.llm_prep_dialog.show()
        self.llm_prep_dialog.raise_()
        self.llm_prep_dialog.activateWindow()

    def _start_llm_install(self, mode: LlmMode):
        self.llm_install_mode = mode
        self.llm_install_worker = LlmInstallWorker(self.llm_advisor, mode)
        self.llm_install_worker.progress.connect(self._on_llm_install_progress)
        self.llm_install_worker.finished.connect(self._on_llm_install_finished)
        self.llm_install_worker.error.connect(self._on_llm_install_error)
        self.llm_install_worker.start()

    def _ensure_llm_component(
        self,
        mode: LlmMode,
        pending_request: Optional[Tuple[str, object, LlmMode]] = None,
        show_banner: bool = True,
    ) -> bool:
        status = self.llm_advisor.component_status(mode)
        if status["available"]:
            self.app_settings.llm_component_status = "installed"
            self.app_settings.llm_component_error = ""
            self._sync_llm_component_settings()
            return True

        if pending_request is not None:
            self.pending_advice_request = pending_request

        if self.llm_install_worker is not None and self.llm_install_worker.isRunning():
            if self.llm_install_mode != mode:
                self.queued_install_mode = mode
            self.app_settings.llm_component_status = "installing"
            self.app_settings.llm_component_error = ""
            self._persist_settings()
            self._set_advisor_progress(True, "Preparing local AI component...", 5)
            self._set_advisor_html(
                f"<p><strong>{mode.value.capitalize()}</strong> mode is being prepared.</p>"
                "<p>Heuristic evidence remains available while the optional component installs.</p>"
            )
            if show_banner:
                self._show_llm_preparation_dialog(mode)
            return False

        self.app_settings.llm_component_status = "installing"
        self.app_settings.llm_component_error = ""
        self._persist_settings()
        self._set_advisor_progress(True, "Preparing local AI component...", 0)
        self._set_advisor_html(
            f"<p><strong>{mode.value.capitalize()}</strong> mode is being prepared.</p>"
            "<p>Heuristic evidence remains available while the optional component installs.</p>"
        )
        if show_banner:
            self._show_llm_preparation_dialog(mode)
        self.statusBar().showMessage(f"Preparing {mode.value.capitalize()} local AI component...")
        self._start_llm_install(mode)
        return False

    def _sync_llm_menu_actions(self):
        self._updating_llm_actions = True
        selected_mode = self._effective_llm_mode()
        for mode, action in self.llm_actions.items():
            action.setChecked(mode == selected_mode)
        self._updating_llm_actions = False

    def _effective_llm_mode(self) -> LlmMode:
        return self.session_llm_mode or self.llm_mode

    def _current_row_context(self) -> Optional[RowContext]:
        row = self.entries_table.currentRow()
        if row < 0:
            return None
        visible_rows = self._visible_row_contexts()
        if 0 <= row < len(visible_rows):
            return visible_rows[row]
        return None

    def _visible_row_contexts(self) -> List[RowContext]:
        """Return row contexts that match active filters."""
        filter_text = self.name_filter.text().strip().lower()
        source_text = self.source_combo.currentText().strip().lower()
        change_text = self.change_filter_combo.currentText().strip().lower()

        visible: List[RowContext] = []
        for row_ctx in self.row_contexts:
            entry = row_ctx.scored_entry.entry
            name_matches = filter_text in entry.name.lower()
            if source_text == "all":
                source_matches = True
            else:
                source_matches = source_text in entry.source.lower()

            kind_text = row_ctx.kind.lower()
            if change_text == "all":
                change_matches = True
            elif change_text == "current":
                change_matches = kind_text == "current"
            else:
                change_matches = kind_text == change_text

            if name_matches and source_matches and change_matches:
                visible.append(row_ctx)
        return visible

    def _score_entries(self, entries: List[Entry]) -> List[ScoredEntry]:
        return self.scorer.score(entries)

    def _build_row_contexts(self, scored_entries: List[ScoredEntry], kind: str = "current") -> List[RowContext]:
        row_contexts: List[RowContext] = []
        for index, scored_entry in enumerate(scored_entries):
            row_contexts.append(
                RowContext(
                    row_id=f"{kind}:{index}:{scored_entry.entry.entry_id or scored_entry.entry.name}",
                    scored_entry=scored_entry,
                    kind=kind,
                )
            )
        return row_contexts

    def _build_diff_row_contexts(self, diff: SnapshotDiff) -> List[RowContext]:
        row_contexts: List[RowContext] = []
        for index, entry in enumerate(diff.new_entries):
            row_contexts.append(
                RowContext(
                    row_id=f"new:{index}:{entry.entry_id or entry.name}",
                    scored_entry=self._score_entries([entry])[0],
                    kind="new",
                )
            )
        for index, entry in enumerate(diff.removed_entries):
            row_contexts.append(
                RowContext(
                    row_id=f"removed:{index}:{entry.entry_id or entry.name}",
                    scored_entry=self._score_entries([entry])[0],
                    kind="removed",
                )
            )
        for index, change in enumerate(diff.changed_entries):
            row_contexts.append(
                RowContext(
                    row_id=f"changed:{index}:{change.entry_after.entry_id or change.entry_after.name}",
                    scored_entry=self._score_entries([change.entry_after])[0],
                    kind="changed",
                    change=change,
                )
            )
        return row_contexts

    def _on_llm_mode_selected(self, mode: LlmMode, persist: bool, user_initiated: bool = False):
        if self._updating_llm_actions:
            return

        self.llm_mode = mode
        if persist:
            self.app_settings.llm_mode = mode.value
            self.app_settings.llm_mode_configured = True
            self.llm_mode_is_configured = True
            self.session_llm_mode = None
            self._persist_settings()
        else:
            self.session_llm_mode = mode

        self._sync_llm_menu_actions()
        self._reset_advisor_panel()
        if user_initiated and mode != LlmMode.OFF:
            self._ensure_llm_component(mode, show_banner=True)
        if user_initiated:
            self.statusBar().showMessage(f"LLM Assistance: {mode.value.capitalize()}")

    def _resolve_llm_mode_for_evidence(self) -> LlmMode:
        if self.llm_mode_is_configured:
            return self.llm_mode
        if self.session_llm_mode is not None:
            return self.session_llm_mode

        dialog = LlmModeDialog(self.llm_advisor, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return LlmMode.OFF

        selected_mode = dialog.selected_mode()
        self._on_llm_mode_selected(selected_mode, persist=dialog.remember_choice(), user_initiated=False)
        return selected_mode

    def _reset_advisor_panel(self):
        self._set_advisor_progress(False)
        if not self.llm_mode_is_configured and self.session_llm_mode is None:
            self._set_advisor_html(
                "<p>Configure <strong>LLM Assistance</strong> from the menu or when you use Get Evidence.</p>"
            )
            return

        mode = self._effective_llm_mode()
        if mode == LlmMode.OFF:
            self._set_advisor_html(
                "<p><strong>LLM Assistance:</strong> Off</p><p>Heuristic evidence remains available.</p>"
            )
            return

        status = self.llm_advisor.component_status(mode)
        if status["available"]:
            self._set_advisor_html(
                f"<p><strong>{mode.value.capitalize()}</strong> mode ready.</p>"
                "<p>Use Get Evidence to request local advisory.</p>"
            )
        elif self.app_settings.llm_component_status == "installing":
            self._set_advisor_html(
                f"<p><strong>{mode.value.capitalize()}</strong> mode is being prepared.</p>"
                "<p>Heuristic evidence remains available while the optional component installs.</p>"
            )
            self._set_advisor_progress(True, "Preparing local AI component...", 5)
        else:
            self._set_advisor_html(
                f"<p><strong>{mode.value.capitalize()}</strong> mode selected, but the optional component is not ready.</p>"
                f"<p>Status: {self._escape_html(status['status'])}</p>"
                f"<p>Estimated download: {self._escape_html(status['estimated_download'])}</p>"
                f"<pre>{self.llm_advisor.install_instructions(mode)}</pre>"
            )

    def _on_start_scan(self, sources: List[str]):
        self.statusBar().showMessage("Scanning...")
        self.progress_bar.setVisible(True)
        self.progress_bar.setValue(0)
        self.scan_worker = ScanWorker(self.scanner, sources)
        self.scan_worker.progress.connect(self.statusBar().showMessage)
        self.scan_worker.finished.connect(self._on_scan_complete)
        self.scan_worker.error.connect(self._on_scan_error)
        self.scan_worker.start()

    def _on_scan_complete(self, result: ScanResult):
        self.current_scan_result = result
        self.current_tx_id = None
        self.current_evidence = None
        self.current_advice = None
        self.current_diff = None
        self.current_view_mode = "live_scan"
        self.current_view_title = "Live Scan"
        self.statusBar().showMessage(f"Scan complete: {len(result.entries)} entries")
        self.progress_bar.setVisible(False)

        self.current_snapshot = self.snapshot_manager.save(result, label="ui-scan")
        self.scored_entries = self._score_entries(result.entries)
        self.row_contexts = self._build_row_contexts(self.scored_entries, kind="current")
        self._populate_entries_table()
        self._reset_advisor_panel()

    def _on_scan_error(self, error_msg: str):
        self.statusBar().showMessage(error_msg)
        self.progress_bar.setVisible(False)
        QMessageBox.critical(self, "Scan Error", error_msg)

    def _populate_entries_table(self):
        visible = self._visible_row_contexts()
        self.entries_table.clearSelection()
        self.entries_table.setRowCount(len(visible))
        self.entries_title.setText(f"Entries — {self.current_view_title}")
        self.selected_row_id = None

        for row, row_ctx in enumerate(visible):
            scored_entry = row_ctx.scored_entry
            entry = scored_entry.entry

            self.entries_table.setItem(row, 0, QTableWidgetItem(entry.name))

            score_item = QTableWidgetItem(str(scored_entry.score))
            score_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.entries_table.setItem(row, 1, score_item)

            risk_item = QTableWidgetItem(scored_entry.risk_level.value)
            risk_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            if scored_entry.risk_level == RiskLevel.MALICIOUS:
                risk_item.setForeground(QColor("red"))
            elif scored_entry.risk_level == RiskLevel.SUSPICIOUS:
                risk_item.setForeground(QColor("orange"))
            else:
                risk_item.setForeground(QColor("green"))
            self.entries_table.setItem(row, 2, risk_item)

            self.entries_table.setItem(row, 3, QTableWidgetItem(entry.source))

            signed_item = QTableWidgetItem(self._format_signed_status(entry))
            signed_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            if signed_item.text() == "Yes":
                signed_item.setForeground(QColor("green"))
            elif signed_item.text() in {"No", "Invalid", "Missing"}:
                signed_item.setForeground(QColor("orange"))
            else:
                signed_item.setForeground(QColor("gray"))
            self.entries_table.setItem(row, 4, signed_item)

            change_label = row_ctx.kind.capitalize()
            if row_ctx.kind == "changed" and row_ctx.change:
                change_label = f"Changed ({len(row_ctx.change.changed_fields)})"
            self.entries_table.setItem(row, 5, QTableWidgetItem(change_label))

    def _select_row_by_id(self, row_id: str):
        """Restore selection to a row context after the table is rebuilt."""
        visible = self._visible_row_contexts()
        for index, row_ctx in enumerate(visible):
            if row_ctx.row_id == row_id:
                self.entries_table.selectRow(index)
                return

    @staticmethod
    def _format_signed_status(entry: Entry) -> str:
        metadata = entry.metadata or {}
        signature_status = str(metadata.get("signature_status", "")).strip().lower()
        signed = metadata.get("signed")
        if signed is True or signature_status in {"valid", "ok", "trusted", "success"}:
            return "Yes"
        if signature_status == "missingfile":
            return "Missing"
        if signed is False and signature_status and signature_status not in {"notsigned", "unsigned"}:
            return "Invalid"
        if signed is False:
            return "No"
        return "Unknown"

    def _on_entry_selected(self):
        row_ctx = self._current_row_context()
        if row_ctx is None:
            self.selected_row_id = None
            return

        self.selected_row_id = row_ctx.row_id
        self.current_evidence = self.evidence_cache.get(row_ctx.scored_entry.entry.entry_id)
        self.current_advice = self._get_cached_advice(
            row_ctx.scored_entry,
            self.current_evidence,
            self._effective_llm_mode(),
        )
        self.pending_llm_row_id = None
        self._update_details_panel()
        self._render_cached_advice_or_reset(row_ctx)

    @staticmethod
    def _build_details_text(row_ctx: RowContext, include_metadata: bool = True) -> str:
        scored_entry = row_ctx.scored_entry
        entry = scored_entry.entry
        details = f"""
Entry: {entry.name}
ID: {entry.entry_id}
Source: {entry.source}
Command: {entry.command}
Image Path: {entry.image_path}
Status: {entry.status}
View Kind: {row_ctx.kind}

Score: {scored_entry.score}/100
Risk: {scored_entry.risk_level.value.upper()}
Explanation: {scored_entry.explanation}

Signals ({len(scored_entry.signals)}):
  {', '.join(scored_entry.signals) if scored_entry.signals else '(none)'}

Rule Matches ({len(scored_entry.rule_matches)}):
  {', '.join(scored_entry.rule_matches) if scored_entry.rule_matches else '(none)'}
        """.strip()

        if row_ctx.change:
            details += (
                "\n\nChanged Fields:\n  "
                + ", ".join(row_ctx.change.changed_fields)
                + "\nPrevious Entry:\n"
                + json.dumps(
                    {
                        "entry_id": row_ctx.change.entry_before.entry_id,
                        "name": row_ctx.change.entry_before.name,
                        "command": row_ctx.change.entry_before.command,
                        "image_path": row_ctx.change.entry_before.image_path,
                        "source": row_ctx.change.entry_before.source,
                        "status": row_ctx.change.entry_before.status,
                        "metadata": row_ctx.change.entry_before.metadata,
                    },
                    indent=2,
                )
            )

        if include_metadata:
            details += "\n\nMetadata:\n" + json.dumps(entry.metadata, indent=2)
        return details

    def _update_details_panel(self, include_metadata: bool = True):
        row_ctx = self._current_row_context()
        if row_ctx is None:
            self.details_text.setText(
                "Select an entry to view summary. Use Details for full context and Get Evidence for forensic output."
            )
            return
        self.details_text.setText(self._build_details_text(row_ctx, include_metadata=include_metadata))

    def _render_cached_advice_or_reset(self, row_ctx: RowContext):
        if row_ctx.kind == "removed":
            self._set_advisor_html(
                "<p><strong>Historical removed entry.</strong></p><p>LLM advice is only available for current evidence gathered from the live system.</p>"
            )
            return

        if self.current_advice:
            self._render_advice(self.current_advice)
            return

        entry_id = row_ctx.scored_entry.entry.entry_id
        cached = self._get_cached_advice(
            row_ctx.scored_entry,
            self.evidence_cache.get(entry_id),
            self._effective_llm_mode(),
        )
        if cached:
            self.current_advice = cached
            self._render_advice(cached)
            return

        if self.app_settings.llm_component_status == "installing" and self._effective_llm_mode() != LlmMode.OFF:
            self._set_advisor_html(
                "<p><strong>Preparing local AI component...</strong></p>"
                "<p>Heuristic evidence remains available while the download completes.</p>"
            )
            self._set_advisor_progress(True, "Preparing local AI component...", 5)
            return

        self._reset_advisor_panel()

    def _render_advice(self, advice: LlmAdvice):
        sections = advice.to_display_sections()
        evidence_list = "".join(
            f"<li>{self._escape_html(item)}</li>" for item in sections["evidence"]
        ) or "<li>(none)</li>"
        self._set_advisor_html(
            f"<p><strong>Summary:</strong> {self._escape_html(sections['summary'])}</p>"
            f"<p><strong>Assessment:</strong> {self._escape_html(sections['assessment'])}</p>"
            f"<p><strong>Recommended Action:</strong> {self._escape_html(sections['recommended_action'])}</p>"
            f"<p><strong>Secondary Action:</strong> {self._escape_html(sections['secondary_action'])}</p>"
            f"<p><strong>Confidence:</strong> {self._escape_html(sections['confidence'])}</p>"
            f"<p><strong>False Positive Risk:</strong> {self._escape_html(sections['false_positive_risk'])}</p>"
            f"<p><strong>Justification:</strong> {self._escape_html(sections['justification'])}</p>"
            f"<p><strong>Evidence:</strong></p><ul>{evidence_list}</ul>"
        )

    @staticmethod
    def _escape_html(value: str) -> str:
        return str(value).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    def _set_advisor_html(self, html: str):
        self.advisor_text.setHtml(html)
        scrollbar = self.advisor_text.verticalScrollBar()
        if scrollbar is not None:
            scrollbar.setValue(0)

    def _advice_cache_key(
        self,
        scored_entry: ScoredEntry,
        evidence,
        mode: LlmMode,
    ) -> Tuple[str, str]:
        fingerprint = self.llm_advisor.cache_fingerprint(
            scored_entry.entry,
            scored_entry,
            evidence,
            mode,
        )
        return (scored_entry.entry.entry_id, fingerprint)

    def _get_cached_advice(
        self,
        scored_entry: ScoredEntry,
        evidence,
        mode: LlmMode,
    ) -> Optional[LlmAdvice]:
        if mode == LlmMode.OFF:
            return None
        return self.advice_cache.get(self._advice_cache_key(scored_entry, evidence, mode))

    def _on_filter_entries(self, text: str = ""):
        self._populate_entries_table()

    def _selected_scored_entry(self) -> Optional[ScoredEntry]:
        row_ctx = self._current_row_context()
        if row_ctx is None:
            return None
        return row_ctx.scored_entry

    def _on_show_details(self):
        row_ctx = self._current_row_context()
        if row_ctx is None:
            QMessageBox.information(self, "No Selection", "Select a row in the table first.")
            return

        self._update_details_panel(include_metadata=True)
        details_dialog = QDialog(self)
        details_dialog.setWindowTitle("Entry Details")
        details_dialog.resize(900, 650)
        layout = QVBoxLayout(details_dialog)
        details_view = QTextEdit()
        details_view.setReadOnly(True)
        details_view.setText(self._build_details_text(row_ctx, include_metadata=True))
        layout.addWidget(details_view)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok)
        buttons.accepted.connect(details_dialog.accept)
        layout.addWidget(buttons)
        details_dialog.exec()

    def _on_get_evidence(self):
        row_ctx = self._current_row_context()
        if row_ctx is None:
            QMessageBox.warning(self, "No Selection", "Please select an entry first.")
            return
        if row_ctx.kind == "removed":
            QMessageBox.information(
                self,
                "Historical Entry",
                "Removed historical entries cannot collect live evidence. Load a current scan or snapshot entry instead.",
            )
            return

        scored_entry = row_ctx.scored_entry
        mode = self._resolve_llm_mode_for_evidence()

        try:
            evidence = self.evidence_gatherer.get_evidence(scored_entry.entry)
        except Exception as exc:
            QMessageBox.critical(self, "Evidence Error", str(exc))
            return

        self.current_evidence = evidence
        self.current_advice = None
        self.evidence_cache[scored_entry.entry.entry_id] = evidence
        scored_entry.entry.metadata.update(evidence.to_metadata_updates())
        selected_row_id = row_ctx.row_id
        self._update_details_panel(include_metadata=True)
        self._populate_entries_table()
        self._select_row_by_id(selected_row_id)

        evidence_dialog = QDialog(self)
        evidence_dialog.setWindowTitle("Evidence Output")
        evidence_dialog.resize(1000, 700)
        layout = QVBoxLayout(evidence_dialog)
        header = QLabel(
            f"Entry ID: {scored_entry.entry.entry_id}\n"
            f"Collected: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
        )
        layout.addWidget(header)
        evidence_view = QTextEdit()
        evidence_view.setReadOnly(True)
        evidence_view.setText(json.dumps(evidence.raw_evidence or {}, indent=2))
        layout.addWidget(evidence_view)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok)
        buttons.accepted.connect(evidence_dialog.accept)
        layout.addWidget(buttons)
        evidence_dialog.exec()

        self.statusBar().showMessage(f"Evidence loaded for {scored_entry.entry.entry_id}")

        if mode == LlmMode.OFF:
            self._set_advisor_html(
                "<p><strong>LLM Assistance:</strong> Off</p><p>Evidence and heuristic explanation remain available.</p>"
            )
            return

        cache_key = self._advice_cache_key(scored_entry, evidence, mode)
        cached = self.advice_cache.get(cache_key)
        if cached:
            self.current_advice = cached
            self._render_advice(cached)
            self.statusBar().showMessage("Loaded cached local advisory")
            return

        if not self._ensure_llm_component(
            mode,
            pending_request=(row_ctx.row_id, evidence, mode),
            show_banner=True,
        ):
            self._set_advisor_html(
                f"<p><strong>{mode.value.capitalize()}</strong> mode is being prepared.</p>"
                "<p>Continuing with heuristic evidence while the optional component installs.</p>"
            )
            self._set_advisor_progress(True, "Preparing local AI component...", 5)
            return

        self._set_advisor_html("<p>Loading local advisory...</p>")
        self._set_advisor_progress(False)
        self._start_llm_advisory_worker(row_ctx, evidence, mode)

    def _start_llm_advisory_worker(self, row_ctx: RowContext, evidence, mode: LlmMode):
        self.pending_llm_row_id = row_ctx.row_id
        self.llm_worker = LlmAdvisorWorker(
            advisor=self.llm_advisor,
            entry=row_ctx.scored_entry.entry,
            scored_entry=row_ctx.scored_entry,
            evidence=evidence,
            mode=mode,
        )
        self.llm_worker.finished.connect(self._on_llm_advice_ready)
        self.llm_worker.unavailable.connect(self._on_llm_advice_unavailable)
        self.llm_worker.error.connect(self._on_llm_advice_error)
        self.llm_worker.start()

    def _on_llm_install_progress(self, message: str, percent: int):
        self._set_advisor_progress(True, message, percent)
        self.statusBar().showMessage(message)
        if self.llm_prep_dialog is not None:
            self.llm_prep_dialog.update_progress(message, percent)

    def _on_llm_install_finished(self, result: LlmInstallResult):
        self.app_settings.llm_component_status = "installed"
        self.app_settings.llm_component_error = ""
        self._sync_llm_component_settings()
        self._set_advisor_progress(False)
        if self.llm_prep_dialog is not None:
            self.llm_prep_dialog.mark_ready(result.runtime_path, result.model_path)
        self.statusBar().showMessage(f"{result.mode.value.capitalize()} local AI component ready")

        pending = self.pending_advice_request
        if pending and pending[2] == result.mode:
            self.pending_advice_request = None
        self.llm_install_worker = None
        completed_mode = self.llm_install_mode
        self.llm_install_mode = None

        if pending and pending[2] == result.mode:
            row_ctx = self._current_row_context()
            if row_ctx is not None and row_ctx.row_id == pending[0]:
                self._set_advisor_html("<p>Loading local advisory...</p>")
                self._start_llm_advisory_worker(row_ctx, pending[1], result.mode)
            else:
                self._reset_advisor_panel()
        else:
            self._reset_advisor_panel()

        if self.queued_install_mode and self.queued_install_mode != completed_mode:
            next_mode = self.queued_install_mode
            self.queued_install_mode = None
            if not self.llm_advisor.component_status(next_mode)["available"]:
                self._ensure_llm_component(next_mode, show_banner=True)

    def _on_llm_install_error(self, message: str):
        self.app_settings.llm_component_status = "error"
        self.app_settings.llm_component_error = message
        self._persist_settings()
        self._set_advisor_progress(False)
        if self.llm_prep_dialog is not None:
            self.llm_prep_dialog.mark_error(message)
        self.llm_install_worker = None
        self.llm_install_mode = None
        self.queued_install_mode = None
        self.pending_advice_request = None
        self._set_advisor_html(
            "<p><strong>LLM component install failed.</strong></p>"
            "<p>Falling back to heuristic evidence.</p>"
            f"<pre>{self._escape_html(message)}</pre>"
        )
        self.statusBar().showMessage("Local AI component install failed; using heuristics")

    def _on_llm_advice_ready(self, advice: LlmAdvice):
        row_ctx = self._current_row_context()
        if row_ctx is None or (self.pending_llm_row_id and self.pending_llm_row_id != row_ctx.row_id):
            return
        self.current_advice = advice
        self.pending_llm_row_id = None
        entry_id = row_ctx.scored_entry.entry.entry_id
        cache_key = self._advice_cache_key(
            row_ctx.scored_entry,
            self.evidence_cache.get(entry_id),
            self._effective_llm_mode(),
        )
        self.advice_cache[cache_key] = advice
        self.app_settings.llm_component_status = "installed"
        self.app_settings.llm_component_error = ""
        self._sync_llm_component_settings()
        self._set_advisor_progress(False)
        self._render_advice(advice)
        self.statusBar().showMessage("Local advisory ready")

    def _on_llm_advice_unavailable(self, message: str):
        row_ctx = self._current_row_context()
        if row_ctx is None or (self.pending_llm_row_id and self.pending_llm_row_id != row_ctx.row_id):
            return
        self.app_settings.llm_component_status = "missing"
        self.app_settings.llm_component_error = message
        self.pending_llm_row_id = None
        self._persist_settings()
        self._set_advisor_progress(False)
        self._set_advisor_html(
            "<p><strong>LLM advisory unavailable.</strong></p>"
            "<p>Falling back to heuristic evidence.</p>"
            f"<pre>{self._escape_html(message)}</pre>"
        )
        self.statusBar().showMessage("LLM advisory unavailable; using heuristics")

    def _on_llm_advice_error(self, message: str):
        row_ctx = self._current_row_context()
        if row_ctx is None or (self.pending_llm_row_id and self.pending_llm_row_id != row_ctx.row_id):
            return
        self.app_settings.llm_component_status = "error"
        self.app_settings.llm_component_error = message
        self.pending_llm_row_id = None
        self._persist_settings()
        self._set_advisor_progress(False)
        self._set_advisor_html(
            "<p><strong>LLM advisory failed.</strong></p>"
            "<p>Falling back to heuristic evidence.</p>"
            f"<pre>{self._escape_html(message)}</pre>"
        )
        self.statusBar().showMessage("LLM advisory failed; using heuristics")

    def _on_plan_remediation(self):
        row_ctx = self._current_row_context()
        if row_ctx is None:
            QMessageBox.warning(self, "No Selection", "Please select an entry first.")
            return
        if row_ctx.kind == "removed":
            QMessageBox.information(
                self,
                "Historical Entry",
                "Removed historical entries cannot be remediated from the current system view.",
            )
            return

        scored_entry = row_ctx.scored_entry
        suggested_action = self.current_advice.recommended_action if self.current_advice else None
        default_reason = f"Risk {scored_entry.score}/100 - {scored_entry.risk_level.value}"
        if suggested_action and suggested_action != "no_action":
            default_reason += f" | Advisor suggested: {suggested_action}"

        reason, ok = QInputDialog.getText(
            self,
            "Plan Remediation",
            "Reason for remediation:",
            text=default_reason,
        )
        if not ok:
            return

        try:
            plan = self.remediator.plan(
                [scored_entry.entry.entry_id],
                reason=reason or "User-initiated remediation",
            )
        except Exception as exc:
            QMessageBox.critical(self, "Plan Error", str(exc))
            return

        self.current_tx_id = plan.tx_id
        plan_summary = {
            "tx_id": plan.tx_id,
            "entry_ids": plan.entry_ids,
            "reason": plan.reason,
            "executed": plan.executed,
            "operations": [
                {
                    "type": op.op_type,
                    "target": op.target,
                    "action": op.action,
                    "op_id": op.op_id,
                    "metadata": op.metadata,
                }
                for op in plan.operations
            ],
        }
        self.details_text.append("\n\n=== Remediation Plan ===\n" + json.dumps(plan_summary, indent=2))
        self.statusBar().showMessage(f"Plan created: {plan.tx_id}")
        QMessageBox.information(
            self,
            "Plan Created",
            f"Transaction {plan.tx_id} created with {len(plan.operations)} operation(s).",
        )

    def _on_apply_remediation(self):
        tx_id = self.current_tx_id
        if not tx_id:
            tx_id, ok = QInputDialog.getText(self, "Apply Remediation", "Transaction ID:")
            if not ok or not tx_id.strip():
                return
            tx_id = tx_id.strip()

        confirm = QMessageBox.question(self, "Confirm Apply", f"Apply remediation transaction {tx_id}?")
        if confirm != QMessageBox.StandardButton.Yes:
            return

        try:
            applied = self.remediator.apply(tx_id)
        except Exception as exc:
            QMessageBox.critical(self, "Apply Error", str(exc))
            return

        if applied:
            self.statusBar().showMessage(f"Applied transaction: {tx_id}")
            QMessageBox.information(self, "Apply", f"Transaction applied: {tx_id}")
        else:
            QMessageBox.warning(self, "Apply", f"Transaction apply failed: {tx_id}")

    def _on_undo_remediation(self):
        tx_id, ok = QInputDialog.getText(
            self,
            "Undo Remediation",
            "Transaction ID to undo:",
            text=self.current_tx_id or "",
        )
        if not ok or not tx_id.strip():
            return
        tx_id = tx_id.strip()
        confirm = QMessageBox.question(self, "Confirm Undo", f"Undo remediation transaction {tx_id}?")
        if confirm != QMessageBox.StandardButton.Yes:
            return

        try:
            undone = self.remediator.undo(tx_id)
        except Exception as exc:
            QMessageBox.critical(self, "Undo Error", str(exc))
            return

        if undone:
            self.statusBar().showMessage(f"Undone transaction: {tx_id}")
            QMessageBox.information(self, "Undo", f"Transaction undone: {tx_id}")
        else:
            QMessageBox.warning(self, "Undo", f"Transaction undo failed: {tx_id}")

    def _load_snapshot_into_view(self, snapshot: Snapshot, title: str):
        self.current_snapshot = snapshot
        self.current_diff = None
        self.current_scan_result = ScanResult(entries=snapshot.entries, errors=[])
        self.current_view_mode = "snapshot"
        self.current_view_title = title
        self.current_evidence = None
        self.current_advice = None
        self.scored_entries = self._score_entries(snapshot.entries)
        self.row_contexts = self._build_row_contexts(self.scored_entries, kind="current")
        self._populate_entries_table()
        self._reset_advisor_panel()
        self.statusBar().showMessage(f"Loaded snapshot with {len(snapshot.entries)} entries")

    def _load_diff_into_view(self, diff: SnapshotDiff, current_snapshot: Snapshot, title: str):
        self.current_snapshot = current_snapshot
        self.current_diff = diff
        self.current_scan_result = None
        self.current_view_mode = "diff"
        self.current_view_title = title
        self.current_evidence = None
        self.current_advice = None
        self.scored_entries = []
        self.row_contexts = self._build_diff_row_contexts(diff)
        self._populate_entries_table()
        self._reset_advisor_panel()
        self.statusBar().showMessage(
            f"Loaded diff: {len(diff.new_entries)} new, {len(diff.removed_entries)} removed, {len(diff.changed_entries)} changed"
        )

    def _on_open_snapshot(self):
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open Snapshot",
            str(self.snapshot_manager.snapshot_dir),
            "JSON Files (*.json);;All Files (*)",
        )
        if not file_path:
            return
        try:
            snapshot = self.snapshot_manager.load_snapshot(Path(file_path))
            self._load_snapshot_into_view(snapshot, f"Snapshot {snapshot.timestamp}")
        except Exception as exc:
            QMessageBox.critical(self, "Open Snapshot Error", str(exc))

    def _on_open_timeline(self):
        snapshots = self.snapshot_manager.list_snapshots()
        if not snapshots:
            QMessageBox.information(self, "Timeline", "No snapshots are available yet.")
            return

        dialog = SnapshotTimelineDialog(snapshots, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        selection = dialog.selection()
        if selection is None:
            return

        action, current_path, previous_path = selection
        try:
            current_snapshot = self.snapshot_manager.load_snapshot(current_path)
            if action == "open":
                self._load_snapshot_into_view(current_snapshot, f"Snapshot {current_snapshot.timestamp}")
                return

            if previous_path is None:
                raise RuntimeError("A previous snapshot is required for diff mode")

            previous_snapshot = self.snapshot_manager.load_snapshot(previous_path)
            diff = self.snapshot_manager.diff(current_snapshot, previous_snapshot)
            self._load_diff_into_view(
                diff,
                current_snapshot=current_snapshot,
                title=f"Diff {previous_snapshot.timestamp} → {current_snapshot.timestamp}",
            )
        except Exception as exc:
            QMessageBox.critical(self, "Timeline Error", str(exc))

    def _build_report_records(self, visible_rows: List[RowContext]) -> List[ForensicEntryRecord]:
        records: List[ForensicEntryRecord] = []
        for row_ctx in visible_rows:
            scored_entry = row_ctx.scored_entry
            entry = scored_entry.entry
            evidence = self.evidence_cache.get(entry.entry_id)
            advice = None
            for mode in (LlmMode.BETTER,):
                advice = self._get_cached_advice(scored_entry, evidence, mode)
                if advice:
                    break

            previous_entry = None
            changed_fields: List[str] = []
            if row_ctx.change:
                previous_entry = {
                    "entry_id": row_ctx.change.entry_before.entry_id,
                    "name": row_ctx.change.entry_before.name,
                    "command": row_ctx.change.entry_before.command,
                    "image_path": row_ctx.change.entry_before.image_path,
                    "source": row_ctx.change.entry_before.source,
                    "status": row_ctx.change.entry_before.status,
                    "metadata": row_ctx.change.entry_before.metadata,
                }
                changed_fields = list(row_ctx.change.changed_fields)

            records.append(
                ForensicEntryRecord(
                    entry_id=entry.entry_id,
                    name=entry.name,
                    source=entry.source,
                    command=entry.command,
                    image_path=entry.image_path,
                    status=entry.status,
                    score=scored_entry.score,
                    risk_level=scored_entry.risk_level.value,
                    explanation=scored_entry.explanation,
                    signals=list(scored_entry.signals),
                    rule_matches=list(scored_entry.rule_matches),
                    metadata=dict(entry.metadata or {}),
                    evidence=(evidence.raw_evidence if evidence else None),
                    advice=advice,
                    change_kind=row_ctx.kind,
                    changed_fields=changed_fields,
                    previous_entry=previous_entry,
                )
            )
        return records

    def _build_report_data(self) -> ForensicReportData:
        visible_rows = self._visible_row_contexts()
        timeline = [
            {
                "timestamp": summary.timestamp,
                "label": summary.label,
                "entry_count": summary.entry_count,
                "error_count": summary.error_count,
            }
            for summary in self.snapshot_manager.list_snapshots()[:10]
        ]
        summary = {
            "view_mode": self.current_view_mode,
            "visible_entries": len(visible_rows),
            "cached_evidence": len(self.evidence_cache),
            "cached_advice": len(self.advice_cache),
        }
        metadata = {
            "view_title": self.current_view_title,
            "snapshot_timestamp": self.current_snapshot.timestamp if self.current_snapshot else "",
            "diff_previous": self.current_diff.timestamp_previous if self.current_diff else "",
            "diff_current": self.current_diff.timestamp_current if self.current_diff else "",
            "new_entries": len(self.current_diff.new_entries) if self.current_diff else 0,
            "removed_entries": len(self.current_diff.removed_entries) if self.current_diff else 0,
            "changed_entries": len(self.current_diff.changed_entries) if self.current_diff else 0,
            "unchanged_entries": self.current_diff.unchanged_entry_count if self.current_diff else 0,
        }
        return ForensicReportData(
            title=f"Uboot Forensic Report — {self.current_view_title}",
            source_type=self.current_view_mode,
            generated_at=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            summary=summary,
            metadata=metadata,
            timeline=timeline,
            entries=self._build_report_records(visible_rows),
        )

    def _on_export_forensic_html(self):
        report = self._build_report_data()
        target_dir = QFileDialog.getExistingDirectory(self, "Choose export folder", str(Path.cwd()))
        if not target_dir:
            return

        safe_title = self.current_view_mode.replace(" ", "_")
        filename = f"uboot-report-{safe_title}-{datetime.now().strftime('%Y%m%d-%H%M%S')}.html"
        output_path = Path(target_dir) / filename
        try:
            self.report_generator.write_html(report, output_path)
        except Exception as exc:
            QMessageBox.critical(self, "Export Error", str(exc))
            return

        QMessageBox.information(self, "Export Complete", f"Forensic HTML exported to:\n{output_path}")
        self.statusBar().showMessage(f"Exported report: {output_path.name}")

    def _on_about(self):
        QMessageBox.about(
            self,
            "About Uboot",
            "Uboot v1.0\n\n"
            "Autoruns-style analyzer with AI scoring, timeline support, and forensic export.\n"
            "Built with Python, PySide6, and C++17.",
        )


def main():
    """Main entry point for GUI."""
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
