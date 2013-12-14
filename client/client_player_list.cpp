#include "client_player_list.hpp"
#include <server_player_list.hpp>
#include "client_player.hpp"

namespace client {
    player_list::player_list(netcom& net) : net_(net) {
        pool_ << net_.watch_message<message::server::player_connected>(
            [&](message::server::player_connected msg) {
            players_.emplace_back(msg.id, msg.ip, msg.name, msg.color);
        });

        pool_ << net_.watch_message<message::server::player_disconnected>(
            [&](message::server::player_disconnected msg) {
            auto iter = players_.find_if([&](const player& p) { return p.id == msg.id; });
            players_.erase(iter);
        });
    }
}
