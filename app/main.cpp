#include "win32/MainWindow.h"
#include <commctrl.h>
#include <windows.h>


// Tell the linker to include common controls
#pragma comment(lib, "comctl32.lib")

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  // Initialize Common Controls
  INITCOMMONCONTROLSEX icex = {};
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
  InitCommonControlsEx(&icex);

  if (!win32::MainWindow::RegisterClass(hInstance)) {
    return 0;
  }

  HWND hWnd = win32::MainWindow::Create(hInstance, nCmdShow);
  if (!hWnd) {
    return 0;
  }

  MSG msg = {};
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return (int)msg.wParam;
}
