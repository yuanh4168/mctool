#include "Config.h"
#include <fstream>
#include <iostream>

bool Config::Load(const std::string& filePath) {
    std::ifstream f(filePath);
    if (!f.is_open()) return false;
    try {
        nlohmann::json j;
        f >> j;

        // Game
        gameCommand = j["game"]["command"];
        gameArgs = j["game"]["args"].get<std::vector<std::string>>();

        // Server
        serverHost = j["server"]["host"];
        serverPort = j["server"]["port"];

        // News
        newsURL = j["news"]["url"];

        // Shortcuts
        if (j.contains("shortcuts") && j["shortcuts"].is_array()) {
            for (const auto& item : j["shortcuts"]) {
                Shortcut sc;
                sc.name = item["name"];
                sc.url  = item["url"];
                shortcuts.push_back(sc);
            }
        }

        // UI
        edgeThreshold = j["ui"]["edge_threshold"];
        popupWidth = j["ui"]["popup_width"];
        popupHeight = j["ui"]["popup_height"];

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config parse error: " << e.what() << std::endl;
        return false;
    }
}