#ifndef SERVER_STATE_IDDLE_HPP
#define SERVER_STATE_IDDLE_HPP

#include "server_state.hpp"

namespace server {
namespace state {
    class iddle : public base_impl<state_id::iddle> {
        scoped_connection_pool pool_;

    public :
        explicit iddle(server::instance& serv);
    };
}
}

namespace request {
namespace server {
    NETCOM_PACKET(new_game) {
        NETCOM_REQUIRES("admin");

        struct answer {};
        struct failure {};
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_state_iddle.hpp"
#endif

#endif
