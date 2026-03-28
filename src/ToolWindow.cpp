#include "ToolWindow.h"
#include "PopupWindow.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

#define ID_PROMPT_EDIT    1001
#define ID_GENERATE_BTN   1002
#define ID_EXPORT_BTN     1003
#define ID_PROMPT_RESULT  1004
#define ID_STRUCTURE_EDIT 1005
#define ID_SELECT_PATH    1006
#define ID_GEN_STRUCT_BTN 1007
#define ID_LOG_EDIT       1008

const int DIALOG_WIDTH = 800;
const int DIALOG_HEIGHT = 720;

ToolWindow::ToolWindow() : m_hWnd(NULL),
    m_hPromptEdit(NULL), m_hGenerateBtn(NULL), m_hExportBtn(NULL), m_hPromptResultEdit(NULL),
    m_hStructureEdit(NULL), m_hSelectPathBtn(NULL), m_hGenerateStructureBtn(NULL), m_hLogEdit(NULL),
    m_hBkBrush(NULL), m_hNormalFont(NULL), m_hBoldFont(NULL), m_hHoverButton(NULL) {
}

ToolWindow::~ToolWindow() {
    if (m_hWnd) DestroyWindow(m_hWnd);
}

bool ToolWindow::Show(HWND hParent, HINSTANCE hInst) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ToolWindowClass";
    RegisterClassExW(&wc);

    // 恢复系统菜单（显示关闭按钮），但禁止调整大小和最大化
    m_hWnd = CreateWindowExW(0, L"ToolWindowClass", L"工具箱",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, DIALOG_WIDTH, DIALOG_HEIGHT,
        hParent, NULL, hInst, this);
    if (!m_hWnd) return false;

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    MSG msg;
    while (IsWindow(m_hWnd) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(m_hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return true;
}

LRESULT CALLBACK ToolWindow::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    ToolWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (ToolWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ToolWindow*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
    }
    if (!pThis) return DefWindowProcW(hDlg, msg, wParam, lParam);

    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE);
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            // 第一部分：DeepSeek 提示词生成器
            CreateWindowW(L"BUTTON", L"DeepSeek 提示词生成器", 
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                10, 10, DIALOG_WIDTH - 30, 320, hDlg, NULL, hInst, NULL);
            
            CreateWindowW(L"STATIC", L"项目描述：", WS_CHILD | WS_VISIBLE,
                20, 40, 150, 20, hDlg, NULL, hInst, NULL);
            pThis->m_hPromptEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                20, 65, DIALOG_WIDTH - 50, 100, hDlg, (HMENU)ID_PROMPT_EDIT, hInst, NULL);
            
            CreateWindowW(L"STATIC", L"生成的提示词：", WS_CHILD | WS_VISIBLE,
                20, 180, 150, 20, hDlg, NULL, hInst, NULL);
            pThis->m_hPromptResultEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                20, 205, DIALOG_WIDTH - 50, 90, hDlg, (HMENU)ID_PROMPT_RESULT, hInst, NULL);
            
            pThis->m_hGenerateBtn = CreateWindowW(L"BUTTON", L"生成提示词",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 305, 140, 25, hDlg, (HMENU)ID_GENERATE_BTN, hInst, NULL);
            pThis->m_hExportBtn = CreateWindowW(L"BUTTON", L"导出为 Markdown",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                170, 305, 150, 25, hDlg, (HMENU)ID_EXPORT_BTN, hInst, NULL);
            
            // 第二部分：项目结构生成器
            CreateWindowW(L"BUTTON", L"项目结构生成器",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                10, 340, DIALOG_WIDTH - 30, 350, hDlg, NULL, hInst, NULL);
            
            CreateWindowW(L"STATIC", L"粘贴目录树（支持 tree /f 风格）：", WS_CHILD | WS_VISIBLE,
                20, 370, 300, 20, hDlg, NULL, hInst, NULL);
            pThis->m_hStructureEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                20, 395, DIALOG_WIDTH - 50, 100, hDlg, (HMENU)ID_STRUCTURE_EDIT, hInst, NULL);
            
            pThis->m_hSelectPathBtn = CreateWindowW(L"BUTTON", L"选择生成目录",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 505, 140, 25, hDlg, (HMENU)ID_SELECT_PATH, hInst, NULL);
            
            CreateWindowW(L"STATIC", L"日志：", WS_CHILD | WS_VISIBLE,
                20, 540, 50, 20, hDlg, NULL, hInst, NULL);
            pThis->m_hLogEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                20, 565, DIALOG_WIDTH - 50, 70, hDlg, (HMENU)ID_LOG_EDIT, hInst, NULL);
            
            pThis->m_hGenerateStructureBtn = CreateWindowW(L"BUTTON", L"生成结构",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 645, 140, 25, hDlg, (HMENU)ID_GEN_STRUCT_BTN, hInst, NULL);
            
            // 设置字体
            for (HWND hChild = GetWindow(hDlg, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
                SendMessage(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
            }
            
            wchar_t modulePath[MAX_PATH];
            GetModuleFileNameW(NULL, modulePath, MAX_PATH);
            std::wstring exeDir = modulePath;
            exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));
            pThis->m_targetPath = exeDir;
            
            return 0;
        }
        case WM_SIZE:
            SetWindowPos(hDlg, NULL, 0, 0, DIALOG_WIDTH, DIALOG_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* pInfo = (MINMAXINFO*)lParam;
            pInfo->ptMinTrackSize.x = DIALOG_WIDTH;
            pInfo->ptMinTrackSize.y = DIALOG_HEIGHT;
            pInfo->ptMaxTrackSize.x = DIALOG_WIDTH;
            pInfo->ptMaxTrackSize.y = DIALOG_HEIGHT;
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_GENERATE_BTN: pThis->OnGeneratePrompt(); break;
                case ID_EXPORT_BTN:   pThis->OnExportPrompt(); break;
                case ID_SELECT_PATH:  pThis->OnSelectPath(); break;
                case ID_GEN_STRUCT_BTN: pThis->OnGenerateStructure(); break;
                case IDCANCEL:        DestroyWindow(hDlg); break;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hDlg);
            break;
        case WM_DESTROY:
            // 不调用 PostQuitMessage(0)，避免退出整个程序
            break;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

// ------------------ 辅助函数 ------------------
std::string ToolWindow::LoadPromptTemplate() {
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    std::string configDir = exePath.substr(0, pos + 1);
    std::string templatePath = configDir + "prompt_template.json";
    try {
        std::ifstream f(templatePath);
        if (!f.is_open()) return "";
        json j;
        f >> j;
        if (j.contains("template") && j["template"].is_string())
            return j["template"].get<std::string>();
    } catch (...) {}
    return "";
}

void ToolWindow::OnGeneratePrompt() {
    int len = GetWindowTextLengthW(m_hPromptEdit);
    std::wstring wdesc(len + 1, L'\0');
    GetWindowTextW(m_hPromptEdit, &wdesc[0], len + 1);
    wdesc.resize(len);
    std::string description = PopupWindow::WideToUTF8(wdesc);
    if (description.empty()) {
        MessageBoxW(m_hWnd, L"请输入项目描述。", L"提示", MB_OK);
        return;
    }
    std::string templateStr = LoadPromptTemplate();
    std::string prompt;
    if (!templateStr.empty()) {
        size_t pos = templateStr.find("{{description}}");
        if (pos != std::string::npos) {
            prompt = templateStr;
            prompt.replace(pos, 15, description);
        } else {
            prompt = templateStr + "\n\n" + description;
        }
    } else {
        prompt = "# DeepSeek 提示词模板\n\n## 项目背景\n" + description + "\n\n## 任务目标\n"
                + "根据上述项目背景，完成以下任务：\n\n1. 分析项目需求，给出技术选型建议\n"
                + "2. 设计核心功能模块及接口\n3. 提供关键代码示例\n4. 指出可能的难点和解决方案\n\n"
                + "## 输出格式\n请以 Markdown 格式输出，包含清晰的标题和代码块。\n";
    }
    std::wstring wprompt = PopupWindow::UTF8ToWide(prompt);
    SetWindowTextW(m_hPromptResultEdit, wprompt.c_str());
}

void ToolWindow::OnExportPrompt() {
    int len = GetWindowTextLengthW(m_hPromptResultEdit);
    if (len == 0) {
        MessageBoxW(m_hWnd, L"没有可导出的内容，请先生成提示词。", L"提示", MB_OK);
        return;
    }
    std::wstring wprompt(len + 1, L'\0');
    GetWindowTextW(m_hPromptResultEdit, &wprompt[0], len + 1);
    wprompt.resize(len);
    std::string prompt = PopupWindow::WideToUTF8(wprompt);
    OPENFILENAMEW ofn = {0};
    wchar_t fileName[MAX_PATH] = L"prompt_template.md";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"Markdown 文件\0*.md\0所有文件\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"md";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        std::ofstream file(fileName, std::ios::binary);
        if (file) {
            file << prompt;
            file.close();
            MessageBoxW(m_hWnd, L"导出成功。", L"提示", MB_OK);
        } else {
            MessageBoxW(m_hWnd, L"保存文件失败。", L"错误", MB_ICONERROR);
        }
    }
}

void ToolWindow::OnSelectPath() {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = L"选择生成目录";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            m_targetPath = path;
            AppendLog(L"目标目录已设置为: " + m_targetPath);
        }
        CoTaskMemFree(pidl);
    }
}

void ToolWindow::OnGenerateStructure() {
    int len = GetWindowTextLengthW(m_hStructureEdit);
    std::wstring wtree(len + 1, L'\0');
    GetWindowTextW(m_hStructureEdit, &wtree[0], len + 1);
    wtree.resize(len);
    std::string treeText = PopupWindow::WideToUTF8(wtree);
    if (treeText.empty()) {
        MessageBoxW(m_hWnd, L"请粘贴目录树。", L"提示", MB_OK);
        return;
    }
    std::string rootPath = PopupWindow::WideToUTF8(m_targetPath);
    std::string log;
    bool success = ParseAndGenerate(treeText, rootPath, log);
    AppendLog(PopupWindow::UTF8ToWide(log));
    if (success)
        MessageBoxW(m_hWnd, L"项目结构生成成功。请查看日志窗口。", L"完成", MB_OK);
    else
        MessageBoxW(m_hWnd, L"生成过程中出现错误，请查看日志窗口。", L"错误", MB_ICONERROR);
}

void ToolWindow::AppendLog(const std::wstring& text) {
    int len = GetWindowTextLengthW(m_hLogEdit);
    std::wstring current(len + 1, L'\0');
    GetWindowTextW(m_hLogEdit, &current[0], len + 1);
    current.resize(len);
    if (!current.empty()) current += L"\r\n";
    current += text;
    SetWindowTextW(m_hLogEdit, current.c_str());
    SendMessageW(m_hLogEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(m_hLogEdit, EM_SCROLLCARET, 0, 0);
}

void ToolWindow::AppendLog(const std::string& text) {
    AppendLog(PopupWindow::UTF8ToWide(text));
}

// ------------------ 核心解析函数（修复版）------------------
bool ToolWindow::ParseAndGenerate(const std::string& treeText, const std::string& rootPath, std::string& log) {
    log.clear();
    std::istringstream iss(treeText);
    std::string line;

    struct Entry {
        int level;
        std::string name;
        bool isDirectory;
        std::string path;
    };
    std::vector<Entry> entries;

    const std::string treeChars = "├─└ ";   // 需要移除的树状符号（不包括 │）
    const std::string pipeChar = "│";       // 竖线，将被替换为空格

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // 1. 将所有的 '│' 替换为空格，以统一缩进
        for (size_t pos = line.find(pipeChar); pos != std::string::npos; pos = line.find(pipeChar, pos)) {
            line[pos] = ' ';
        }

        // 2. 移除其他树状符号
        line.erase(std::remove_if(line.begin(), line.end(),
                     [&](char c) { return treeChars.find(c) != std::string::npos; }),
                   line.end());

        // 3. 计算缩进层级（每4个空格一级）
        int level = 0;
        size_t pos = 0;
        while (pos < line.size() && line[pos] == ' ') {
            ++pos;
            if (pos % 4 == 0) ++level;
        }
        // 移除前导空格
        line = line.substr(pos);

        // 4. 去除首尾多余空格
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        if (line.empty()) continue;

        std::string name = line;

        // 5. 判断是目录还是文件
        bool isDir = false;
        if (!name.empty() && name.back() == '/') {
            isDir = true;
            name.pop_back();   // 移除末尾的 '/'
        } else {
            // 包含点号且点号不在末尾 → 视为文件（有扩展名）
            size_t dot = name.find('.');
            if (dot != std::string::npos && dot != name.size() - 1) {
                isDir = false;
            } else {
                // 无点号或点号在末尾 → 视为目录
                isDir = true;
            }
        }

        entries.push_back({level, name, isDir, ""});
    }

    if (entries.empty()) {
        log = "未找到有效的目录树条目。";
        return false;
    }

    std::vector<std::string> pathStack;
    bool allSuccess = true;

    for (size_t i = 0; i < entries.size(); ++i) {
        Entry& e = entries[i];

        // 调整栈大小
        while ((int)pathStack.size() > e.level) pathStack.pop_back();

        if (e.level == 0 && pathStack.empty()) {
            pathStack.push_back(e.name);
        } else {
            // 确保当前层级正确，防止跳级
            if ((int)pathStack.size() != e.level) {
                log += "✗ 层级不匹配，跳过: " + e.name + "\n";
                allSuccess = false;
                continue;
            }
            pathStack.push_back(e.name);
        }

        // 构建完整路径
        fs::path full = fs::path(rootPath);
        for (const auto& seg : pathStack) {
            full /= seg;
        }
        e.path = full.string();

        if (e.isDirectory) {
            try {
                fs::create_directories(e.path);
                log += "✓ 创建目录: " + e.path + "\n";
            } catch (const std::exception& ex) {
                log += "✗ 创建目录失败: " + e.path + " - " + ex.what() + "\n";
                allSuccess = false;
            }
        } else {
            // 确保父目录存在
            fs::path parent = fs::path(e.path).parent_path();
            if (!parent.empty() && !fs::exists(parent)) {
                try {
                    fs::create_directories(parent);
                    log += "✓ 创建父目录: " + parent.string() + "\n";
                } catch (const std::exception& ex) {
                    log += "✗ 创建父目录失败: " + parent.string() + " - " + ex.what() + "\n";
                    allSuccess = false;
                    continue;
                }
            }

            if (!fs::exists(e.path)) {
                std::ofstream file(e.path, std::ios::binary);
                if (file) {
                    file.close();
                    log += "✓ 创建文件: " + e.path + "\n";
                } else {
                    log += "✗ 创建文件失败: " + e.path + "\n";
                    allSuccess = false;
                }
            } else {
                log += "○ 文件已存在，跳过: " + e.path + "\n";
            }
        }
    }
    return allSuccess;
}