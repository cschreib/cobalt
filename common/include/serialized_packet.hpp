#ifndef SERIALIZED_PACKET_HPP
#define SERIALIZED_PACKET_HPP

#include <SFML/Network/Packet.hpp>
#include <variadic.hpp>

namespace rob_impl {
    // Robery to expose private members of sf::Packet
    // Adapted from:
    // http://bloglitb.blogspot.fr/2010/07/access-to-private-members-thats-easy.html
    // https://gist.github.com/dabrahams/1528856

    template<typename T>
    struct stolen {
        static typename T::type ptr;
    };

    template<typename T>
    typename T::type stolen<T>::ptr;

    template<typename T, typename T::type P>
    struct rob {
        rob() {
            stolen<T>::ptr = P;
        }

        static rob robbed;
    };

    template<typename T, typename T::type P>
    rob<T,P> rob<T,P>::robbed;

    struct sf_packet_read_pos {
        using type = std::size_t (sf::Packet::*);
    };

    template struct rob<sf_packet_read_pos, &sf::Packet::m_readPos>;
}

struct serialized_packet_view;

class serialized_packet : public sf::Packet {
    std::size_t* read_pos_;

    typedef bool (Packet::*BoolType)(std::size_t);
    operator BoolType() = delete;

public :
    using base = sf::Packet;

    serialized_packet();
    serialized_packet(const serialized_packet& p);
    serialized_packet(serialized_packet&& p);
    serialized_packet(const sf::Packet& p);
    serialized_packet(sf::Packet&& p);

    serialized_packet& operator = (const serialized_packet& p);
    serialized_packet& operator = (serialized_packet&& p);
    serialized_packet& operator = (const sf::Packet& p);
    serialized_packet& operator = (sf::Packet&& p);

    std::size_t tellg() const;
    void seekg(std::size_t pos);

    serialized_packet_view view() const;

    explicit operator bool();
};

using packet_t = serialized_packet;

serialized_packet& operator << (serialized_packet& p, const serialized_packet& ip);
serialized_packet& operator >> (serialized_packet& p, serialized_packet& op);

std::istream& operator >> (std::istream& in, serialized_packet& p);
std::ostream& operator << (std::ostream& in, const serialized_packet& path);

template<typename T, typename enable =
    typename std::enable_if<!std::is_same<T,serialized_packet>::value>::type>
serialized_packet& operator << (serialized_packet& p, T&& t) {
    serialized_packet::base& bp = p;
    bp << std::forward<T>(t);
    return p;
}

template<typename T, typename enable =
    typename std::enable_if<!std::is_same<T,serialized_packet>::value>::type>
serialized_packet& operator >> (serialized_packet& p, T&& t) {
    serialized_packet::base& bp = p;
    bp >> std::forward<T>(t);
    return p;
}

struct serialized_packet_view {
    serialized_packet_view() = default;
    serialized_packet_view(const serialized_packet& p);

    std::size_t tellg() const;
    void seekg(std::size_t pos);

    template<typename T>
    serialized_packet_view& operator >> (T& t) {
        serialized_packet& p = const_cast<serialized_packet&>(*packet_);
        std::size_t opos = p.tellg();
        p.seekg(read_pos_);
        p >> t;
        read_pos_ = p.tellg();
        p.seekg(opos);
        return *this;
    }

private :
    const serialized_packet* packet_ = nullptr;
    std::size_t read_pos_ = 0;
};

#endif
