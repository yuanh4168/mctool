#include "ReminderWindow.h"
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

static HWND g_hReminderWnd = NULL;
static NOTIFYICONDATAW g_nid = {};

static LRESULT CALLBACK HiddenWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            memset(&g_nid, 0, sizeof(g_nid));
            g_nid.cbSize = sizeof(NOTIFYICONDATAW);
            g_nid.hWnd = hWnd;
            g_nid.uID = 1;
            g_nid.uFlags = NIF_INFO;
            g_nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
            wcscpy_s(g_nid.szInfoTitle, L"MCTool 提醒");
            Shell_NotifyIconW(NIM_ADD, &g_nid);
            break;
        case WM_USER_SHOW_BALLOON: {
            const wchar_t* msgText = (const wchar_t*)lParam;
            wcscpy_s(g_nid.szInfo, msgText);
            Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            delete[] msgText;   // 释放内存
            break;
        }
        case WM_DESTROY:
            if (g_nid.hWnd) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
                memset(&g_nid, 0, sizeof(g_nid));
            }
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

ReminderWindow::ReminderWindow() {}
ReminderWindow::~ReminderWindow() {}

void ReminderWindow::Show(const std::wstring& message, int displaySeconds) {
    (void)displaySeconds;  // 消除未使用参数警告，如需停留时间可自行扩展

    if (!g_hReminderWnd) {
        HINSTANCE hInst = GetModuleHandle(NULL);
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = HiddenWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"ReminderHiddenClass";
        RegisterClassExW(&wc);
        g_hReminderWnd = CreateWindowExW(0, L"ReminderHiddenClass", NULL, WS_POPUP,
            0, 0, 0, 0, NULL, NULL, hInst, NULL);
    }

    if (g_hReminderWnd) {
        wchar_t* msgCopy = new wchar_t[message.length() + 1];
        wcscpy(msgCopy, message.c_str());
        PostMessage(g_hReminderWnd, WM_USER_SHOW_BALLOON, 0, (LPARAM)msgCopy);
    }
}