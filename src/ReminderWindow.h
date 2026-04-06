#pragma once
#include <windows.h>
#include <string>

#define WM_USER_SHOW_BALLOON (WM_USER + 100)

class ReminderWindow {
public:
    ReminderWindow();
    ~ReminderWindow();
    void Show(const std::wstring& message, int displaySeconds = 5);
};