#pragma once

#include <cstdint>
#include <string>
#include <vector>

void md5(const uint8_t* data, size_t len, uint32_t out[4]);
std::string md5_hexdigest(const std::vector<unsigned char>& data);
