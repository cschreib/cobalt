#ifndef CLIENT_PLAYER_HPP
#define CLIENT_PLAYER_HPP

#include <color32.hpp>
#include <netcom_base.hpp>
#include <string>

namespace client {
    struct player {
        player(actor_id_t id, const std::string& ip, const std::string& name, color32 color) : 
            id(id), ip(ip), name(name), color(color) {}

        actor_id_t id;
        std::string ip;
        std::string name;
        color32 color;
    };
}

#endif
