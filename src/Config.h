#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct Config {
    std::string gameCommand;
    std::vector<std::string> gameArgs;
    std::string serverHost;
    int serverPort;
    std::string newsURL;
    int edgeThreshold;
    int popupWidth;
    int popupHeight;

    bool Load(const std::string& filePath);
};