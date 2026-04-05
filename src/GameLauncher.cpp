#include "GameLauncher.h"
#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>

bool LaunchGame() {
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring exeDir = modulePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));
    std::wstring batPathW = exeDir + L"\\start_game.bat";

    // 将宽字符串转换为窄字符串（UTF-8）
    std::string batPath = std::filesystem::path(batPathW).string();

    if (!std::filesystem::exists(batPath)) {
        // 自动创建模板 bat 文件
        std::ofstream batFile(batPath);
        if (batFile) {
            batFile << "@echo off\n";
            batFile << "echo 请编辑此文件，填入你的 Minecraft 启动命令。\n";
            batFile << "echo 例如：\n";
            batFile << "echo   start \"\" \"C:\\Program Files\\Minecraft Launcher\\MinecraftLauncher.exe\"\n";
            batFile << "echo 或直接启动 Java：\n";
            batFile << "echo   javaw -Xmx2G -jar \"minecraft_client.jar\"\n";
            batFile << "pause\n";
            batFile.close();
            MessageBoxW(NULL, L"未找到 start_game.bat，已自动创建模板文件。\n请编辑该文件后再次点击“启动游戏”。", L"提示", MB_OK);
            return false;
        } else {
            MessageBoxW(NULL, L"无法创建 start_game.bat 文件，请检查目录权限。", L"错误", MB_ICONERROR);
            return false;
        }
    }

    HINSTANCE h = ShellExecuteW(NULL, L"open", batPathW.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(h) > 32;
}