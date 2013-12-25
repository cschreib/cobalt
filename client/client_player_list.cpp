#include "client_player_list.hpp"
#include <server_player_list.hpp>
#include "client_player.hpp"
#include <print.hpp>

namespace client {
    player_list::player_list(netcom& net) : net_(net), pool_(net) {
        // pool << net.watch_message<message::server::connection_granted>(
        //     [&](message::server::connection_granted msg) {
        //     net.send_request(client::netcom::server_actor_id,
        //         make_packet<request::client::join_players>("kalith", color32::blue, false),
        //         [](request::client::join_players::answer msg) {
        //             print("joined as player");
        //         }, [](request::client::join_players::failure msg) {
        //             error("could not join as player");
        //             std::string rsn;
        //             switch (msg.rsn) {
        //                 case request::client::join_players::failure::reason::too_many_players :
        //                     rsn = "too many players"; break;
        //             }
        //             reason(rsn);
        //         }, []() {
        //             error("could not join as player");
        //             reason("server does not support it");
        //         }
        //     );
        // });

        pool_.hold_all();
        net_.send_request(netcom::server_actor_id,
            make_packet<request::client::list_players>(),
            [&](request::client::list_players::answer msg) {
                for (auto& p : msg.players) {
                    players_.emplace_back(p.id, p.ip, p.name, p.color, p.is_ai);
                }

                pool_.release_all();
            }
        );

        pool_ << net_.watch_message([&](message::server::player_connected msg) {
            players_.emplace_back(msg.id, msg.ip, msg.name, msg.color, msg.is_ai);
        });

        pool_ << net_.watch_message([&](message::server::player_disconnected msg) {
            auto iter = players_.find_if([&](const player& p) { return p.id == msg.id; });
            players_.erase(iter);
        });

    }

    const player& player_list::get_player(actor_id_t id) const {
        auto iter = players_.find_if([&](const player& p) { return p.id == id; });
        return *iter;
    }
}
