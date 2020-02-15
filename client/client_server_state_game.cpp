#include "client_server_state_game.hpp"
#include <sol.hpp>

namespace client {
namespace server_state {
    game::game(server_instance& serv) : server_state::base(serv, server::state_id::game, "game") {}

    void game::set_player_list(std::unique_ptr<client::player_list> plist) {
        plist_ = std::move(plist);
    }

    void game::end_of_transition_() {
        if (!plist_) {
            // Configure state didn't give us a player list, make our own
            plist_ = std::make_unique<client::player_list>(serv_);
            plist_->connect();
        }
    }

    void game::register_lua(sol::state& lua) {
        auto px = lua["server"];
        if (px.is<sol::nil_t>()) {
            lua.create_table("server");
        }

        auto stbl = px.get<sol::table>();
        plist_->register_lua(stbl);

        auto gtbl = stbl.create_table("game");
    }

    void game::unregister_lua(sol::state& lua) {
        auto tbl = lua["game"].get<sol::table>();
        plist_->unregister_lua(tbl);
        tbl["game"] = sol::nil;
    }
}
}
