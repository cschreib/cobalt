#ifndef CLIENT_PLAYER_LIST_HPP
#define CLIENT_PLAYER_LIST_HPP

#include <color32.hpp>
#include <ptr_vector.hpp>
#include <connection_handler.hpp>
#include "client_netcom.hpp"
#include "client_player.hpp"

namespace client {
    class player_list {
    public :
        explicit player_list(netcom& net);

        bool is_player(actor_id_t id) const;
        const player& get_player(actor_id_t id) const;

        void join_as(const std::string& name, const color32& col, bool as_ai);

        // TODO: implement this (client and server side)
        void leave();

        bool is_joined() const;
        const player& get_self() const;

    // private :
    //     handler_group
    // public :
    //     template<typename T>
    //     void on_joined()


    private :
        netcom& net_;

        ctl::ptr_vector<player> players_;
        player* self_;

        scoped_connection_pool pool_;
    };
}

#endif
