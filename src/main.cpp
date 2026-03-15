#include <windows.h>
#include "MainWindow.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow; // 避免警告
    MainWindow mainWnd;
    if (!mainWnd.Create(hInstance)) {
        return 1;
    }
    mainWnd.RunMessageLoop();
    return 0;
}