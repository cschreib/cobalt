#ifndef SERVER_STATE_GAME_HPP
#define SERVER_STATE_GAME_HPP

#include "server_state.hpp"

namespace server {
namespace state {
    class game : public base_impl<state_id::game> {
        scoped_connection_pool pool_;

    public :
        explicit game(server::instance& serv);
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_state_game.hpp"
#endif

#endif
