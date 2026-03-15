#include "Utils.h"
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

bool ReadVarInt(SOCKET sock, int& out) {
    out = 0;
    int bytesRead = 0;
    uint8_t b;
    do {
        int ret = recv(sock, (char*)&b, 1, 0);
        if (ret != 1) return false;
        out |= (b & 0x7F) << (7 * bytesRead);
        bytesRead++;
        if (bytesRead > 5) return false;
    } while (b & 0x80);
    return true;
}

void WriteVarInt(std::vector<uint8_t>& buffer, int value) {
    do {
        uint8_t b = value & 0x7F;
        value >>= 7;
        if (value != 0) b |= 0x80;
        buffer.push_back(b);
    } while (value != 0);
}