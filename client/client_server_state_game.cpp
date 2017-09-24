#include "client_server_state_game.hpp"

namespace client {
namespace server_state {
    game::game(server_instance& serv) : server_state::base(serv, server::state_id::game, "game") {}

    void game::set_player_list(std::unique_ptr<client::player_list> plist) {
        plist_ = std::move(plist);
    }

    void game::end_of_transition_() {
        if (!plist_) {
            // Configure state didn't give us a player list, make our own
            plist_ = std::make_unique<client::player_list>(net_);
        }
    }
}
}
