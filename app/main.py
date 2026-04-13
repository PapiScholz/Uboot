"""Uboot GUI main application."""
import sys
import json
from pathlib import Path
from typing import Optional, List

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QTableWidget, QTableWidgetItem, QLabel, QPushButton,
    QMenu, QMenuBar, QToolBar, QTextEdit, QComboBox, QLineEdit,
    QDialog, QDialogButtonBox, QMessageBox, QProgressBar, QStatusBar
)
from PySide6.QtCore import Qt, QThread, Signal, QTimer
from PySide6.QtGui import QColor, QIcon

from app.orchestrator import Scanner, Scorer, Evidence, Remediation, SnapshotManager
from app.orchestrator.scanner import ScanResult
from app.orchestrator.scoring import ScoredEntry, RiskLevel


class ScanWorker(QThread):
    """Background worker for scanning."""
    
    progress = Signal(str)
    finished = Signal(ScanResult)
    error = Signal(str)

    def __init__(self, scanner: Scanner, sources: List[str] = None):
        super().__init__()
        self.scanner = scanner
        self.sources = sources or ["all"]

    def run(self):
        """Run scan in background."""
        try:
            self.progress.emit("Starting scan...")
            result = self.scanner.scan(self.sources)
            self.finished.emit(result)
        except Exception as e:
            self.error.emit(f"Scan failed: {str(e)}")


class MainWindow(QMainWindow):
    """Uboot main application window."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Uboot — Startup & Persistence Analyzer")
        self.setGeometry(100, 100, 1400, 900)

        # Components
        self.scanner = Scanner()
        self.scorer = Scorer()
        self.evidence_gatherer = Evidence()
        self.remediator = Remediation()
        self.snapshot_manager = SnapshotManager()

        # State
        self.current_scan_result: Optional[ScanResult] = None
        self.scored_entries: List[ScoredEntry] = []
        self.selected_entry_id: Optional[str] = None

        # Setup UI
        self._setup_menu()
        self._setup_toolbar()
        self._setup_central_widget()
        self._setup_status_bar()

        # Connect signals
        self._connect_signals()

    def _setup_menu(self):
        """Setup menu bar."""
        menubar = self.menuBar()

        # File menu
        file_menu = menubar.addMenu("&File")
        
        open_action = file_menu.addAction("&Open Snapshot...")
        open_action.triggered.connect(self._on_open_snapshot)
        
        file_menu.addSeparator()
        
        exit_action = file_menu.addAction("E&xit")
        exit_action.triggered.connect(self.close)

        # Scan menu
        scan_menu = menubar.addMenu("&Scan")
        
        full_scan_action = scan_menu.addAction("&Full System Scan")
        full_scan_action.triggered.connect(lambda: self._on_start_scan(["all"]))
        
        registry_scan_action = scan_menu.addAction("&Registry Only")
        registry_scan_action.triggered.connect(lambda: self._on_start_scan(["registry"]))

        # Remediation menu
        remediation_menu = menubar.addMenu("&Remediation")
        
        plan_action = remediation_menu.addAction("&Plan...")
        plan_action.triggered.connect(self._on_plan_remediation)
        
        apply_action = remediation_menu.addAction("&Apply")
        apply_action.triggered.connect(self._on_apply_remediation)

        # Help menu
        help_menu = menubar.addMenu("&Help")
        about_action = help_menu.addAction("&About")
        about_action.triggered.connect(self._on_about)

    def _setup_toolbar(self):
        """Setup toolbar."""
        toolbar = self.addToolBar("Main")
        toolbar.setObjectName("MainToolbar")

        # Source filter
        toolbar.addWidget(QLabel("Filter:"))
        self.source_combo = QComboBox()
        self.source_combo.addItems(["All", "Registry", "Services", "Tasks", "Logon"])
        toolbar.addWidget(self.source_combo)

        # Name filter
        toolbar.addWidget(QLabel(" Name:"))
        self.name_filter = QLineEdit()
        self.name_filter.setPlaceholderText("Type to filter...")
        self.name_filter.setMaximumWidth(150)
        toolbar.addWidget(self.name_filter)

        toolbar.addSeparator()

        # Action buttons
        scan_btn = QPushButton("🔍 Scan")
        scan_btn.clicked.connect(lambda: self._on_start_scan(["all"]))
        toolbar.addWidget(scan_btn)

        details_btn = QPushButton("📋 Details")
        details_btn.clicked.connect(self._on_show_details)
        toolbar.addWidget(details_btn)

        toolbar.addSeparator()

        remove_btn = QPushButton("❌ Remove")
        remove_btn.clicked.connect(self._on_plan_remediation)
        toolbar.addWidget(remove_btn)

        toolbar.addStretch()

    def _setup_central_widget(self):
        """Setup central widget with three panels."""
        central = QWidget()
        self.setCentralWidget(central)

        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(5, 5, 5, 5)

        # Left panel: entries list
        left_layout = QVBoxLayout()
        left_layout.addWidget(QLabel("Entries"))
        
        self.entries_table = QTableWidget()
        self.entries_table.setColumnCount(5)
        self.entries_table.setHorizontalHeaderLabels(
            ["Name", "Score", "Risk", "Source", "Signed"]
        )
        self.entries_table.setColumnWidth(0, 250)
        self.entries_table.setColumnWidth(1, 60)
        self.entries_table.setColumnWidth(2, 100)
        self.entries_table.setColumnWidth(3, 150)
        self.entries_table.setColumnWidth(4, 60)
        self.entries_table.itemSelectionChanged.connect(self._on_entry_selected)
        
        left_layout.addWidget(self.entries_table)

        # Right panel: details and actions
        right_layout = QVBoxLayout()

        # Top: entry details
        right_layout.addWidget(QLabel("Entry Details"))
        
        self.details_text = QTextEdit()
        self.details_text.setReadOnly(True)
        self.details_text.setMaximumHeight(400)
        right_layout.addWidget(self.details_text)

        # Middle: action buttons
        right_layout.addWidget(QLabel("Actions"))
        
        actions_layout = QHBoxLayout()
        
        evidence_btn = QPushButton("🔍 Get Evidence")
        evidence_btn.clicked.connect(self._on_get_evidence)
        actions_layout.addWidget(evidence_btn)
        
        remove_selected_btn = QPushButton("❌ Plan Removal")
        remove_selected_btn.clicked.connect(self._on_plan_remediation)
        actions_layout.addWidget(remove_selected_btn)
        
        undo_btn = QPushButton("↶ Undo")
        undo_btn.clicked.connect(self._on_undo_remediation)
        actions_layout.addWidget(undo_btn)
        
        right_layout.addLayout(actions_layout)

        # Bottom: progress
        right_layout.addWidget(QLabel("Progress"))
        
        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        right_layout.addWidget(self.progress_bar)

        right_layout.addStretch()

        # Combine into main layout with splitter
        left_widget = QWidget()
        left_widget.setLayout(left_layout)
        
        right_widget = QWidget()
        right_widget.setLayout(right_layout)

        splitter = QSplitter(Qt.Horizontal)
        splitter.addWidget(left_widget)
        splitter.addWidget(right_widget)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 2)

        main_layout.addWidget(splitter)

    def _setup_status_bar(self):
        """Setup status bar."""
        self.statusBar().showMessage("Ready")

    def _connect_signals(self):
        """Connect signals."""
        self.name_filter.textChanged.connect(self._on_filter_entries)

    def _on_start_scan(self, sources: List[str]):
        """Start system scan in background."""
        self.statusBar().showMessage("Scanning...")
        self.progress_bar.setVisible(True)
        self.progress_bar.setValue(0)

        self.scan_worker = ScanWorker(self.scanner, sources)
        self.scan_worker.progress.connect(self.statusBar().showMessage)
        self.scan_worker.finished.connect(self._on_scan_complete)
        self.scan_worker.error.connect(self._on_scan_error)
        self.scan_worker.start()

    def _on_scan_complete(self, result: ScanResult):
        """Handle scan completion."""
        self.current_scan_result = result
        self.statusBar().showMessage(f"Scan complete: {len(result.entries)} entries")
        self.progress_bar.setVisible(False)

        # Score entries
        self.scored_entries = self.scorer.score(result.entries)

        # Populate table
        self._populate_entries_table()

        # Save snapshot
        self.snapshot_manager.save(result, label="ui-scan")

    def _on_scan_error(self, error_msg: str):
        """Handle scan error."""
        self.statusBar().showMessage(error_msg)
        self.progress_bar.setVisible(False)
        QMessageBox.critical(self, "Scan Error", error_msg)

    def _populate_entries_table(self):
        """Populate entries table with scored entries."""
        self.entries_table.setRowCount(len(self.scored_entries))

        for row, scored_entry in enumerate(self.scored_entries):
            entry = scored_entry.entry

            # Name
            name_item = QTableWidgetItem(entry.name)
            self.entries_table.setItem(row, 0, name_item)

            # Score
            score_item = QTableWidgetItem(str(scored_entry.score))
            score_item.setTextAlignment(Qt.AlignCenter)
            self.entries_table.setItem(row, 1, score_item)

            # Risk level with color
            risk_item = QTableWidgetItem(scored_entry.risk_level.value)
            risk_item.setTextAlignment(Qt.AlignCenter)
            
            # Color by risk level
            if scored_entry.risk_level == RiskLevel.MALICIOUS:
                risk_item.setForeground(QColor("red"))
            elif scored_entry.risk_level == RiskLevel.SUSPICIOUS:
                risk_item.setForeground(QColor("orange"))
            else:
                risk_item.setForeground(QColor("green"))
            
            self.entries_table.setItem(row, 2, risk_item)

            # Source
            source_item = QTableWidgetItem(entry.source)
            self.entries_table.setItem(row, 3, source_item)

            # Signed (placeholder)
            signed_item = QTableWidgetItem("?")
            self.entries_table.setItem(row, 4, signed_item)

    def _on_entry_selected(self):
        """Handle entry selection."""
        selected_rows = self.entries_table.selectionModel().selectedRows()
        if selected_rows:
            row = selected_rows[0].row()
            if row < len(self.scored_entries):
                self.selected_entry_id = self.scored_entries[row].entry.entry_id
                self._update_details_panel()

    def _update_details_panel(self):
        """Update details panel for selected entry."""
        if not self.selected_entry_id:
            return

        # Find scored entry
        scored_entry = next(
            (e for e in self.scored_entries if e.entry.entry_id == self.selected_entry_id),
            None
        )
        if not scored_entry:
            return

        entry = scored_entry.entry
        details = f"""
Entry: {entry.name}
ID: {entry.entry_id}
Source: {entry.source}
Command: {entry.command}
Status: {entry.status}

Score: {scored_entry.score}/100
Risk: {scored_entry.risk_level.value.upper()}
Explanation: {scored_entry.explanation}

Signals ({len(scored_entry.signals)}):
  {', '.join(scored_entry.signals) if scored_entry.signals else '(none)'}

Rule Matches ({len(scored_entry.rule_matches)}):
  {', '.join(scored_entry.rule_matches) if scored_entry.rule_matches else '(none)'}

Metadata:
{json.dumps(entry.metadata, indent=2)}
        """.strip()

        self.details_text.setText(details)

    def _on_filter_entries(self, text: str):
        """Filter entries by name."""
        for row in range(self.entries_table.rowCount()):
            item = self.entries_table.item(row, 0)
            if item:
                matches = text.lower() in item.text().lower()
                self.entries_table.setRowHidden(row, not matches)

    def _on_show_details(self):
        """Show details for selected entry."""
        self._update_details_panel()

    def _on_get_evidence(self):
        """Get detailed evidence for selected entry."""
        if not self.selected_entry_id:
            QMessageBox.warning(self, "No Selection", "Please select an entry first.")
            return

        QMessageBox.information(self, "Evidence", "Evidence gathering not yet implemented.")

    def _on_plan_remediation(self):
        """Plan remediation for selected entry."""
        if not self.selected_entry_id:
            QMessageBox.warning(self, "No Selection", "Please select an entry first.")
            return

        QMessageBox.information(
            self, "Plan", 
            f"Plan would be created for entry: {self.selected_entry_id}"
        )

    def _on_apply_remediation(self):
        """Apply remediation plan."""
        QMessageBox.information(self, "Apply", "Apply remediation not yet implemented.")

    def _on_undo_remediation(self):
        """Undo remediation."""
        QMessageBox.information(self, "Undo", "Undo not yet implemented.")

    def _on_open_snapshot(self):
        """Open existing snapshot."""
        QMessageBox.information(self, "Open", "Open snapshot not yet implemented.")

    def _on_about(self):
        """Show about dialog."""
        QMessageBox.about(
            self,
            "About Uboot",
            "Uboot v1.0\\n\\n"
            "Autoruns-style analyzer with AI scoring.\\n"
            "Built with Python, PySide6, and C++17."
        )


def main():
    """Main entry point for GUI."""
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
