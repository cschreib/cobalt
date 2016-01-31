#include "serialized_packet.hpp"
#include <fstream>

serialized_packet::serialized_packet() :
    read_pos_(&(this->*rob_impl::stolen<rob_impl::sf_packet_read_pos>::ptr)) {}

serialized_packet::serialized_packet(const serialized_packet& p) : sf::Packet(p),
    read_pos_(&(this->*rob_impl::stolen<rob_impl::sf_packet_read_pos>::ptr)) {}

serialized_packet::serialized_packet(serialized_packet&& p) : sf::Packet(std::move(p)),
    read_pos_(&(this->*rob_impl::stolen<rob_impl::sf_packet_read_pos>::ptr)) {}

serialized_packet::serialized_packet(const sf::Packet& p) : sf::Packet(p),
    read_pos_(&(this->*rob_impl::stolen<rob_impl::sf_packet_read_pos>::ptr)) {}

serialized_packet::serialized_packet(sf::Packet&& p) : sf::Packet(std::move(p)),
    read_pos_(&(this->*rob_impl::stolen<rob_impl::sf_packet_read_pos>::ptr)) {}

serialized_packet& serialized_packet::operator = (const serialized_packet& p) {
    sf::Packet::operator=(p);
    return *this;
}

serialized_packet& serialized_packet::operator = (serialized_packet&& p) {
    sf::Packet::operator=(std::move(p));
    return *this;
}

serialized_packet& serialized_packet::operator = (const sf::Packet& p) {
    sf::Packet::operator=(p);
    return *this;
}

serialized_packet& serialized_packet::operator = (sf::Packet&& p) {
    sf::Packet::operator=(std::move(p));
    return *this;
}

std::size_t serialized_packet::tellg() const {
    return *read_pos_;
}

void serialized_packet::seekg(std::size_t pos) {
    *read_pos_ = pos;
}

serialized_packet_view serialized_packet::view() const {
    return serialized_packet_view(*this);
}

serialized_packet::operator bool() {
    using btype = bool (sf::Packet::*)(std::size_t);
    return sf::Packet::operator btype() != nullptr;
}

serialized_packet& operator << (serialized_packet& p, const serialized_packet& ip) {
    if (ip.endOfPacket()) return p;
    std::size_t pos = ip.tellg();
    p.append(static_cast<const char*>(ip.getData()) + pos, ip.getDataSize() - pos);
    return p;
}

serialized_packet& operator >> (serialized_packet& p, serialized_packet& op) {
    if (p.endOfPacket()) return p;
    std::size_t pos = p.tellg();
    op.append(static_cast<const char*>(p.getData()) + pos, p.getDataSize() - pos);
    p.seekg(p.getDataSize());
    return p;
}

std::istream& operator >> (std::istream& in, serialized_packet& p) {
    // Use a temporary packet to deserialize the data size
    std::uint32_t s;
    serialized_packet tp;
    std::vector<char> buffer(sizeof(s));
    in.read(buffer.data(), buffer.size());
    tp.append(static_cast<const void*>(buffer.data()), buffer.size());
    tp >> s;

    // Then read the actual data
    buffer.resize(s);
    in.read(buffer.data(), s);
    p.append(buffer.data(), s);

    return in;
}

std::ostream& operator << (std::ostream& out, const serialized_packet& p) {
    // Use a temporary pack to serialize the data size
    serialized_packet tp;
    std::uint32_t s = p.getDataSize();
    tp << s;
    out.write(static_cast<const char*>(tp.getData()), tp.getDataSize());

    // Then write the actual data
    out.write(static_cast<const char*>(p.getData()), p.getDataSize());

    return out;
}

serialized_packet_view::serialized_packet_view(const serialized_packet& p) :
    packet_(&p), read_pos_(p.tellg()) {}

std::size_t serialized_packet_view::tellg() const {
    return read_pos_;
}

void serialized_packet_view::seekg(std::size_t pos) {
    read_pos_ = pos;
}
