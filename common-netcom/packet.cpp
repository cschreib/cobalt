#include "packet.hpp"
#include <color32.hpp>

namespace sf {
    #ifdef HAS_UINT64_T
    packet_t::base& operator << (packet_t::base& p, std::uint64_t data) {
        #ifdef BOOST_BIG_ENDIAN
        p << std::uint32_t(data >> 32) << std::uint32_t(data);
        #else
        p << std::uint32_t(data) << std::uint32_t(data >> 32);
        #endif

        return p;
    }

    packet_t::base& operator >> (packet_t::base& p, std::uint64_t& data) {
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

    packet_t::base& operator << (packet_t::base& p, impl_u64 data) {
        #ifdef BOOST_BIG_ENDIAN
        p << data.hi << data.lo;
        #else
        p << data.lo << data.hi;
        #endif

        return p;
    }

    packet_t::base& operator >> (packet_t::base& p, impl_u64& data) {
        #ifdef BOOST_BIG_ENDIAN
        p >> data.hi >> data.lo;
        #else
        p >> data.lo >> data.hi;
        #endif

        return p;
    }
    #endif

    packet_t::base& operator << (packet_t::base& o, ctl::empty_t) { return o; }
    packet_t::base& operator >> (packet_t::base& i, ctl::empty_t) { return i; }

    packet_t::base& operator << (packet_t::base& s, const color32& c) {
        return s << c.r << c.g << c.b << c.a;
    }

    packet_t::base& operator >> (packet_t::base& s, color32& c) {
        return s >> c.r >> c.g >> c.b >> c.a;
    }
}

#include "autogen/packet.cpp"
