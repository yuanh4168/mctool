#include "MainWindow.h"
#include "GameLauncher.h"
#include "NewsFetcher.h"
#include <thread>

MainWindow::MainWindow() : m_hWnd(NULL), m_popupVisible(false) {}
MainWindow::~MainWindow() {}

bool MainWindow::Create(HINSTANCE hInst) {
    m_hInst = hInst;

    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    std::string configPath = exePath.substr(0, pos + 1) + "config.json";
    if (!m_config.Load(configPath)) {
        MessageBoxA(NULL, "Failed to load config.json", "Error", MB_ICONERROR);
        return false;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MainHiddenClass";
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MainHiddenClass";
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(0, L"MainHiddenClass", L"MCTool", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInst, this);
    if (!m_hWnd) return false;

    if (!m_popup.Create(m_hWnd, hInst, m_config)) return false;

    SetTimer(m_hWnd, 1, 200, NULL);
    StartServerPingThread();
    return true;
}

void MainWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void MainWindow::CheckMouseEdge() {
    POINT pt;
    GetCursorPos(&pt);
    if (pt.y < m_config.edgeThreshold && !m_popupVisible) {
        m_popup.Show();
        m_popupVisible = true;
        StartNewsFetch();
    } else if (pt.y > m_config.popupHeight + 20 && m_popupVisible) {
        m_popup.Hide();
        m_popupVisible = false;
    }
}

void MainWindow::StartServerPingThread() {
    std::thread([this]() {
        while (true) {
            ServerStatus status;
            if (PingServer(m_config.serverHost, m_config.serverPort, status)) {
                PostMessage(m_hWnd, WM_UPDATE_SERVER_STATUS, 0, (LPARAM)new ServerStatus(status));
            } else {
                ServerStatus offline;
                PostMessage(m_hWnd, WM_UPDATE_SERVER_STATUS, 0, (LPARAM)new ServerStatus(offline));
            }
            Sleep(3000);
        }
    }).detach();
}

void MainWindow::StartNewsFetch() {
    std::thread([this]() {
        std::string news = FetchNews(m_config.newsURL);
        PostMessage(m_hWnd, WM_UPDATE_NEWS, 0, (LPARAM)new std::string(news));
    }).detach();
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (MainWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (MainWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        switch (msg) {
            case WM_TIMER:
                pThis->CheckMouseEdge();
                break;
            case WM_COMMAND:
                if (LOWORD(wParam) == IDC_LAUNCH_BUTTON) {
                    LaunchGame(pThis->m_config);
                }
                break;
            case WM_UPDATE_SERVER_STATUS:
            {
                ServerStatus* pStatus = (ServerStatus*)lParam;
                pThis->m_popup.UpdateServerStatus(*pStatus);
                delete pStatus;
                break;
            }
            case WM_UPDATE_NEWS:
            {
                std::string* pNews = (std::string*)lParam;
                pThis->m_popup.UpdateNews(*pNews);
                delete pNews;
                break;
            }
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}