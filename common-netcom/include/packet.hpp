#ifndef PACKET_HPP
#define PACKET_HPP

#include <SFML/Network/Packet.hpp>
#include <vector>
#include <atomic>
#include <array>
#include <crc32.hpp>
#include <variadic.hpp>
#include <sorted_vector.hpp>
#include <range.hpp>
#include <serialized_packet.hpp>

struct color32;

namespace sf {
    // Extend sf::Packet to handle 64bit types
    #ifdef HAS_UINT64_T
    packet_t::base& operator << (packet_t::base& p, std::uint64_t data);
    packet_t::base& operator >> (packet_t::base& p, std::uint64_t& data);
    #else
    struct impl_u64 {
        std::uint32_t lo, hi;
        bool operator < (const impl_u64& i) const;
    };
    packet_t::base& operator << (packet_t::base& p, impl_u64 data);
    packet_t::base& operator >> (packet_t::base& p, impl_u64& data);
    #endif

    template<typename T, typename enable = typename std::enable_if<std::is_enum<T>::value>::type>
    packet_t::base& operator >> (packet_t::base& p, T& t) {
        return p >> reinterpret_cast<typename std::underlying_type<T>::type&>(t);
    }

    template<typename T, typename enable = typename std::enable_if<std::is_enum<T>::value>::type>
    packet_t::base& operator << (packet_t::base& p, T t) {
        return p << static_cast<typename std::underlying_type<T>::type>(t);
    }

    packet_t::base& operator << (packet_t::base& o, ctl::empty_t);
    packet_t::base& operator >> (packet_t::base& i, ctl::empty_t);

    template<typename T>
    packet_t::base& operator >> (packet_t::base& p, std::vector<T>& t) {
        std::uint32_t s;
        p >> s;
        std::uint32_t i0 = t.size();
        t.resize(i0 + s);
        for (std::uint32_t i : range(s)) {
            p >> t[i0+i];
        }
        return p;
    }

    template<typename T>
    packet_t::base& operator << (packet_t::base& p, const std::vector<T>& t) {
        p << static_cast<std::uint32_t>(t.size());
        for (auto& i : t) {
            p << i;
        }
        return p;
    }

    template<typename T, typename C>
    packet_t::base& operator >> (packet_t::base& p, ctl::sorted_vector<T,C>& t) {
        std::uint32_t s; p >> s;
        for (std::uint32_t i : range(s)) {
            T tmp;
            p >> tmp;
            t.insert(std::move(tmp));
        }
        return p;
    }

    template<typename T, typename C>
    packet_t::base& operator << (packet_t::base& p, const ctl::sorted_vector<T,C>& t) {
        p << static_cast<std::uint32_t>(t.size());
        for (auto& i : t) {
            p << i;
        }
        return p;
    }

    template<typename T, std::size_t N>
    packet_t::base& operator >> (packet_t::base& p, std::array<T,N>& t) {
        for (std::size_t i : range(N)) {
            p >> t[i];
        }
        return p;
    }

    template<typename T, std::size_t N>
    packet_t::base& operator << (packet_t::base& p, const std::array<T,N>& t) {
        for (auto& i : t) {
            p << i;
        }
        return p;
    }

    template<typename T>
    packet_t::base& operator >> (packet_t::base& p, std::atomic<T>& t) {
        T tmp; p >> tmp;
        t = tmp;
        return p;
    }

    template<typename T, typename C>
    packet_t::base& operator << (packet_t::base& p, const std::atomic<T>& t) {
        p << t.load();
        return p;
    }

    packet_t::base& operator << (packet_t::base& s, const color32& c);
    packet_t::base& operator >> (packet_t::base& s, color32& c);
}

/// Physical type of a packet identifier.
using packet_id_t = std::uint32_t;

/// Return a user readable name for a given packet.
std::string get_packet_name(packet_id_t id);

/// Check if the provided ID matches that of a real packet.
bool is_packet_id(packet_id_t id);

namespace packet_impl {
    struct base_ {};

    template<packet_id_t ID>
    struct base : base_ {
        static const packet_id_t packet_id__ = ID;
        static const char* packet_name__;
    };

    template<packet_id_t ID>
    const packet_id_t base<ID>::packet_id__;

    template<typename T>
    using is_packet = std::is_base_of<base_, T>;
}

#define NETCOM_PACKET(name) \
    struct name : packet_impl::base<#name ## _crc32>

namespace packet_impl {
    template<typename T>
    struct packet_builder;

    template<typename T, typename ... Args>
    T make_packet_impl(std::true_type, Args&& ... args) {
        return {{}, std::forward<Args>(args)...};
    }

    template<typename T, typename ... Args>
    T make_packet_impl(std::false_type, Args&& ... args) {
        return {std::forward<Args>(args)...};
    }
}

/// Create a packet and set its elements.
template<typename T, typename ... Args>
T make_packet(Args&& ... args) {
    return packet_impl::make_packet_impl<T>(packet_impl::is_packet<T>{}, std::forward<Args>(args)...);
}

template<typename T>
T make_packet() {
    return T{};
}

/// Write a list of objects into a packet.
template<typename P>
void packet_write(P& p) {}

template<typename P, typename T, typename ... Args>
void packet_write(P& p, T&& t, Args&& ... args) {
    p << std::forward<T>(t);
    packet_write(p, std::forward<Args>(args)...);
}

/// Read a list of objects from a packet.
template<typename P>
void packet_read(P& p) {}

template<typename P, typename T, typename ... Args>
void packet_read(P& p, T& t, Args& ... args) {
    p >> t;
    packet_read(p, args...);
}

#endif
