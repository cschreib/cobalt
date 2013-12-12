#ifndef PACKET_HPP
#define PACKET_HPP

#include <SFML/Network/Packet.hpp>
#include <vector>
#include <array>

// Extend sf::Packet to handle 64bit types
#ifdef HAS_UINT64_T
sf::Packet& operator << (sf::Packet& p, std::uint64_t data);
sf::Packet& operator >> (sf::Packet& p, std::uint64_t& data);
#else
struct impl_u64 {
    std::uint32_t lo, hi;
    bool operator < (const impl_u64& i) const;
};
sf::Packet& operator << (sf::Packet& p, impl_u64 data);
sf::Packet& operator >> (sf::Packet& p, impl_u64& data);
#endif

template<typename T, typename enable = typename std::enable_if<std::is_enum<T>::value>::type>
sf::Packet& operator >> (sf::Packet& p, T& t) {
    return p >> reinterpret_cast<typename std::underlying_type<T>::type&>(t);
}

template<typename T, typename enable = typename std::enable_if<std::is_enum<T>::value>::type>
sf::Packet& operator << (sf::Packet& p, T t) {
    return p << static_cast<typename std::underlying_type<T>::type>(t);
}

template<typename T>
sf::Packet& operator >> (sf::Packet& p, std::vector<T>& t) {
    std::uint32_t s;
    p >> s;
    std::uint32_t i0 = t.size();
    t.resize(i0 + s);
    for (std::uint32_t i = 0; i < s; ++i) {
        p >> t[i0+i];
    }
    return p;
}

template<typename T>
sf::Packet& operator << (sf::Packet& p, const std::vector<T>& t) {
    p << (std::uint32_t)t.size();
    for (auto& i : t) {
        p << i;
    }
    return p;
}

template<typename T, std::size_t N>
sf::Packet& operator >> (sf::Packet& p, std::array<T,N>& t) {
    for (std::size_t i = 0; i < N; ++i) {
        p >> t[i];
    }
    return p;
}

template<typename T, std::size_t N>
sf::Packet& operator << (sf::Packet& p, const std::array<T,N>& t) {
    for (auto& i : t) {
        p << i;
    }
    return p;
}

#ifdef NO_AUTOGEN
    #define PACKET_AUTOGEN_FOLDER ""
#else
    #define PACKET_AUTOGEN_FOLDER "autogen/packets/"
#endif

#endif
