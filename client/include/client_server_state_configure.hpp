#ifndef CLIENT_SERVER_STATE_CONFIGURE_HPP
#define CLIENT_SERVER_STATE_CONFIGURE_HPP

#include "client_server_state.hpp"
#include "client_player_list.hpp"
#include <config_shared_state.hpp>

namespace client {
namespace server_state {
    class game;

    class configure : public server_state::base {
        config::shared_state config_;

        std::unique_ptr<client::player_list> plist_;

        void transition_to_(server_state::base& st) override;

    public :
        explicit configure(server_instance& serv);

    };
}
}

#endif
