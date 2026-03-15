#include "NewsFetcher.h"
#include <windows.h>
#include <wininet.h>
#include <string>

std::string FetchNews(const std::string& url) {
    // 使用 ANSI 版本的 InternetOpenA，传递普通字符串常量
    HINTERNET hNet = InternetOpenA("MCTool", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hNet) return "";

    HINTERNET hUrl = InternetOpenUrlA(hNet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        InternetCloseHandle(hNet);
        return "";
    }

    char buffer[4096];
    DWORD bytesRead;
    std::string result;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    return result;
}