#include "Config.h"
#include "DPIHelper.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <windows.h>

nlohmann::json Config::CreateDefaultJson() {
    nlohmann::json j;
    
    // 默认服务器列表（至少一个）
    j["servers"] = nlohmann::json::array();
    nlohmann::json defaultServer;
    defaultServer["host"] = "localhost";
    defaultServer["port"] = 25565;
    j["servers"].push_back(defaultServer);
    
    j["current_server"] = 0;
    
    // 默认快捷方式
    j["shortcuts"] = nlohmann::json::array();
    j["shortcuts"].push_back({{"name", "DeepSeek"}, {"url", "https://chat.deepseek.com"}});
    j["shortcuts"].push_back({{"name", "GitHub"}, {"url", "https://github.com"}});
    j["shortcuts"].push_back({{"name", "B站"}, {"url", "https://bilibili.com"}});
    j["shortcuts"].push_back({{"name", "Minecraft官网"}, {"url", "https://www.minecraft.net"}});
    
    // UI 配置（逻辑像素，后面会根据 DPI 缩放）
    j["ui"]["edge_threshold"] = 10;
    j["ui"]["popup_width"] = 400;
    j["ui"]["popup_height"] = 300;
    
    // 按钮布局（可选，不配置则使用默认布局）
    j["buttons"] = nlohmann::json::array();
    
    return j;
}

bool Config::Load(const std::string& filePath) {
    nlohmann::json j;
    bool needSave = false;
    
    std::ifstream f(filePath);
    if (!f.is_open()) {
        // 文件不存在，使用默认配置
        j = CreateDefaultJson();
        needSave = true;
    } else {
        try {
            f >> j;
        } catch (const std::exception& e) {
            std::cerr << "Config parse error: " << e.what() << ", using defaults." << std::endl;
            j = CreateDefaultJson();
            needSave = true;
        }
    }
    
    // ---- 服务器列表 ----
    if (j.contains("servers") && j["servers"].is_array()) {
        for (const auto& item : j["servers"]) {
            ServerInfo si;
            si.host = item.value("host", "");
            si.port = item.value("port", 25565);
            if (!si.host.empty()) {
                servers.push_back(si);
            }
        }
    }
    if (servers.empty()) {
        // 保证至少有一个服务器
        ServerInfo local;
        local.host = "localhost";
        local.port = 25565;
        servers.push_back(local);
        needSave = true;
    }
    
    currentServer = j.value("current_server", 0);
    if (currentServer < 0 || currentServer >= (int)servers.size()) {
        currentServer = 0;
        needSave = true;
    }
    
    // ---- game 配置 ----
    if (j.contains("game") && j["game"].is_object()) {
        gameCommand = j["game"].value("command", "");
        if (j["game"].contains("args") && j["game"]["args"].is_array())
            gameArgs = j["game"]["args"].get<std::vector<std::string>>();
    } else {
        gameCommand = "";
        gameArgs.clear();
    }
    
    newsURL = j.value("news", nlohmann::json::object()).value("url", "");
    
    // ---- shortcuts ----
    if (j.contains("shortcuts") && j["shortcuts"].is_array()) {
        for (const auto& item : j["shortcuts"]) {
            Shortcut sc;
            sc.name = item.value("name", "");
            sc.url  = item.value("url", "");
            if (!sc.name.empty() && !sc.url.empty()) {
                shortcuts.push_back(sc);
            }
        }
    }
    // 保证至少有一个快捷方式（否则界面可能太空）
    if (shortcuts.empty()) {
        shortcuts.push_back({"DeepSeek", "https://chat.deepseek.com"});
        needSave = true;
    }
    
    // ---- ui 配置 ----
    int origWidth = 400, origHeight = 300;
    if (j.contains("ui") && j["ui"].is_object()) {
        edgeThreshold = j["ui"].value("edge_threshold", 10);
        origWidth     = j["ui"].value("popup_width", 400);
        origHeight    = j["ui"].value("popup_height", 300);
    } else {
        edgeThreshold = 10;
        origWidth = 400;
        origHeight = 300;
    }
    
    // 按钮布局
    buttonRects.clear();
    if (j.contains("buttons") && j["buttons"].is_array()) {
        for (const auto& btn : j["buttons"]) {
            ButtonRect br;
            br.id = btn.value("id", "");
            br.left = btn.value("left", 0);
            br.top = btn.value("top", 0);
            br.right = btn.value("right", 0);
            br.bottom = btn.value("bottom", 0);
            br.radius = btn.value("radius", 8);
            if (!br.id.empty()) buttonRects.push_back(br);
        }
    }
    
    // DPI 缩放
    double scale = GetDPIScale();
    popupWidth  = (int)(origWidth * scale);
    popupHeight = (int)(origHeight * scale);
    edgeThreshold = (int)(edgeThreshold * scale);
    for (auto& br : buttonRects) {
        br.left   = (int)(br.left * scale);
        br.top    = (int)(br.top * scale);
        br.right  = (int)(br.right * scale);
        br.bottom = (int)(br.bottom * scale);
        br.radius = (int)(br.radius * scale);
    }
    
    // 如果需要保存（文件缺失、解析错误、或必要字段缺失），写入文件
    if (needSave) {
        Save(filePath);
    }
    
    return true;
}

bool Config::Save(const std::string& filePath) {
    nlohmann::json j;
    for (const auto& sv : servers) {
        nlohmann::json js;
        js["host"] = sv.host;
        js["port"] = sv.port;
        j["servers"].push_back(js);
    }
    j["current_server"] = currentServer;
    
    // 读取现有文件以保留其他字段（如 ui, buttons, shortcuts 等）
    try {
        std::ifstream ifs(filePath);
        nlohmann::json full;
        if (ifs.is_open()) {
            ifs >> full;
            // 合并：保留 full 中除 servers/current_server 外的所有字段
            for (auto it = full.begin(); it != full.end(); ++it) {
                if (it.key() != "servers" && it.key() != "current_server") {
                    j[it.key()] = it.value();
                }
            }
        } else {
            // 如果是新文件，也写入 shortcuts 等默认值
            if (shortcuts.empty()) {
                j["shortcuts"] = nlohmann::json::array();
                j["shortcuts"].push_back({{"name", "DeepSeek"}, {"url", "https://chat.deepseek.com"}});
                j["shortcuts"].push_back({{"name", "GitHub"}, {"url", "https://github.com"}});
                j["shortcuts"].push_back({{"name", "B站"}, {"url", "https://bilibili.com"}});
                j["shortcuts"].push_back({{"name", "Minecraft官网"}, {"url", "https://www.minecraft.net"}});
            }
            if (!j.contains("ui")) {
                j["ui"]["edge_threshold"] = 10;
                j["ui"]["popup_width"] = 400;
                j["ui"]["popup_height"] = 300;
            }
        }
    } catch (...) {
        // 忽略合并错误
    }
    
    std::ofstream ofs(filePath);
    if (!ofs) return false;
    ofs << j.dump(4);
    return true;
}