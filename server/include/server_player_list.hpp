#ifndef SERVER_PLAYER_LIST_HPP
#define SERVER_PLAYER_LIST_HPP

#include <color32.hpp>
#include <ptr_vector.hpp>
#include "server_netcom.hpp"

namespace server {
    struct player;

    class player_list {
    public :
        explicit player_list(netcom& net);

    private :
        netcom& net_;
        netcom::watch_pool_t pool_;

        ptr_vector<player> players_;
    };
}

namespace request {
namespace server {
    NETCOM_PACKET(get_player_info) {
        struct answer {
            std::string name;
            color32 color;
        };
        struct failure {};
    };
}
}

namespace message {
namespace server {
    NETCOM_PACKET(player_connected) {
        actor_id_t id;
        std::string ip, name;
        color32 color;
    };
    NETCOM_PACKET(player_disconnected) {
        actor_id_t id;
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_player_list.hpp" 
#endif

#endif
