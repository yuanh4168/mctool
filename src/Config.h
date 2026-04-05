#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct ServerInfo {
    std::string host;
    int port;
};

struct Shortcut {
    std::string name;
    std::string url;
};

struct ButtonRect {
    std::string id;
    int left;
    int top;
    int right;
    int bottom;
    int radius;
};

struct Config {
    std::vector<ServerInfo> servers;
    int currentServer;
    std::string newsURL;
    std::vector<Shortcut> shortcuts;
    int edgeThreshold;
    int popupWidth;
    int popupHeight;
    std::vector<ButtonRect> buttonRects;
    std::string gameCommand;
    std::vector<std::string> gameArgs;

    bool Load(const std::string& filePath);
    bool Save(const std::string& filePath);
    
    // 新增：生成默认配置的 JSON 对象
    static nlohmann::json CreateDefaultJson();
};