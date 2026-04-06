#pragma once
#include <windows.h>
#include <vector>
#include "MainWindow.h"

class StatsWindow {
public:
    StatsWindow(HINSTANCE hInst, const std::vector<MainWindow::LatencyRecord>& history);
    void Show();
private:
    HINSTANCE m_hInst;
    std::vector<MainWindow::LatencyRecord> m_history;
    HWND m_hWnd;
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HDC hdc);
};