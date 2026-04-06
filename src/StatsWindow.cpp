#include "StatsWindow.h"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#pragma comment(lib, "gdiplus.lib")

static ULONG_PTR statsGdiplusToken = 0;

StatsWindow::StatsWindow(HINSTANCE hInst, const std::vector<MainWindow::LatencyRecord>& history)
    : m_hInst(hInst), m_history(history), m_hWnd(NULL) {
    if (statsGdiplusToken == 0) {
        Gdiplus::GdiplusStartupInput input;
        GdiplusStartup(&statsGdiplusToken, &input, NULL);
    }
}

void StatsWindow::Show() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"StatsWindowClass";
    RegisterClassExW(&wc);

    int width = 800;
    int height = 500;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    m_hWnd = CreateWindowExW(0, L"StatsWindowClass", L"服务器延迟统计图",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, width, height,
        NULL, NULL, m_hInst, this);
    if (!m_hWnd) return;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void StatsWindow::OnPaint(HDC hdc) {
    using namespace Gdiplus;
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    RECT client;
    GetClientRect(m_hWnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    SolidBrush bgBrush(Color(32, 32, 32));
    graphics.FillRectangle(&bgBrush, 0, 0, width, height);

    if (m_history.size() < 2) {
        Font font(L"Microsoft YaHei", 16);
        SolidBrush textBrush(Color(255, 255, 255));
        graphics.DrawString(L"数据不足，无法绘制折线图", -1, &font, PointF(50, 50), &textBrush);
        return;
    }

    int marginLeft = 60;
    int marginRight = 30;
    int marginTop = 30;
    int marginBottom = 50;
    int plotWidth = width - marginLeft - marginRight;
    int plotHeight = height - marginTop - marginBottom;

    int maxLatency = 0;
    for (auto& rec : m_history) {
        int lat = rec.latency > 0 ? rec.latency : 0;
        if (lat > maxLatency) maxLatency = lat;
    }
    if (maxLatency == 0) maxLatency = 100;

    Pen gridPen(Color(80, 80, 80), 1.0f);
    Pen axisPen(Color(200, 200, 200), 1.0f);
    Font font(L"Microsoft YaHei", 10);
    SolidBrush textBrush(Color(200, 200, 200));

    // Y轴网格线
    for (int i = 0; i <= 5; ++i) {
        int yVal = maxLatency * i / 5;
        int y = marginTop + (int)(plotHeight * (1.0f - (float)yVal / maxLatency));
        graphics.DrawLine(&gridPen, marginLeft, y, marginLeft + plotWidth, y);
        wchar_t label[16];
        swprintf(label, 16, L"%d", yVal);
        graphics.DrawString(label, -1, &font, PointF(marginLeft - 40, y - 8), &textBrush);
    }

    // X轴网格线
    int n = (int)m_history.size();
    for (int i = 0; i < n; ++i) {
        int x = marginLeft + (int)(plotWidth * (float)i / (n - 1));
        graphics.DrawLine(&gridPen, x, marginTop, x, marginTop + plotHeight);
        wchar_t label[32];
        time_t t = m_history[i].timestamp;
        struct tm tm;
        localtime_s(&tm, &t);
        swprintf(label, 32, L"%02d:%02d", tm.tm_hour, tm.tm_min);
        graphics.DrawString(label, -1, &font, PointF(x - 20, marginTop + plotHeight + 5), &textBrush);
    }

    // 折线
    Pen linePen(Color(255, 200, 100), 2.0f);
    std::vector<Point> points;
    for (int i = 0; i < n; ++i) {
        int lat = m_history[i].latency > 0 ? m_history[i].latency : 0;
        int x = marginLeft + (int)(plotWidth * (float)i / (n - 1));
        int y = marginTop + (int)(plotHeight * (1.0f - (float)lat / maxLatency));
        points.push_back(Point(x, y));
    }
    graphics.DrawLines(&linePen, points.data(), (INT)points.size());

    // 数据点
    SolidBrush pointBrush(Color(255, 100, 200, 255));
    for (auto& pt : points) {
        graphics.FillEllipse(&pointBrush, pt.X - 3, pt.Y - 3, 6, 6);
    }
}

LRESULT CALLBACK StatsWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    StatsWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (StatsWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (StatsWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (pThis) pThis->OnPaint(hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}