#include "ReminderWindow.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

static ULONG_PTR gdiplusToken = 0;

ReminderWindow::ReminderWindow() : m_hWnd(NULL) {
    if (gdiplusToken == 0) {
        Gdiplus::GdiplusStartupInput input;
        GdiplusStartup(&gdiplusToken, &input, NULL);
    }
}

ReminderWindow::~ReminderWindow() {
    if (m_hWnd) DestroyWindow(m_hWnd);
}

void ReminderWindow::Show(const std::wstring& message, int displaySeconds) {
    m_message = message;
    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = L"ReminderWindowClass";
    RegisterClassExW(&wc);

    int width = 300;
    int height = 100;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = screenWidth - width - 20;
    int y = screenHeight - height - 20;

    m_hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"ReminderWindowClass", NULL,
        WS_POPUP,
        x, y, width, height,
        NULL, NULL, hInst, this);
    if (!m_hWnd) return;

    SetTimer(m_hWnd, 1, displaySeconds * 1000, NULL);
    ShowWindow(m_hWnd, SW_SHOW);
    AnimateWindow(m_hWnd, 200, AW_SLIDE | AW_VER_POSITIVE | AW_ACTIVATE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void ReminderWindow::AnimateAndClose() {
    if (m_hWnd && IsWindow(m_hWnd)) {
        AnimateWindow(m_hWnd, 200, AW_SLIDE | AW_VER_NEGATIVE | AW_HIDE);
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
    }
}

LRESULT CALLBACK ReminderWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ReminderWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (ReminderWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ReminderWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            HBRUSH bgBrush = CreateSolidBrush(RGB(32, 32, 32));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(pen);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
            if (pThis) {
                DrawTextW(hdc, pThis->m_message.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            SelectObject(hdc, oldFont);
            DeleteObject(hFont);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (pThis) pThis->AnimateAndClose();
            return 0;
        case WM_CLOSE:
            if (pThis) pThis->AnimateAndClose();
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}