#ifndef CLIENT_PLAYER_LIST_HPP
#define CLIENT_PLAYER_LIST_HPP

#include <color32.hpp>
#include <ptr_vector.hpp>
#include "client_netcom.hpp"
#include "client_player.hpp"

namespace client {
    class player_list {
    public :
        explicit player_list(netcom& net);

        const player& get_player(actor_id_t id) const;
        const player& get_self() const;

    private :
        netcom& net_;
        netcom::watch_pool_t pool_;

        ptr_vector<player> players_;

        player* self_;
    };
}

#endif
