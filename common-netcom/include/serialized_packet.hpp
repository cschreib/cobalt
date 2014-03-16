#ifndef SERIALIZED_PACKET_HPP
#define SERIALIZED_PACKET_HPP

#include <SFML/Network/Packet.hpp>

namespace netcom {
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

        template struct rob<sf_packet_read_pos, &sf::Packet::m_read_pos>;
    }

    class serialized_packet : public sf::Packet {
        std::size_t* read_pos_;

    public :
        serialized_packet() {
            read_pos_ = &(this->*rob_impl::stolen<rob_impl::sf_packet_read_pos>::ptr);
        }

        virtual ~serialized_packet();

        std::size_t tellg() const {
            return *read_pos_;
        }
        void seekg(std::size_t pos) const {
            *read_pos_ = pos;
        }

        using sf::Packet::operator <<;
        using sf::Packet::operator >>;
        using sf::Packet::operator sf::Packet::BoolType;
    };

    serialized_packet& operator << (serialized_packet& p, const serialized_packet& ip) {
        std::size_t pos = ip.tellg();
        if (pos >= ip.getDataSize()) return p;
        p.append(ip.getData() + pos, ip.getDataSize() - pos);
        return p;
    }

    serialized_packet& operator >> (serialized_packet& p, serialized_packet& op) {
        std::size_t pos = p.tellg();
        if (pos >= p.getDataSize()) return p;
        op.append(p.getData() + pos, p.getDataSize() - pos);
        return p;
    }
}
