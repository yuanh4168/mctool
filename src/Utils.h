#pragma once
#include <vector>
#include <cstdint>
#include <winsock2.h>

bool ReadVarInt(SOCKET sock, int& out);
void WriteVarInt(std::vector<uint8_t>& buffer, int value);