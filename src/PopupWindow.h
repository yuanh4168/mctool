#pragma once
#include <windows.h>
#include <string>
#include "Config.h"
#include "ServerPinger.h"

#define IDC_SERVER_STATUS 1001
#define IDC_NEWS_CONTENT  1002  // 不再使用，保留定义避免冲突
#define IDC_LAUNCH_BUTTON 1003
#define IDC_SHORTCUT1     1004
#define IDC_SHORTCUT2     1005
#define IDC_SHORTCUT3     1006
#define IDC_SHORTCUT4     1007

class PopupWindow {
public:
    PopupWindow();
    ~PopupWindow();

    bool Create(HWND hParent, HINSTANCE hInst, const Config& cfg);
    void Show();
    void Hide();
    void UpdateServerStatus(const ServerStatus& status);
    HWND GetHWND() const { return m_hWnd; }

private:
    HWND m_hWnd;
    HWND m_hServerStatic;
    HWND m_hShortcutButtons[4];
    Config m_config;
    HBRUSH m_hBkBrush;
    HFONT m_hFont;

    // UTF-8 转宽字符串的静态辅助函数
    static std::wstring UTF8ToWide(const std::string& utf8);

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};