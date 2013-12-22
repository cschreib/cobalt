#ifndef SERVER_PLAYER_HPP
#define SERVER_PLAYER_HPP

#include <color32.hpp>
#include <netcom_base.hpp>
#include <string>

namespace server {
    struct player {
        player(actor_id_t id, const std::string& ip, const std::string& name, color32 color,
            bool is_ai) : id(id), ip(ip), name(name), color(color), is_ai(is_ai) {}

        player(const player&) = delete;
        player& operator = (const player&) = delete;

        actor_id_t id;
        std::string ip;
        std::string name;
        color32 color;
        bool is_ai;
        double when_connected;
    };
}

#endif
