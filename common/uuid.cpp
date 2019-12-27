#include "uuid.hpp"
#include "range.hpp"
#include <SFML/Network/Packet.hpp>
#include <iostream>
#include <chrono>

bool uuid_t::operator <  (const uuid_t& id) const {
    for (std::size_t i : range(data_.size()-1)) {
        if (data_[i] < id.data_[i]) return true;
        if (data_[i] > id.data_[i]) return false;
    }

    return data_[data_.size()-1] < id.data_[data_.size()-1];
}

bool uuid_t::operator >  (const uuid_t& id) const {
    for (std::size_t i : range(data_.size()-1)) {
        if (data_[i] > id.data_[i]) return true;
        if (data_[i] < id.data_[i]) return false;
    }

    return data_[data_.size()-1] > id.data_[data_.size()-1];
}

bool uuid_t::operator <= (const uuid_t& id) const {
    for (std::size_t i : range(data_.size()-1)) {
        if (data_[i] < id.data_[i]) return true;
        if (data_[i] > id.data_[i]) return false;
    }

    return data_[data_.size()-1] <= id.data_[data_.size()-1];
}

bool uuid_t::operator >= (const uuid_t& id) const {
    for (std::size_t i : range(data_.size()-1)) {
        if (data_[i] > id.data_[i]) return true;
        if (data_[i] < id.data_[i]) return false;
    }

    return data_[data_.size()-1] >= id.data_[data_.size()-1];
}

bool uuid_t::operator == (const uuid_t& id) const {
    for (std::size_t i : range(data_)) {
        if (data_[i] != id.data_[i]) return false;
    }

    return true;
}

bool uuid_t::operator != (const uuid_t& id) const {
    for (std::size_t i : range(data_)) {
        if (data_[i] != id.data_[i]) return true;
    }

    return false;
}

std::ostream& operator << (std::ostream& out, const uuid_t& id) {
    auto fmt = out.flags();
    out << std::hex;
    out << id.data_[0];
    for (std::size_t i : range(1, id.data_)) {
        out << '-' << id.data_[i];
    }
    out.flags(fmt);

    return out;
}

sf::Packet& operator << (sf::Packet& out, const uuid_t& id) {
    for (std::size_t i : range(id.data_)) {
        out << id.data_[i];
    }

    return out;
}

sf::Packet& operator >> (sf::Packet& in, uuid_t& id) {
    for (std::size_t i : range(id.data_)) {
        in >> id.data_[i];
    }

    return in;
}

namespace impl {
    void assign_ptr_(uuid_t& id, std::uintptr_t obj, std::true_type) {
        id.data_[2] = std::uint32_t(obj >> 32);
        id.data_[3] = std::uint32_t(obj);
    }

    void assign_ptr_(uuid_t& id, std::uintptr_t obj, std::false_type) {
        id.data_[2] = 0;
        id.data_[3] = obj;
    }

    uuid_t make_uuid_(std::uintptr_t obj) {
        static_assert(sizeof(obj) <= 2*sizeof(std::uint32_t),
                      "pointer size must be less or equal to 64bit");

        uuid_t id;

        auto chrono = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

        static_assert(sizeof(chrono) == 2*sizeof(std::uint32_t),
                      "chrono::nanoseconds type must be 64bit");

        id.data_[0] = std::uint32_t(chrono >> 32);
        id.data_[1] = std::uint32_t(chrono);

        assign_ptr_(id, obj, std::integral_constant<bool, (sizeof(obj) > sizeof(std::uint32_t))>{});

        return id;
    }
}
