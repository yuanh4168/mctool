#pragma once
#include <windows.h>
#include <string>

class ReminderWindow {
public:
    ReminderWindow();
    ~ReminderWindow();
    void Show(const std::wstring& message, int displaySeconds = 5);
private:
    HWND m_hWnd;
    std::wstring m_message;
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void AnimateAndClose();
};