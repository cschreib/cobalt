#ifndef CRC32_HPP
#define CRC32_HPP

#include <cstdint>
#include <string>

constexpr std::uint32_t recursive_crc32(std::uint32_t id, const char* str, std::size_t i,
    std::size_t n, std::size_t j, std::uint32_t poly) {
    return j == 8 ?
        (i == n ? id : recursive_crc32(id, str, i+1, n, 0, poly)) :
        recursive_crc32(
            ((id >> 31) ^ ((str[i] >> j) & 1)) == 1 ?
                ((id << 1) ^ poly) : id << 1,
            str, i, n, j+1, poly
        );
}

std::uint32_t get_crc32(const std::string& str);

constexpr std::uint32_t operator "" _crc32(const char* str, std::size_t length) {
    return recursive_crc32(0, str, 0, length, 0, 0x4C11DB7);
}

namespace crc32_impl {
    template<std::uint32_t ID>
    struct base {
        static const std::uint32_t id = ID;
    };

    template<std::uint32_t ID>
    const std::uint32_t base<ID>::id;
}

#define ID_STRUCT(name) struct name : public crc32_impl::base<#name ## _crc32>

#endif
