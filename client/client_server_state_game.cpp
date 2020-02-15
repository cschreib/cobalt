#include "client_server_state_game.hpp"
#include <sol/sol.hpp>

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
        auto stbl = lua["server"].get_or_create<sol::table>();

        player_list::register_lua_type(lua);
        stbl["player_list"] = plist_.get();

        auto gtbl = stbl["game"].get_or_create<sol::table>();
    }

    void game::unregister_lua(sol::state& lua) {
        auto stbl = lua["server"].get<sol::table>();
        stbl["player_list"] = sol::nil;
        stbl["game"] = sol::nil;
    }
}
}
