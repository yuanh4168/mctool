// SettingsWindow.cpp
#include "SettingsWindow.h"
#include "DPIHelper.h"
#include "PopupWindow.h"
#include "ServerPinger.h"
#include "ReminderWindow.h"
#include "resource.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define WM_CONFIG_UPDATED (WM_USER + 300)
#define WM_CLEAR_HISTORY  (WM_USER + 500)

#ifndef Button_SetCheck
#define Button_SetCheck(hwnd, check) SendMessage(hwnd, BM_SETCHECK, (check) ? BST_CHECKED : BST_UNCHECKED, 0)
#endif
#ifndef Button_GetCheck
#define Button_GetCheck(hwnd) (int)SendMessage(hwnd, BM_GETCHECK, 0, 0)
#endif

// 自定义输入对话框数据
struct InputDialogData {
    std::string title;
    std::string label1;
    std::string label2;
    std::string value1;
    std::string value2;
    bool confirmed;
    HWND hDlg;
    HFONT hFont;
};

// 输入对话框过程
static LRESULT CALLBACK InputDialogWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    InputDialogData* pData = nullptr;
    if (msg == WM_NCCREATE) {
        pData = (InputDialogData*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pData);
        pData->hDlg = hWnd;
        return TRUE;
    }
    pData = (InputDialogData*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pData) return DefWindowProc(hWnd, msg, wParam, lParam);

    switch (msg) {
        case WM_CREATE: {
            double scale = GetDPIScale();
            int xMargin = (int)(15 * scale);
            int yMargin = (int)(15 * scale);
            int labelW = (int)(80 * scale);
            int editW = (int)(200 * scale);
            int btnW = (int)(80 * scale);
            int rowH = (int)(28 * scale);
            int y = yMargin;

            HDC hdc = GetDC(hWnd);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hWnd, hdc);
            int fontSize = -MulDiv(11, dpiX, 96);
            LOGFONTW lf = {0};
            lf.lfHeight = fontSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
            pData->hFont = CreateFontIndirectW(&lf);

            // 标签1
            CreateWindowW(L"STATIC", PopupWindow::UTF8ToWide(pData->label1).c_str(),
                WS_CHILD | WS_VISIBLE, xMargin, y, labelW, rowH, hWnd, NULL, GetModuleHandle(NULL), NULL);
            // 编辑框1
            HWND hEdit1 = CreateWindowW(L"EDIT", PopupWindow::UTF8ToWide(pData->value1).c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                xMargin + labelW + xMargin, y, editW, rowH, hWnd, (HMENU)101, GetModuleHandle(NULL), NULL);
            y += rowH + yMargin;

            // 标签2
            CreateWindowW(L"STATIC", PopupWindow::UTF8ToWide(pData->label2).c_str(),
                WS_CHILD | WS_VISIBLE, xMargin, y, labelW, rowH, hWnd, NULL, GetModuleHandle(NULL), NULL);
            // 编辑框2
            HWND hEdit2 = CreateWindowW(L"EDIT", PopupWindow::UTF8ToWide(pData->value2).c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                xMargin + labelW + xMargin, y, editW, rowH, hWnd, (HMENU)102, GetModuleHandle(NULL), NULL);
            y += rowH + yMargin * 2;

            // 确定按钮
            CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                xMargin + labelW + xMargin + editW - btnW * 2 - xMargin, y, btnW, rowH,
                hWnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);
            // 取消按钮
            CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                xMargin + labelW + xMargin + editW - btnW, y, btnW, rowH,
                hWnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            // 设置字体
            for (HWND hChild = GetWindow(hWnd, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
                SendMessage(hChild, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
            }

            // 设置焦点
            SetFocus(hEdit1);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                wchar_t buf1[256] = {0}, buf2[256] = {0};
                GetDlgItemTextW(hWnd, 101, buf1, 256);
                GetDlgItemTextW(hWnd, 102, buf2, 256);
                pData->value1 = PopupWindow::WideToUTF8(buf1);
                pData->value2 = PopupWindow::WideToUTF8(buf2);
                pData->confirmed = true;
                DestroyWindow(hWnd);
            } else if (LOWORD(wParam) == IDCANCEL) {
                pData->confirmed = false;
                DestroyWindow(hWnd);
            }
            return 0;
        case WM_CLOSE:
            pData->confirmed = false;
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            if (pData->hFont) DeleteObject(pData->hFont);
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static bool ShowInputDialog(HWND hParent, const std::string& title,
                            const std::string& label1, const std::string& label2,
                            std::string& value1, std::string& value2)
{
    HINSTANCE hInst = GetModuleHandle(NULL);
    double scale = GetDPIScale();
    int width = (int)(350 * scale);
    int height = (int)(150 * scale);

    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = InputDialogWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"InputDialogClass";
    RegisterClassExW(&wc);

    InputDialogData data;
    data.title = title;
    data.label1 = label1;
    data.label2 = label2;
    data.value1 = value1;
    data.value2 = value2;
    data.confirmed = false;
    data.hFont = NULL;

    HWND hDlg = CreateWindowExW(0, L"InputDialogClass", PopupWindow::UTF8ToWide(title).c_str(),
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
        width, height,
        hParent, NULL, hInst, &data);
    if (!hDlg) return false;

    // 消息循环（模态）
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (data.confirmed) {
        value1 = data.value1;
        value2 = data.value2;
        return true;
    }
    return false;
}

SettingsWindow::SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent)
    : m_config(cfg), m_hDlg(NULL), m_hInst(hInst), m_hParent(hParent), m_selectedTab(0),
      m_hClearFont(NULL)
{
    m_hServerList = m_hAddServerBtn = m_hEditServerBtn = m_hDelServerBtn = NULL;
    m_hMoveUpBtn = m_hMoveDownBtn = m_hSetDefaultBtn = m_hTestServerBtn = NULL;
    m_hShortcutList = m_hAddShortcutBtn = m_hEditShortcutBtn = m_hDelShortcutBtn = NULL;
    m_hPopupWidth = m_hPopupHeight = m_hEdgeThreshold = NULL;
    m_hTimeEnabled = m_hTimeFormat = NULL;
    m_hReminderEnabled = m_hReminderInterval = m_hReminderMessage = m_hTestReminderBtn = NULL;
    m_hMonitorEnabled = m_hMonitorInterval = m_hMonitorMaxPoints = m_hClearHistoryBtn = NULL;
    m_hGameCommand = m_hBrowseGameBtn = NULL;
    m_hStartupEnabled = m_hResetConfigBtn = m_hBackupConfigBtn = m_hRestoreConfigBtn = m_hAboutText = NULL;
    m_hTabCtrl = NULL;
}

SettingsWindow::~SettingsWindow()
{
    if (m_hClearFont) DeleteObject(m_hClearFont);
    if (m_hDlg && IsWindow(m_hDlg))
        DestroyWindow(m_hDlg);
}

void SettingsWindow::Show()
{
    if (m_hDlg && IsWindow(m_hDlg)) {
        SetForegroundWindow(m_hDlg);
        return;
    }

    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    m_configPath = exePath.substr(0, pos + 1) + "config.json";

    m_config.Load(m_configPath);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = m_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SettingsDialogClass";
    RegisterClassExW(&wc);

    double scale = GetDPIScale();
    int width = (int)(750 * scale);
    int height = (int)(850 * scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    std::wstring title = PopupWindow::UTF8ToWide("设置");
    m_hDlg = CreateWindowExW(0, L"SettingsDialogClass", title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL,
        x, y, width, height,
        m_hParent, NULL, m_hInst, this);
    if (!m_hDlg) {
        MessageBoxW(m_hParent, L"创建设置窗口失败", L"错误", MB_ICONERROR);
        return;
    }

    SCROLLINFO si = { sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE };
    si.nMin = 0;
    si.nMax = (int)(1200 * scale);
    si.nPage = height;
    SetScrollInfo(m_hDlg, SB_VERT, &si, TRUE);

    InitControls();
    LoadDataToUI();

    ShowWindow(m_hDlg, SW_SHOW);
    UpdateWindow(m_hDlg);
}

void SettingsWindow::InitControls()
{
    double scale = GetDPIScale();
    int margin = (int)(15 * scale);
    int groupMargin = (int)(20 * scale);
    int groupHeight = (int)(25 * scale);
    int btnWidth = (int)(100 * scale);
    int btnHeight = (int)(28 * scale);
    int labelW = (int)(120 * scale);
    int editW = (int)(150 * scale);
    int lineH = (int)(32 * scale);
    int x = margin;
    int y = margin;

    if (!m_hClearFont) {
        HDC hdc = GetDC(m_hDlg);
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(m_hDlg, hdc);
        int fontSize = -MulDiv(11, dpiX, 96);
        LOGFONTW lf = {};
        lf.lfHeight = fontSize;
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
        m_hClearFont = CreateFontIndirectW(&lf);
    }

    // 服务器管理区域
    CreateWindowW(L"BUTTON", L"服务器管理", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, (int)(700 * scale), (int)(350 * scale), m_hDlg, NULL, m_hInst, NULL);
    y += groupHeight + margin;

    m_hServerList = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        x + margin, y, (int)(400 * scale), (int)(270 * scale),
        m_hDlg, (HMENU)IDC_SERVER_LIST, m_hInst, NULL);
    int btnX = x + margin + (int)(400 * scale) + margin;
    int btnY = y;
    m_hAddServerBtn = CreateWindowW(L"BUTTON", L"添加", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_ADD_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hEditServerBtn = CreateWindowW(L"BUTTON", L"编辑", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_EDIT_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hDelServerBtn = CreateWindowW(L"BUTTON", L"删除", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_DEL_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hMoveUpBtn = CreateWindowW(L"BUTTON", L"上移", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_MOVE_UP_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hMoveDownBtn = CreateWindowW(L"BUTTON", L"下移", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_MOVE_DOWN_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hSetDefaultBtn = CreateWindowW(L"BUTTON", L"设为默认", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_SET_DEFAULT_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hTestServerBtn = CreateWindowW(L"BUTTON", L"测试连接", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_TEST_SERVER, m_hInst, NULL);
    y += (int)(280 * scale) + groupMargin;

    // 快捷方式管理区域
    CreateWindowW(L"BUTTON", L"快捷方式管理", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, (int)(700 * scale), (int)(200 * scale), m_hDlg, NULL, m_hInst, NULL);
    y += groupHeight + margin;

    m_hShortcutList = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        x + margin, y, (int)(400 * scale), (int)(120 * scale),
        m_hDlg, (HMENU)IDC_SHORTCUT_LIST, m_hInst, NULL);
    btnX = x + margin + (int)(400 * scale) + margin;
    btnY = y;
    m_hAddShortcutBtn = CreateWindowW(L"BUTTON", L"添加", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_ADD_SHORTCUT, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hEditShortcutBtn = CreateWindowW(L"BUTTON", L"编辑", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_EDIT_SHORTCUT, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hDelShortcutBtn = CreateWindowW(L"BUTTON", L"删除", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_DEL_SHORTCUT, m_hInst, NULL);
    y += (int)(130 * scale) + groupMargin;

    // 界面设置
    CreateWindowW(L"BUTTON", L"界面设置", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, (int)(700 * scale), (int)(160 * scale), m_hDlg, NULL, m_hInst, NULL);
    y += groupHeight + margin;

    CreateWindowW(L"STATIC", L"弹窗宽度:", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hPopupWidth = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + margin + labelW + margin, y, editW, lineH, m_hDlg, (HMENU)IDC_POPUP_WIDTH, m_hInst, NULL);
    y += lineH + margin;
    CreateWindowW(L"STATIC", L"弹窗高度:", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hPopupHeight = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + margin + labelW + margin, y, editW, lineH, m_hDlg, (HMENU)IDC_POPUP_HEIGHT, m_hInst, NULL);
    y += lineH + margin;
    CreateWindowW(L"STATIC", L"边缘阈值(px):", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hEdgeThreshold = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + margin + labelW + margin, y, editW, lineH, m_hDlg, (HMENU)IDC_EDGE_THRESHOLD, m_hInst, NULL);
    y += lineH + groupMargin;

    // 时间显示
    CreateWindowW(L"BUTTON", L"时间显示", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, (int)(700 * scale), (int)(130 * scale), m_hDlg, NULL, m_hInst, NULL);
    y += groupHeight + margin;

    m_hTimeEnabled = CreateWindowW(L"BUTTON", L"启用时间显示", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x + margin, y, (int)(150 * scale), lineH, m_hDlg, (HMENU)IDC_TIME_ENABLED, m_hInst, NULL);
    y += lineH + margin;
    CreateWindowW(L"STATIC", L"时间格式:", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hTimeFormat = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        x + margin + labelW + margin, y, editW, lineH * 5, m_hDlg, (HMENU)IDC_TIME_FORMAT, m_hInst, NULL);
    SendMessageW(m_hTimeFormat, CB_ADDSTRING, 0, (LPARAM)L"HH:mm:ss");
    SendMessageW(m_hTimeFormat, CB_ADDSTRING, 0, (LPARAM)L"HH:mm");
    y += lineH + groupMargin;

    // 休息提醒
    CreateWindowW(L"BUTTON", L"休息提醒", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, (int)(700 * scale), (int)(200 * scale), m_hDlg, NULL, m_hInst, NULL);
    y += groupHeight + margin;

    m_hReminderEnabled = CreateWindowW(L"BUTTON", L"启用休息提醒", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x + margin, y, (int)(150 * scale), lineH, m_hDlg, (HMENU)IDC_REMINDER_ENABLED, m_hInst, NULL);
    y += lineH + margin;
    CreateWindowW(L"STATIC", L"间隔(分钟):", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hReminderInterval = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + margin + labelW + margin, y, editW, lineH, m_hDlg, (HMENU)IDC_REMINDER_INTERVAL, m_hInst, NULL);
    y += lineH + margin;
    CreateWindowW(L"STATIC", L"提醒消息:", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hReminderMessage = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        x + margin + labelW + margin, y, (int)(300 * scale), lineH, m_hDlg, (HMENU)IDC_REMINDER_MESSAGE, m_hInst, NULL);
    y += lineH + margin;
    m_hTestReminderBtn = CreateWindowW(L"BUTTON", L"测试提醒", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + margin, y, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_TEST_REMINDER, m_hInst, NULL);
    y += btnHeight + groupMargin;

    // 后台监控
    CreateWindowW(L"BUTTON", L"后台监控", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, (int)(700 * scale), (int)(200 * scale), m_hDlg, NULL, m_hInst, NULL);
    y += groupHeight + margin;

    m_hMonitorEnabled = CreateWindowW(L"BUTTON", L"启用后台监控", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x + margin, y, (int)(150 * scale), lineH, m_hDlg, (HMENU)IDC_MONITOR_ENABLED, m_hInst, NULL);
    y += lineH + margin;
    CreateWindowW(L"STATIC", L"间隔(秒):", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hMonitorInterval = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + margin + labelW + margin, y, editW, lineH, m_hDlg, (HMENU)IDC_MONITOR_INTERVAL, m_hInst, NULL);
    y += lineH + margin;
    CreateWindowW(L"STATIC", L"最大数据点:", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hMonitorMaxPoints = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + margin + labelW + margin, y, editW, lineH, m_hDlg, (HMENU)IDC_MONITOR_MAXPOINTS, m_hInst, NULL);
    y += lineH + margin;
    m_hClearHistoryBtn = CreateWindowW(L"BUTTON", L"清除历史数据", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + margin, y, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_CLEAR_HISTORY, m_hInst, NULL);
    y += btnHeight + groupMargin;

    // 游戏启动
    CreateWindowW(L"BUTTON", L"游戏启动", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, (int)(700 * scale), (int)(130 * scale), m_hDlg, NULL, m_hInst, NULL);
    y += groupHeight + margin;

    CreateWindowW(L"STATIC", L"启动命令:", WS_CHILD | WS_VISIBLE,
        x + margin, y, labelW, lineH, m_hDlg, NULL, m_hInst, NULL);
    m_hGameCommand = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        x + margin + labelW + margin, y, (int)(300 * scale), lineH, m_hDlg, (HMENU)IDC_GAME_COMMAND, m_hInst, NULL);
    m_hBrowseGameBtn = CreateWindowW(L"BUTTON", L"浏览...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + margin + labelW + margin + (int)(300 * scale) + margin, y, btnWidth, btnHeight,
        m_hDlg, (HMENU)IDC_BROWSE_GAME, m_hInst, NULL);
    y += lineH + groupMargin;

    // 其他设置
    CreateWindowW(L"BUTTON", L"其他", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, (int)(700 * scale), (int)(200 * scale), m_hDlg, NULL, m_hInst, NULL);
    y += groupHeight + margin;

    m_hStartupEnabled = CreateWindowW(L"BUTTON", L"开机启动", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x + margin, y, (int)(150 * scale), lineH, m_hDlg, (HMENU)IDC_STARTUP_ENABLED, m_hInst, NULL);
    y += lineH + margin;
    m_hResetConfigBtn = CreateWindowW(L"BUTTON", L"重置配置", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + margin, y, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_RESET_CONFIG, m_hInst, NULL);
    y += btnHeight + margin;
    m_hBackupConfigBtn = CreateWindowW(L"BUTTON", L"备份配置", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + margin, y, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_BACKUP_CONFIG, m_hInst, NULL);
    y += btnHeight + margin;
    m_hRestoreConfigBtn = CreateWindowW(L"BUTTON", L"恢复配置", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + margin, y, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_RESTORE_CONFIG, m_hInst, NULL);
    y += btnHeight + margin;
    m_hAboutText = CreateWindowW(L"STATIC", L"YMC-toolkit v1.0\n一个 Minecraft 服务器状态监控工具",
        WS_CHILD | WS_VISIBLE,
        x + margin, y, (int)(400 * scale), (int)(60 * scale), m_hDlg, (HMENU)IDC_ABOUT_TEXT, m_hInst, NULL);
    y += (int)(70 * scale);

    EnumChildWindows(m_hDlg, [](HWND hWnd, LPARAM lParam) -> BOOL {
        SendMessage(hWnd, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
    }, (LPARAM)m_hClearFont);

    int totalHeight = y + margin;
    SCROLLINFO si = { sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE };
    si.nMin = 0;
    si.nMax = totalHeight;
    RECT rc;
    GetClientRect(m_hDlg, &rc);
    si.nPage = rc.bottom - rc.top;
    SetScrollInfo(m_hDlg, SB_VERT, &si, TRUE);
}

void SettingsWindow::LoadDataToUI()
{
    UpdateServerListUI();
    UpdateShortcutListUI();

    wchar_t buf[32];
    swprintf(buf, 32, L"%d", m_config.popupWidth);
    SetWindowTextW(m_hPopupWidth, buf);
    swprintf(buf, 32, L"%d", m_config.popupHeight);
    SetWindowTextW(m_hPopupHeight, buf);
    swprintf(buf, 32, L"%d", m_config.edgeThreshold);
    SetWindowTextW(m_hEdgeThreshold, buf);

    Button_SetCheck(m_hTimeEnabled, m_config.timeDisplay.enabled ? BST_CHECKED : BST_UNCHECKED);
    int formatIdx = (m_config.timeDisplay.format == "HH:mm:ss") ? 0 : 1;
    SendMessageW(m_hTimeFormat, CB_SETCURSEL, formatIdx, 0);

    Button_SetCheck(m_hReminderEnabled, m_config.reminder.enabled ? BST_CHECKED : BST_UNCHECKED);
    swprintf(buf, 32, L"%d", m_config.reminder.intervalMinutes);
    SetWindowTextW(m_hReminderInterval, buf);
    SetWindowTextW(m_hReminderMessage, PopupWindow::UTF8ToWide(m_config.reminder.message).c_str());

    Button_SetCheck(m_hMonitorEnabled, m_config.serverMonitor.backgroundEnabled ? BST_CHECKED : BST_UNCHECKED);
    swprintf(buf, 32, L"%d", m_config.serverMonitor.intervalSeconds);
    SetWindowTextW(m_hMonitorInterval, buf);
    swprintf(buf, 32, L"%d", m_config.serverMonitor.maxDataPoints);
    SetWindowTextW(m_hMonitorMaxPoints, buf);

    SetWindowTextW(m_hGameCommand, PopupWindow::UTF8ToWide(m_config.gameCommand).c_str());

    HKEY hKey;
    bool startup = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        startup = (RegQueryValueExW(hKey, L"YMC-toolkit", NULL, NULL, (BYTE*)path, &size) == ERROR_SUCCESS);
        RegCloseKey(hKey);
    }
    Button_SetCheck(m_hStartupEnabled, startup ? BST_CHECKED : BST_UNCHECKED);
}

void SettingsWindow::SaveUItoConfig()
{
    BOOL translated;
    int val = GetDlgItemInt(m_hDlg, IDC_POPUP_WIDTH, &translated, FALSE);
    if (translated && val >= 200) m_config.popupWidth = val;
    val = GetDlgItemInt(m_hDlg, IDC_POPUP_HEIGHT, &translated, FALSE);
    if (translated && val >= 200) m_config.popupHeight = val;
    val = GetDlgItemInt(m_hDlg, IDC_EDGE_THRESHOLD, &translated, FALSE);
    if (translated) m_config.edgeThreshold = val;

    m_config.timeDisplay.enabled = (Button_GetCheck(m_hTimeEnabled) == BST_CHECKED);
    int formatIdx = (int)SendMessageW(m_hTimeFormat, CB_GETCURSEL, 0, 0);
    m_config.timeDisplay.format = (formatIdx == 0) ? "HH:mm:ss" : "HH:mm";

    m_config.reminder.enabled = (Button_GetCheck(m_hReminderEnabled) == BST_CHECKED);
    val = GetDlgItemInt(m_hDlg, IDC_REMINDER_INTERVAL, &translated, FALSE);
    if (translated && val > 0) m_config.reminder.intervalMinutes = val;
    wchar_t msgBuf[256];
    GetWindowTextW(m_hReminderMessage, msgBuf, 256);
    m_config.reminder.message = PopupWindow::WideToUTF8(msgBuf);

    m_config.serverMonitor.backgroundEnabled = (Button_GetCheck(m_hMonitorEnabled) == BST_CHECKED);
    val = GetDlgItemInt(m_hDlg, IDC_MONITOR_INTERVAL, &translated, FALSE);
    if (translated && val > 0) m_config.serverMonitor.intervalSeconds = val;
    val = GetDlgItemInt(m_hDlg, IDC_MONITOR_MAXPOINTS, &translated, FALSE);
    if (translated && val > 0) m_config.serverMonitor.maxDataPoints = val;

    wchar_t cmdBuf[MAX_PATH];
    GetWindowTextW(m_hGameCommand, cmdBuf, MAX_PATH);
    m_config.gameCommand = PopupWindow::WideToUTF8(cmdBuf);

    m_config.Save(m_configPath);
}

void SettingsWindow::UpdateServerListUI()
{
    SendMessageW(m_hServerList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < m_config.servers.size(); ++i) {
        std::string entry = m_config.servers[i].host + ":" + std::to_string(m_config.servers[i].port);
        std::wstring wentry = PopupWindow::UTF8ToWide(entry);
        SendMessageW(m_hServerList, LB_ADDSTRING, 0, (LPARAM)wentry.c_str());
    }
    if (m_config.currentServer >= 0 && m_config.currentServer < (int)m_config.servers.size()) {
        SendMessageW(m_hServerList, LB_SETCURSEL, m_config.currentServer, 0);
    }
}

void SettingsWindow::UpdateShortcutListUI()
{
    SendMessageW(m_hShortcutList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < m_config.shortcuts.size(); ++i) {
        std::string entry = m_config.shortcuts[i].name + " -> " + m_config.shortcuts[i].url;
        std::wstring wentry = PopupWindow::UTF8ToWide(entry);
        SendMessageW(m_hShortcutList, LB_ADDSTRING, 0, (LPARAM)wentry.c_str());
    }
}

void SettingsWindow::AddServer()
{
    std::string host = "localhost";
    std::string port = "25565";
    if (ShowInputDialog(m_hDlg, "添加服务器", "主机名/IP", "端口", host, port)) {
        if (!host.empty()) {
            ServerInfo sv;
            sv.host = host;
            sv.port = std::stoi(port);
            m_config.servers.push_back(sv);
            UpdateServerListUI();
        }
    }
}

void SettingsWindow::EditServer()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    ServerInfo& sv = m_config.servers[sel];
    std::string host = sv.host;
    std::string port = std::to_string(sv.port);
    if (ShowInputDialog(m_hDlg, "编辑服务器", "主机名/IP", "端口", host, port)) {
        sv.host = host;
        sv.port = std::stoi(port);
        UpdateServerListUI();
    }
}

void SettingsWindow::DeleteServer()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        m_config.servers.erase(m_config.servers.begin() + sel);
        if (m_config.currentServer >= (int)m_config.servers.size())
            m_config.currentServer = (int)m_config.servers.size() - 1;
        if (m_config.currentServer < 0) m_config.currentServer = 0;
        UpdateServerListUI();
    }
}

void SettingsWindow::MoveServerUp()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel > 0) {
        std::swap(m_config.servers[sel], m_config.servers[sel-1]);
        if (m_config.currentServer == sel) m_config.currentServer = sel-1;
        else if (m_config.currentServer == sel-1) m_config.currentServer = sel;
        UpdateServerListUI();
        SendMessageW(m_hServerList, LB_SETCURSEL, sel-1, 0);
    }
}

void SettingsWindow::MoveServerDown()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < (int)m_config.servers.size() - 1) {
        std::swap(m_config.servers[sel], m_config.servers[sel+1]);
        if (m_config.currentServer == sel) m_config.currentServer = sel+1;
        else if (m_config.currentServer == sel+1) m_config.currentServer = sel;
        UpdateServerListUI();
        SendMessageW(m_hServerList, LB_SETCURSEL, sel+1, 0);
    }
}

void SettingsWindow::SetDefaultServer()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        m_config.currentServer = sel;
        UpdateServerListUI();
    }
}

void SettingsWindow::TestServer()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    ServerInfo& sv = m_config.servers[sel];
    ServerStatus status;
    if (PingServer(sv.host, sv.port, status)) {
        wchar_t msg[1024];
        swprintf(msg, 1024, L"在线\n玩家: %d/%d\n版本: %s\n延迟: %d ms",
            status.players, status.maxPlayers,
            PopupWindow::UTF8ToWide(status.version).c_str(),
            status.latency);
        MessageBoxW(m_hDlg, msg, L"测试结果", MB_OK);
    } else {
        MessageBoxW(m_hDlg, L"离线或无法连接", L"测试结果", MB_OK);
    }
}

void SettingsWindow::AddShortcut()
{
    std::string name = "新快捷方式";
    std::string url = "https://";
    if (ShowInputDialog(m_hDlg, "添加快捷方式", "名称", "URL", name, url)) {
        if (!name.empty() && !url.empty()) {
            Shortcut sc;
            sc.name = name;
            sc.url = url;
            m_config.shortcuts.push_back(sc);
            UpdateShortcutListUI();
        }
    }
}

void SettingsWindow::EditShortcut()
{
    int sel = (int)SendMessageW(m_hShortcutList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    Shortcut& sc = m_config.shortcuts[sel];
    std::string name = sc.name;
    std::string url = sc.url;
    if (ShowInputDialog(m_hDlg, "编辑快捷方式", "名称", "URL", name, url)) {
        sc.name = name;
        sc.url = url;
        UpdateShortcutListUI();
    }
}

void SettingsWindow::DeleteShortcut()
{
    int sel = (int)SendMessageW(m_hShortcutList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        m_config.shortcuts.erase(m_config.shortcuts.begin() + sel);
        UpdateShortcutListUI();
    }
}

void SettingsWindow::BrowseGameCommand()
{
    OPENFILENAMEW ofn = {};
    wchar_t fileName[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hDlg;
    ofn.lpstrFilter = L"可执行文件\0*.exe\0所有文件\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) {
        SetWindowTextW(m_hGameCommand, fileName);
    }
}

void SettingsWindow::TestReminder()
{
    ReminderWindow reminder;
    reminder.Show(PopupWindow::UTF8ToWide(m_config.reminder.message));
}

void SettingsWindow::ClearHistory()
{
    PostMessage(m_hParent, WM_CLEAR_HISTORY, 0, 0);
    MessageBoxW(m_hDlg, L"历史数据已清除", L"提示", MB_OK);
}

void SettingsWindow::ResetConfig()
{
    if (MessageBoxW(m_hDlg, L"重置所有配置到默认值？当前配置将丢失。", L"确认", MB_YESNO) == IDYES) {
        m_config = Config::FromJson(nlohmann::json::object());
        m_config.Save(m_configPath);
        LoadDataToUI();
        PostMessage(m_hParent, WM_CONFIG_UPDATED, 0, 0);
    }
}

void SettingsWindow::BackupConfig()
{
    wchar_t backupPath[MAX_PATH];
    GetModuleFileNameW(NULL, backupPath, MAX_PATH);
    wcscat_s(backupPath, L".backup.json");
    if (CopyFileW(PopupWindow::UTF8ToWide(m_configPath).c_str(), backupPath, FALSE)) {
        MessageBoxW(m_hDlg, L"备份成功", L"提示", MB_OK);
    } else {
        MessageBoxW(m_hDlg, L"备份失败", L"错误", MB_ICONERROR);
    }
}

void SettingsWindow::RestoreConfig()
{
    wchar_t backupPath[MAX_PATH];
    GetModuleFileNameW(NULL, backupPath, MAX_PATH);
    wcscat_s(backupPath, L".backup.json");
    if (CopyFileW(backupPath, PopupWindow::UTF8ToWide(m_configPath).c_str(), FALSE)) {
        m_config.Load(m_configPath);
        LoadDataToUI();
        PostMessage(m_hParent, WM_CONFIG_UPDATED, 0, 0);
        MessageBoxW(m_hDlg, L"恢复成功", L"提示", MB_OK);
    } else {
        MessageBoxW(m_hDlg, L"未找到备份文件", L"错误", MB_ICONERROR);
    }
}

void SettingsWindow::SetStartup(bool enable)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        if (enable) {
            RegSetValueExW(hKey, L"YMC-toolkit", 0, REG_SZ, (BYTE*)exePath, (wcslen(exePath)+1)*sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, L"YMC-toolkit");
        }
        RegCloseKey(hKey);
    }
}

LRESULT CALLBACK SettingsWindow::DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SettingsWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    pThis = reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (!pThis) return DefWindowProc(hWnd, msg, wParam, lParam);

    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_ADD_SERVER: pThis->AddServer(); break;
                case IDC_EDIT_SERVER: pThis->EditServer(); break;
                case IDC_DEL_SERVER: pThis->DeleteServer(); break;
                case IDC_MOVE_UP_SERVER: pThis->MoveServerUp(); break;
                case IDC_MOVE_DOWN_SERVER: pThis->MoveServerDown(); break;
                case IDC_SET_DEFAULT_SERVER: pThis->SetDefaultServer(); break;
                case IDC_TEST_SERVER: pThis->TestServer(); break;
                case IDC_ADD_SHORTCUT: pThis->AddShortcut(); break;
                case IDC_EDIT_SHORTCUT: pThis->EditShortcut(); break;
                case IDC_DEL_SHORTCUT: pThis->DeleteShortcut(); break;
                case IDC_BROWSE_GAME: pThis->BrowseGameCommand(); break;
                case IDC_TEST_REMINDER: pThis->TestReminder(); break;
                case IDC_CLEAR_HISTORY: pThis->ClearHistory(); break;
                case IDC_RESET_CONFIG: pThis->ResetConfig(); break;
                case IDC_BACKUP_CONFIG: pThis->BackupConfig(); break;
                case IDC_RESTORE_CONFIG: pThis->RestoreConfig(); break;
                case IDC_STARTUP_ENABLED:
                    pThis->SetStartup(Button_GetCheck(pThis->m_hStartupEnabled) == BST_CHECKED);
                    break;
                case IDCANCEL:
                    DestroyWindow(hWnd);
                    break;
            }
            return 0;
        case WM_VSCROLL: {
            int pos = GetScrollPos(hWnd, SB_VERT);
            int newPos = pos;
            switch (LOWORD(wParam)) {
                case SB_LINEUP:      newPos -= 20; break;
                case SB_LINEDOWN:    newPos += 20; break;
                case SB_PAGEUP:      newPos -= (int)(GetSystemMetrics(SM_CYSCREEN) * 0.8); break;
                case SB_PAGEDOWN:    newPos += (int)(GetSystemMetrics(SM_CYSCREEN) * 0.8); break;
                case SB_THUMBTRACK:  newPos = HIWORD(wParam); break;
            }
            SCROLLINFO si = { sizeof(SCROLLINFO), SIF_POS };
            GetScrollInfo(hWnd, SB_VERT, &si);
            if (newPos < si.nMin) newPos = si.nMin;
            if (newPos > si.nMax - (int)si.nPage) newPos = si.nMax - si.nPage;
            if (newPos != pos) {
                SetScrollPos(hWnd, SB_VERT, newPos, TRUE);
                ScrollWindow(hWnd, 0, pos - newPos, NULL, NULL);
                UpdateWindow(hWnd);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int pos = GetScrollPos(hWnd, SB_VERT);
            int newPos = pos - delta / 2;
            SCROLLINFO si = { sizeof(SCROLLINFO), SIF_POS | SIF_RANGE | SIF_PAGE };
            GetScrollInfo(hWnd, SB_VERT, &si);
            if (newPos < si.nMin) newPos = si.nMin;
            if (newPos > si.nMax - (int)si.nPage) newPos = si.nMax - si.nPage;
            if (newPos != pos) {
                SetScrollPos(hWnd, SB_VERT, newPos, TRUE);
                ScrollWindow(hWnd, 0, pos - newPos, NULL, NULL);
                UpdateWindow(hWnd);
            }
            return 0;
        }
        case WM_CLOSE:
            pThis->SaveUItoConfig();
            PostMessage(pThis->m_hParent, WM_CONFIG_UPDATED, 0, 0);
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            pThis->m_hDlg = NULL;
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}