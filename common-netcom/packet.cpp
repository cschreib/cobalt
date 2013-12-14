#include "packet.hpp"
#include <color32.hpp>

#ifdef HAS_UINT64_T
sf::Packet& operator << (sf::Packet& p, std::uint64_t data) {
    #ifdef BOOST_BIG_ENDIAN
    p << std::uint32_t(data >> 32) << std::uint32_t(data);
    #else
    p << std::uint32_t(data) << std::uint32_t(data >> 32);
    #endif

    return p;
}

sf::Packet& operator >> (sf::Packet& p, std::uint64_t& data) {
    std::uint32_t lo, hi;
    #ifdef BOOST_BIG_ENDIAN
    p >> hi >> lo;
    #else
    p >> lo >> hi;
    #endif

    data = (std::uint64_t(hi) << 32) | std::uint64_t(lo);

    return p;
}
#else
bool impl_u64::operator< (const impl_u64& i) const {
    if (hi < i.hi) return true;
    if (hi > i.hi) return false;
    return lo < i.lo;
}

sf::Packet& operator << (sf::Packet& p, impl_u64 data) {
    #ifdef BOOST_BIG_ENDIAN
    p << data.hi << data.lo;
    #else
    p << data.lo << data.hi;
    #endif

    return p;
}

sf::Packet& operator >> (sf::Packet& p, impl_u64& data) {
    #ifdef BOOST_BIG_ENDIAN
    p >> data.hi >> data.lo;
    #else
    p >> data.lo >> data.hi;
    #endif

    return p;
}
#endif

sf::Packet& operator << (sf::Packet& s, const color32& c) {
    return s << c.r << c.g << c.b << c.a;
}

sf::Packet& operator >> (sf::Packet& s, color32& c) {
    return s >> c.r >> c.g >> c.b >> c.a;
}
