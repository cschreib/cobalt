#ifndef SERVER_STATE_IDLE_HPP
#define SERVER_STATE_IDLE_HPP

#include "server_state.hpp"

namespace server {
namespace state {
    class idle : public base {
        scoped_connection_pool pool_;

    public :
        explicit idle(server::instance& serv);

        void register_callbacks() override;
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
#include "autogen/packets/server_state_idle.hpp"
#endif

#endif
