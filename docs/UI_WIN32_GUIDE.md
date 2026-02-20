# Uboot Win32 UI Guide

This document describes how the Uboot Desktop UI works. The interface is built purely using the Win32 API.

## Architecture

The layout logic and component initialization is separated into dedicated files to avoid memory bloat and spaghetti code in `MainWindow.cpp`.

- **`MainWindow`**: Core UI class that registers the `WNDCLASSEXW`, instantiates child controls via OnCreate, resizes them via OnSize, and handles events via OnNotify.
- **`Layout`**: Contains recalculation rules. Re-arranges controls using `SetWindowPos` whenever `WM_SIZE` triggers.
- **`Tabs`**: Initializer for `SysTabControl32` (`Logon` / `Services` / `Scheduled Tasks` / `Drivers` / `WMI` / `All`).
- **`EntriesListView`**: Initializer for `SysListView32` containing the dataset. Implements double-buffering using `LVS_EX_DOUBLEBUFFER` to avoid UI flicker.
- **`DetailPane`**: A `STATIC` text control that updates based on list view selection.

## Modifying the UI

### 1. Adding a new Tab
Open `app/win32/Tabs.cpp` and modify `tabNames[]`. Adjust the loop counter bounds.

### 2. Adding Colums
Open `app/win32/EntriesListView.cpp` and adjust the `columns[]` and `widths[]` arrays inside `InitializeColumns`. Change the loop upper limit for insertions.

### 3. Handling Events
Inside `app/win32/MainWindow.cpp`, check `MainWindow::OnNotify(HWND hWnd, LPARAM lParam)`. Look for logic catching `LPNMHDR` structs matching control events (`TCN_SELCHANGE` for tabs, `LVN_ITEMCHANGED` for ListView).
