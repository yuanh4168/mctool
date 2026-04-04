#include <windows.h>
#include "MainWindow.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    // 启用 DPI 感知（避免系统缩放导致模糊）
    SetProcessDPIAware();   // Windows Vista 及以上
    // 或者更现代的方式（需要 Windows 10 1607+）：
    // SetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    MainWindow mainWnd;
    if (!mainWnd.Create(hInstance)) {
        return 1;
    }
    mainWnd.RunMessageLoop();
    return 0;
}