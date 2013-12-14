#ifndef CLIENT_PLAYER_LIST_HPP
#define CLIENT_PLAYER_LIST_HPP

#include <color32.hpp>
#include <ptr_vector.hpp>
#include "client_netcom.hpp"

namespace client {
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

#endif
