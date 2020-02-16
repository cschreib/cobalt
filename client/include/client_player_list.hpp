#ifndef CLIENT_PLAYER_LIST_HPP
#define CLIENT_PLAYER_LIST_HPP

#include <color32.hpp>
#include <ptr_vector.hpp>
#include <scoped_connection_pool.hpp>
#include <shared_collection.hpp>
#include "client_netcom.hpp"
#include "server_player_list.hpp"
#include "client_player.hpp"

namespace sol {
    class state;
}

namespace client {
    class server_instance;

    class player_list {
    public :
        explicit player_list(server_instance& serv);

        static void register_lua_type(sol::state& lua);

        void connect();
        signal_t<void()> on_list_received;
        signal_t<void()> on_connect_fail;

        void disconnect();
        signal_t<void()> on_disconnect;

        bool is_connected() const;

        bool is_player(actor_id_t id) const;
        const player& get_player(actor_id_t id) const;
        signal_t<void(player&)>       on_player_connected;
        signal_t<void(const player&)> on_player_disconnected;

        void join_as(const std::string& name, const color32& col, bool as_ai);
        signal_t<void(player&)> on_join;
        signal_t<void()>        on_join_fail;

        void leave();
        signal_t<void()> on_leave;

        bool is_joined() const;
        const player* get_self() const;

        using iterator = ctl::ptr_vector<player>::iterator;
        using const_iterator = ctl::ptr_vector<player>::const_iterator;

        bool empty() const;
        std::size_t size() const;

        iterator begin();
        iterator end();
        const_iterator begin() const;
        const_iterator end() const;

    private :
        void request_join_(const std::string& name, const color32& col, bool as_ai);
        void request_leave_();

        server_instance& serv_;
        netcom& net_;

        ctl::ptr_vector<player> players_;
        player* self_;
        bool joining_;
        bool leaving_;

        scoped_connection_pool pool_;
        shared_collection_observer<player_collection_traits> collection_;
    };
}

#endif
