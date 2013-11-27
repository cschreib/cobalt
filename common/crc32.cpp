#include "crc32.hpp"

std::uint32_t get_crc32(const std::string& str) {
    return recursive_crc32(0, str.c_str(), 0, str.size(), 0, 0x4C11DB7);
}
