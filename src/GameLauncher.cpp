#include "GameLauncher.h"
#include <windows.h>
#include <string>

bool LaunchGame(const Config& cfg) {
    std::string cmdLine = "\"" + cfg.gameCommand + "\"";
    for (const auto& arg : cfg.gameArgs) {
        cmdLine += " \"" + arg + "\"";
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}