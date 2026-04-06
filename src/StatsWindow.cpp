#include "StatsWindow.h"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static ULONG_PTR statsGdiplusToken = 0;

StatsWindow::StatsWindow(HINSTANCE hInst, const std::vector<MainWindow::LatencyRecord>& history)
    : m_hInst(hInst), m_history(history), m_hWnd(NULL) {
    if (statsGdiplusToken == 0) {
        GdiplusStartupInput input;
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
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"StatsWindowClass";
    RegisterClassExW(&wc);

    int width = 900;   // 加宽窗口，留出更多边距
    int height = 600;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    // 使用系统标准标题栏（带关闭按钮、可调整大小）
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
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    RECT client;
    GetClientRect(m_hWnd, &client);
    REAL width = (REAL)(client.right - client.left);
    REAL height = (REAL)(client.bottom - client.top);

    // 背景色（深色，与主程序风格一致）
    SolidBrush bgBrush(Color(32, 32, 32));
    graphics.FillRectangle(&bgBrush, 0.0f, 0.0f, width, height);

    // 图表区域边距（增加左边距以容纳较长的数字，如 "120"）
    REAL marginLeft = 80.0f;
    REAL marginRight = 40.0f;
    REAL marginTop = 40.0f;
    REAL marginBottom = 50.0f;
    REAL plotWidth = width - marginLeft - marginRight;
    REAL plotHeight = height - marginTop - marginBottom;

    if (m_history.size() < 2) {
        Font msgFont(L"Microsoft YaHei", 14.0f);
        SolidBrush msgBrush(Color(200, 200, 200));
        graphics.DrawString(L"数据不足，无法绘制折线图", -1, &msgFont,
            PointF(width / 2.0f - 100.0f, height / 2.0f), &msgBrush);
        return;
    }

    // 计算最大延迟（忽略 -1）
    int maxLatency = 0;
    for (auto& rec : m_history) {
        int lat = rec.latency > 0 ? rec.latency : 0;
        if (lat > maxLatency) maxLatency = lat;
    }
    if (maxLatency == 0) maxLatency = 100;

    // 字体和画刷
    Font font(L"Microsoft YaHei", 10.0f);
    SolidBrush textBrush(Color(220, 220, 220));
    Pen gridPen(Color(80, 80, 80), 1.0f);

    // 垂直居中 + 右对齐格式（用于 Y 轴标签）
    StringFormat yAlign;
    yAlign.SetAlignment(StringAlignmentFar);   // 右对齐
    yAlign.SetLineAlignment(StringAlignmentCenter); // 垂直居中

    // 绘制 Y 轴网格线和标签
    for (int i = 0; i <= 5; ++i) {
        int yVal = maxLatency * i / 5;
        REAL y = marginTop + plotHeight * (1.0f - (REAL)yVal / maxLatency);
        graphics.DrawLine(&gridPen, marginLeft, y, marginLeft + plotWidth, y);

        wchar_t label[16];
        swprintf(label, 16, L"%d", yVal);
        // 标签矩形：宽度 60，高度 30，确保三位数（如 120）完整显示
        RectF labelRect(marginLeft - 65.0f, y - 15.0f, 60.0f, 30.0f);
        graphics.DrawString(label, -1, &font, labelRect, &yAlign, &textBrush);
    }

    // X 轴标签（居中 + 垂直居中）
    int n = (int)m_history.size();
    StringFormat xAlign;
    xAlign.SetAlignment(StringAlignmentCenter);
    xAlign.SetLineAlignment(StringAlignmentCenter);

    for (int i = 0; i < n; ++i) {
        REAL x = marginLeft + plotWidth * (REAL)i / (n - 1);
        graphics.DrawLine(&gridPen, x, marginTop, x, marginTop + plotHeight);

        wchar_t label[32];
        time_t t = m_history[i].timestamp;
        struct tm tm;
        localtime_s(&tm, &t);
        swprintf(label, 32, L"%02d:%02d", tm.tm_hour, tm.tm_min);

        // 标签矩形宽度 80，高度 25，避免文字被裁剪
        RectF labelRect(x - 40.0f, marginTop + plotHeight + 5.0f, 80.0f, 25.0f);
        graphics.DrawString(label, -1, &font, labelRect, &xAlign, &textBrush);
    }

    // 绘制折线
    Pen linePen(Color(255, 200, 100), 2.0f);
    std::vector<PointF> points;
    for (int i = 0; i < n; ++i) {
        int lat = m_history[i].latency > 0 ? m_history[i].latency : 0;
        REAL x = marginLeft + plotWidth * (REAL)i / (n - 1);
        REAL y = marginTop + plotHeight * (1.0f - (REAL)lat / maxLatency);
        points.push_back(PointF(x, y));
    }
    graphics.DrawLines(&linePen, points.data(), (INT)points.size());

    // 绘制数据点
    SolidBrush pointBrush(Color(255, 100, 200, 255));
    for (auto& pt : points) {
        graphics.FillEllipse(&pointBrush, pt.X - 4.0f, pt.Y - 4.0f, 8.0f, 8.0f);
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
        case WM_ERASEBKGND:
            return 1;   // 避免闪烁，由 OnPaint 全权绘制
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}