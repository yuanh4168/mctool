#include "PopupWindow.h"
#include <shellapi.h>   // 用于 ShellExecuteA
#include <cstdio>       // 用于 swprintf

// 静态辅助函数：UTF-8 转宽字符串
std::wstring PopupWindow::UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], len);
    return wstr;
}

PopupWindow::PopupWindow() 
    : m_hWnd(NULL), m_hServerStatic(NULL), m_hBkBrush(NULL), m_hFont(NULL) {
    for (int i = 0; i < 4; ++i) m_hShortcutButtons[i] = NULL;
}

PopupWindow::~PopupWindow() {
    if (m_hWnd) DestroyWindow(m_hWnd);
    if (m_hBkBrush) DeleteObject(m_hBkBrush);
    if (m_hFont) DeleteObject(m_hFont);
}

bool PopupWindow::Create(HWND hParent, HINSTANCE hInst, const Config& cfg) {
    m_config = cfg;

    // 注册窗口类
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;          // 背景擦除由我们自己处理
    wc.lpszClassName = L"PopupClass";
    RegisterClassExW(&wc);

    // 创建窗口（带扩展样式）
    m_hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"PopupClass", NULL,
        WS_POPUP | WS_BORDER,
        0, 0, cfg.popupWidth, cfg.popupHeight,
        hParent, NULL, hInst, this);

    if (!m_hWnd) return false;

    // 创建深色背景画刷和字体
    m_hBkBrush = CreateSolidBrush(RGB(32, 32, 32));
    m_hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // 创建服务器状态标签
    CreateWindowW(L"STATIC", L"服务器状态", WS_CHILD | WS_VISIBLE,
        10, 10, 380, 20, m_hWnd, NULL, hInst, NULL);
    m_hServerStatic = CreateWindowW(L"STATIC", L"未知", WS_CHILD | WS_VISIBLE,
        10, 30, 380, 60, m_hWnd, (HMENU)IDC_SERVER_STATUS, hInst, NULL);
    SendMessageW(m_hServerStatic, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // 创建四个快捷按钮（自绘样式）
    int btnWidth = (cfg.popupWidth - 50) / 4;
    for (int i = 0; i < 4 && i < (int)cfg.shortcuts.size(); ++i) {
        m_hShortcutButtons[i] = CreateWindowW(
            L"BUTTON",
            UTF8ToWide(cfg.shortcuts[i].name).c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            10 + i * (btnWidth + 10), 100, btnWidth, 30,
            m_hWnd, (HMENU)(IDC_SHORTCUT1 + i), hInst, NULL);
        SendMessageW(m_hShortcutButtons[i], WM_SETFONT, (WPARAM)m_hFont, TRUE);
    }

    // 创建启动游戏按钮
    CreateWindowW(L"BUTTON", L"启动游戏", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 150, 100, 30, m_hWnd, (HMENU)IDC_LAUNCH_BUTTON, hInst, NULL);

    return true;
}

void PopupWindow::Show() {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    SetWindowPos(m_hWnd, HWND_TOPMOST,
        (screenWidth - m_config.popupWidth) / 2, 0,
        m_config.popupWidth, m_config.popupHeight,
        SWP_SHOWWINDOW);
}

void PopupWindow::Hide() {
    ShowWindow(m_hWnd, SW_HIDE);
}

void PopupWindow::UpdateServerStatus(const ServerStatus& status) {
    if (!m_hServerStatic) return;
    std::wstring text;
    if (status.online) {
        wchar_t buf[1024];
        swprintf(buf, 1024, L"在线 - %s\n%d/%d 玩家\n版本: %s\n延迟: %d ms",
            UTF8ToWide(status.motd).c_str(),
            status.players, status.maxPlayers,
            UTF8ToWide(status.version).c_str(),
            status.latency);
        text = buf;
    } else {
        text = L"离线";
    }
    SetWindowTextW(m_hServerStatic, text.c_str());
}

LRESULT CALLBACK PopupWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PopupWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (PopupWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (PopupWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (!pThis) return DefWindowProcW(hWnd, msg, wParam, lParam);

    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, pThis->m_hBkBrush);
            return TRUE;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(240, 240, 240));
            SetBkColor(hdcStatic, RGB(32, 32, 32));
            return (LRESULT)pThis->m_hBkBrush;
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
            if (lpDIS->CtlType == ODT_BUTTON) {
                wchar_t text[256];
                GetWindowTextW(lpDIS->hwndItem, text, 256);

                HDC hdc = lpDIS->hDC;
                SetBkMode(hdc, TRANSPARENT);

                // 根据按钮状态选择背景色
                COLORREF bgColor;
                if (lpDIS->itemState & ODS_SELECTED)
                    bgColor = RGB(60, 60, 60);
                else if (lpDIS->itemState & ODS_DISABLED)
                    bgColor = RGB(20, 20, 20);
                else
                    bgColor = RGB(45, 45, 45);

                HBRUSH bgBrush = CreateSolidBrush(bgColor);
                FillRect(hdc, &lpDIS->rcItem, bgBrush);
                DeleteObject(bgBrush);

                // 绘制边框（浅灰色）
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
                SelectObject(hdc, pen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, lpDIS->rcItem.left, lpDIS->rcItem.top,
                    lpDIS->rcItem.right, lpDIS->rcItem.bottom);
                DeleteObject(pen);

                // 绘制文本
                SetTextColor(hdc, RGB(220, 220, 220));
                DrawTextW(hdc, text, -1, &lpDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id >= IDC_SHORTCUT1 && id <= IDC_SHORTCUT4) {
                int index = id - IDC_SHORTCUT1;
                if (index < (int)pThis->m_config.shortcuts.size()) {
                    const std::string& url = pThis->m_config.shortcuts[index].url;
                    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                }
                break;
            } else if (id == IDC_LAUNCH_BUTTON) {
                // 发送启动命令给父窗口
                SendMessage(GetParent(hWnd), WM_COMMAND, IDC_LAUNCH_BUTTON, 0);
            }
            break;
        }
        case WM_DESTROY:
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}