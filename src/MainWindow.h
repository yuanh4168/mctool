#pragma once
#include <windows.h>
#include "Config.h"
#include "PopupWindow.h"
#include "ServerPinger.h"

#define WM_UPDATE_SERVER_STATUS (WM_USER + 1)
// 不再使用新闻更新消息
// #define WM_UPDATE_NEWS          (WM_USER + 2)

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInst);
    void RunMessageLoop();
    HWND GetHWND() const { return m_hWnd; }
    const Config& GetConfig() const { return m_config; }

private:
    HWND m_hWnd;
    HINSTANCE m_hInst;
    Config m_config;
    PopupWindow m_popup;
    bool m_popupVisible;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CheckMouseEdge();
    void StartServerPingThread();
    // void StartNewsFetch();  // 删除
};