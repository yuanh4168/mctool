#pragma once
#include <string>
#include <cstdint>

struct ServerStatus {
    bool online = false;
    std::string motd;
    int players = 0;
    int maxPlayers = 0;
    std::string version;
    int latency = 0;          // 延迟（毫秒）
};

// 主函数：尝试现代协议，失败则尝试旧版
bool PingServer(const std::string& host, uint16_t port, ServerStatus& status);