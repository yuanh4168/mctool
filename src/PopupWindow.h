#pragma once
#include <windows.h>
#include "Config.h"
#include "ServerPinger.h"

#define IDC_SERVER_STATUS 1001
#define IDC_NEWS_CONTENT  1002
#define IDC_LAUNCH_BUTTON 1003

class PopupWindow {
public:
    PopupWindow();
    ~PopupWindow();

    bool Create(HWND hParent, HINSTANCE hInst, const Config& cfg);
    void Show();
    void Hide();
    void UpdateServerStatus(const ServerStatus& status);
    void UpdateNews(const std::string& news);
    HWND GetHWND() const { return m_hWnd; }

private:
    HWND m_hWnd;
    HWND m_hServerStatic;
    HWND m_hNewsEdit;
    Config m_config;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};