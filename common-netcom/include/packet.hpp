#ifndef PACKET_HPP
#define PACKET_HPP

#include <SFML/Network/Packet.hpp>
#include <vector>
#include <array>
#include <crc32.hpp>

struct color32;

namespace sf {
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

    sf::Packet& operator << (sf::Packet& s, const color32& c);
    sf::Packet& operator >> (sf::Packet& s, color32& c);
}

// Physical type of a packet identifier.
using packet_id_t = std::uint32_t;

std::string get_packet_name(packet_id_t id);

namespace packet_impl {
    template<packet_id_t ID>
    struct base {
        static const packet_id_t packet_id__ = ID;
        static const char* packet_name__;
    };

    template<packet_id_t ID>
    const packet_id_t base<ID>::packet_id__;

    template<typename T>
    struct packet_builder;
}

template<typename T, typename ... Args>
T make_packet(Args&& ... args) {
    return packet_impl::packet_builder<T>()(std::forward<Args>(args)...);
}

#define NETCOM_PACKET(name) \
    struct name : packet_impl::base<#name ## _crc32>

#endif
