#ifndef UUI_HPP
#define UUI_HPP

#include <cstdint>
#include <array>
#include <iosfwd>

namespace sf {
    class Packet;
}

struct uuid_t {
    std::array<std::uint32_t,4> data_;

    bool operator <  (const uuid_t&) const;
    bool operator >  (const uuid_t&) const;
    bool operator <= (const uuid_t&) const;
    bool operator >= (const uuid_t&) const;
    bool operator == (const uuid_t&) const;
    bool operator != (const uuid_t&) const;
};

std::ostream& operator << (std::ostream& out, const uuid_t& id);
sf::Packet& operator << (sf::Packet& out, const uuid_t& id);
sf::Packet& operator >> (sf::Packet& out, uuid_t& id);

namespace impl {
    uuid_t make_uuid_(std::uintptr_t obj);
}

template<typename T>
uuid_t make_uuid(T& obj) {
    return impl::make_uuid_(reinterpret_cast<std::uintptr_t>(&obj));
}

#endif
