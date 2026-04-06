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
    
    // UI 配置
    j["ui"]["edge_threshold"] = 10;
    j["ui"]["popup_width"] = 400;
    j["ui"]["popup_height"] = 300;
    
    j["buttons"] = nlohmann::json::array();
    
    // 新增默认配置
    j["time_display"]["enabled"] = true;
    j["time_display"]["format"] = "HH:mm:ss";
    j["reminder"]["enabled"] = false;
    j["reminder"]["interval_minutes"] = 15;
    j["reminder"]["message"] = "该休息一会儿了，站起来活动一下吧";
    j["server_monitor"]["background_enabled"] = false;
    j["server_monitor"]["interval_seconds"] = 60;
    j["server_monitor"]["max_data_points"] = 30;
    
    return j;
}

bool Config::Load(const std::string& filePath) {
    nlohmann::json j;
    bool needSave = false;
    
    std::ifstream f(filePath);
    if (!f.is_open()) {
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
    
    // ========== 新增配置加载 ==========
    if (j.contains("time_display") && j["time_display"].is_object()) {
        timeDisplay.enabled = j["time_display"].value("enabled", true);
        timeDisplay.format = j["time_display"].value("format", "HH:mm:ss");
    }
    if (j.contains("reminder") && j["reminder"].is_object()) {
        reminder.enabled = j["reminder"].value("enabled", false);
        reminder.intervalMinutes = j["reminder"].value("interval_minutes", 15);
        reminder.message = j["reminder"].value("message", "该休息一会儿了，站起来活动一下吧");
    }
    if (j.contains("server_monitor") && j["server_monitor"].is_object()) {
        serverMonitor.backgroundEnabled = j["server_monitor"].value("background_enabled", false);
        serverMonitor.intervalSeconds = j["server_monitor"].value("interval_seconds", 60);
        serverMonitor.maxDataPoints = j["server_monitor"].value("max_data_points", 30);
    }
    
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
    
    try {
        std::ifstream ifs(filePath);
        nlohmann::json full;
        if (ifs.is_open()) {
            ifs >> full;
            for (auto it = full.begin(); it != full.end(); ++it) {
                if (it.key() != "servers" && it.key() != "current_server") {
                    j[it.key()] = it.value();
                }
            }
        } else {
            // 默认值
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
            // 新增默认值
            if (!j.contains("time_display")) {
                j["time_display"]["enabled"] = true;
                j["time_display"]["format"] = "HH:mm:ss";
            }
            if (!j.contains("reminder")) {
                j["reminder"]["enabled"] = false;
                j["reminder"]["interval_minutes"] = 15;
                j["reminder"]["message"] = "该休息一会儿了，站起来活动一下吧";
            }
            if (!j.contains("server_monitor")) {
                j["server_monitor"]["background_enabled"] = false;
                j["server_monitor"]["interval_seconds"] = 60;
                j["server_monitor"]["max_data_points"] = 30;
            }
        }
    } catch (...) {}
    
    std::ofstream ofs(filePath);
    if (!ofs) return false;
    ofs << j.dump(4);
    return true;
}