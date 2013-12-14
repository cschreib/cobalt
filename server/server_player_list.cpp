#include "server_player_list.hpp"
#include "server_player.hpp"

namespace server {
    player_list::player_list(netcom& net) : net_(net) {
        pool_ << net_.watch_message<message::server::internal::client_connected>(
            [&](message::server::internal::client_connected msg) {
            net_.send_request(msg.id, make_packet<request::server::get_player_info>(),
                [&](request::server::get_player_info::answer ans) {
                players_.emplace_back(msg.id, msg.ip, ans.name, ans.color);

                net_.send_message(server::netcom::all_actor_id, 
                    make_packet<message::server::player_connected>(
                        msg.id, msg.ip, ans.name, ans.color
                    )
                );
            });
        });

        pool_ << net_.watch_message<message::server::internal::client_disconnected>(
            [&](message::server::internal::client_disconnected msg) {
            auto iter = players_.find_if([&](const player& p) { return p.id == msg.id; });
            players_.erase(iter);
            net_.send_message(server::netcom::all_actor_id,
                make_packet<message::server::player_disconnected>(msg.id)
            );
        });
    }
}
