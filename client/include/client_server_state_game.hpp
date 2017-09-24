#ifndef CLIENT_SERVER_STATE_GAME_HPP
#define CLIENT_SERVER_STATE_GAME_HPP

#include "client_server_state.hpp"
#include "client_player_list.hpp"

namespace client {
namespace server_state {
    class game : public server_state::base {
        std::unique_ptr<client::player_list> plist_;

        virtual void end_of_transition_() override;

    public :
        explicit game(server_instance& serv);

        void set_player_list(std::unique_ptr<client::player_list> plist);
    };
}
}

#endif
