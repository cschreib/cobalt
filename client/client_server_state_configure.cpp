#include "client_server_state_configure.hpp"
#include "client_server_state_game.hpp"
#include "client_server_instance.hpp"

namespace client {
namespace server_state {
    configure::configure(server_instance& serv) :
        server_state::base(serv, server::state_id::configure, "configure"),
        config_(net_, "server_state_configure") {

        plist_ = std::make_unique<client::player_list>(net_);
    }

    void configure::transition_to_(server_state::base& st) {
        if (st.id() == server::state_id::game) {
            server_state::game& gs = static_cast<server_state::game&>(st);
            gs.set_player_list(std::move(plist_));
        }
    }
}
}
