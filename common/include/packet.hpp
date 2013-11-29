#ifndef PACKET_HPP
#define PACKET_HPP

#include <SFML/Network/Packet.hpp>

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

#endif
