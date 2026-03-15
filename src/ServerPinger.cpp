#include "ServerPinger.h"
#include "Utils.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#pragma comment(lib, "ws2_32.lib")

// 启用调试输出（0=关闭，1=打开）
#define ENABLE_DEBUG_OUTPUT 1

#if ENABLE_DEBUG_OUTPUT
#include <windows.h>
#include <sstream>
#define DEBUG_LOG(msg) { std::stringstream ss; ss << "[MCTool] " << msg << "\n"; OutputDebugStringA(ss.str().c_str()); }
#else
#define DEBUG_LOG(msg)
#endif

using json = nlohmann::json;
using namespace std::chrono;

// 辅助函数：从JSON description中提取纯文本（忽略格式）
static std::string ExtractMotdText(const json& desc) {
    if (desc.is_string()) {
        return desc.get<std::string>();
    } else if (desc.is_object()) {
        std::string result;
        if (desc.contains("text") && desc["text"].is_string()) {
            result += desc["text"].get<std::string>();
        }
        if (desc.contains("extra") && desc["extra"].is_array()) {
            for (const auto& item : desc["extra"]) {
                result += ExtractMotdText(item);
            }
        }
        return result;
    }
    return "[Complex MOTD]";
}

// 现代协议握手包构建
static std::vector<uint8_t> BuildHandshakePacket(const std::string& host, uint16_t port) {
    std::vector<uint8_t> packet;
    packet.push_back(0x00); // 包ID
    WriteVarInt(packet, -1); // 协议版本自动协商
    WriteVarInt(packet, host.size());
    packet.insert(packet.end(), host.begin(), host.end());
    packet.push_back((port >> 8) & 0xFF);
    packet.push_back(port & 0xFF);
    WriteVarInt(packet, 1); // 下一状态 status

    std::vector<uint8_t> finalPacket;
    WriteVarInt(finalPacket, packet.size());
    finalPacket.insert(finalPacket.end(), packet.begin(), packet.end());
    return finalPacket;
}

// 现代协议请求包构建
static std::vector<uint8_t> BuildStatusRequestPacket() {
    std::vector<uint8_t> packet;
    packet.push_back(0x00); // 包ID
    std::vector<uint8_t> finalPacket;
    WriteVarInt(finalPacket, packet.size());
    finalPacket.insert(finalPacket.end(), packet.begin(), packet.end());
    return finalPacket;
}

// 旧版协议查询 (1.6及以下)
static bool PingLegacy(SOCKET sock, const std::string& host, uint16_t port, ServerStatus& status) {
    DEBUG_LOG("Attempting legacy ping...");
    uint8_t legacyReq[] = { 0xFE, 0x01 };
    if (send(sock, (char*)legacyReq, sizeof(legacyReq), 0) != sizeof(legacyReq)) {
        DEBUG_LOG("Legacy send failed");
        return false;
    }
    DEBUG_LOG("Legacy request sent");

    uint8_t buffer[2048];
    int received = recv(sock, (char*)buffer, sizeof(buffer), 0);
    if (received < 3) {
        DEBUG_LOG("Legacy recv too short: " << received);
        return false;
    }
    if (buffer[0] != 0xFF) {
        DEBUG_LOG("Legacy response first byte not 0xFF: " << (int)buffer[0]);
        return false;
    }

    int dataLen = received - 3;
    if (dataLen % 2 != 0) {
        DEBUG_LOG("Legacy data length odd: " << dataLen);
        return false;
    }

    std::vector<wchar_t> wide(dataLen / 2 + 1);
    for (int i = 0; i < dataLen / 2; i++) {
        wide[i] = (buffer[3 + i * 2] << 8) | buffer[3 + i * 2 + 1];
    }
    wide[dataLen / 2] = 0;
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, NULL, 0, NULL, NULL);
    std::string utf8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, &utf8[0], len, NULL, NULL);
    DEBUG_LOG("Legacy raw UTF8: " << utf8);

    std::vector<std::string> parts;
    size_t pos = 0;
    while (pos < utf8.size()) {
        size_t next = utf8.find('\0', pos);
        if (next == std::string::npos) break;
        parts.push_back(utf8.substr(pos, next - pos));
        pos = next + 1;
    }
    DEBUG_LOG("Legacy parts count: " << parts.size());

    if (parts.size() >= 7) { // 至少需要 7 个字段
        status.online = true;
        status.version = parts[4];
        status.players = std::stoi(parts[5]);
        status.maxPlayers = std::stoi(parts[6]);
        status.motd = parts[3];
        DEBUG_LOG("Legacy success: version=" << status.version << " players=" << status.players << "/" << status.maxPlayers);
        return true;
    }
    DEBUG_LOG("Legacy parsing failed, need >=7 parts");
    return false;
}

// 现代协议查询
static bool PingModern(SOCKET sock, const std::string& host, uint16_t port, ServerStatus& status, long long& latencyMs) {
    DEBUG_LOG("Attempting modern ping...");
    auto start = high_resolution_clock::now();

    auto handshake = BuildHandshakePacket(host, port);
    if (send(sock, (char*)handshake.data(), handshake.size(), 0) != handshake.size()) {
        DEBUG_LOG("Modern handshake send failed");
        return false;
    }
    DEBUG_LOG("Modern handshake sent, size=" << handshake.size());

    auto request = BuildStatusRequestPacket();
    if (send(sock, (char*)request.data(), request.size(), 0) != request.size()) {
        DEBUG_LOG("Modern request send failed");
        return false;
    }
    DEBUG_LOG("Modern request sent, size=" << request.size());

    int packetLen;
    if (!ReadVarInt(sock, packetLen)) {
        DEBUG_LOG("Modern read packetLen failed");
        return false;
    }
    DEBUG_LOG("Modern packetLen=" << packetLen);

    int packetId;
    if (!ReadVarInt(sock, packetId) || packetId != 0) {
        DEBUG_LOG("Modern read packetId failed or not 0: " << packetId);
        return false;
    }
    DEBUG_LOG("Modern packetId=" << packetId);

    int jsonLen;
    if (!ReadVarInt(sock, jsonLen)) {
        DEBUG_LOG("Modern read jsonLen failed");
        return false;
    }
    DEBUG_LOG("Modern jsonLen=" << jsonLen);

    std::vector<char> jsonBuf(jsonLen + 1, 0);
    int total = 0;
    while (total < jsonLen) {
        int recvd = recv(sock, &jsonBuf[total], jsonLen - total, 0);
        if (recvd <= 0) {
            DEBUG_LOG("Modern recv error or closed, recvd=" << recvd);
            break;
        }
        total += recvd;
    }
    if (total != jsonLen) {
        DEBUG_LOG("Modern incomplete json, expected " << jsonLen << " got " << total);
        return false;
    }
    DEBUG_LOG("Modern json received, total=" << total);

    auto end = high_resolution_clock::now();
    latencyMs = duration_cast<milliseconds>(end - start).count();

    try {
        auto j = json::parse(jsonBuf.data());
        status.online = true;

        if (j.contains("version") && j["version"].is_object()) {
            status.version = j["version"]["name"].get<std::string>();
        }
        if (j.contains("players") && j["players"].is_object()) {
            status.players = j["players"]["online"].get<int>();
            status.maxPlayers = j["players"]["max"].get<int>();
        }
        if (j.contains("description")) {
            status.motd = ExtractMotdText(j["description"]);
        }
        DEBUG_LOG("Modern parse success: version=" << status.version 
                  << " players=" << status.players << "/" << status.maxPlayers);
        return true;
    } catch (const std::exception& e) {
        DEBUG_LOG("Modern JSON parse exception: " << e.what());
        return false;
    }
}

// 主函数：尝试现代协议，失败则尝试旧版
bool PingServer(const std::string& host, uint16_t port, ServerStatus& status) {
    DEBUG_LOG("PingServer start: host=" << host << " port=" << port);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        DEBUG_LOG("WSAStartup failed");
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        DEBUG_LOG("socket creation failed");
        WSACleanup();
        return false;
    }

    int timeout = 5000; // 5秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // 解析地址
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        // DNS解析
        struct hostent* he = gethostbyname(host.c_str());
        if (!he) {
            DEBUG_LOG("DNS resolution failed for " << host);
            closesocket(sock);
            WSACleanup();
            return false;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
        DEBUG_LOG("DNS resolved: " << inet_ntoa(addr.sin_addr));
    }

    // 连接
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        DEBUG_LOG("connect failed, error=" << WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return false;
    }
    DEBUG_LOG("Connected to " << host << ":" << port);

    long long latency = 0;
    bool success = PingModern(sock, host, port, status, latency);
    if (!success) {
        DEBUG_LOG("Modern ping failed, trying legacy...");
        success = PingLegacy(sock, host, port, status);
        if (success) {
            status.latency = 0;
            DEBUG_LOG("Legacy ping succeeded");
        } else {
            DEBUG_LOG("Legacy ping also failed");
        }
    } else {
        status.latency = static_cast<int>(latency);
        DEBUG_LOG("Modern ping succeeded, latency=" << status.latency << "ms");
    }

    closesocket(sock);
    WSACleanup();
    return success;
}