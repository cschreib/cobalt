#ifndef SERVER_PLAYER_LIST_HPP
#define SERVER_PLAYER_LIST_HPP

#include <color32.hpp>
#include <ptr_vector.hpp>
#include <scoped_connection_pool.hpp>
#include <shared_collection.hpp>
#include "server_netcom.hpp"
#include "server_player.hpp"

namespace config {
    class state;
}

namespace request {
namespace client {
    NETCOM_PACKET(player_list_collection_id) {
        struct answer {
            shared_collection_id_t id;
        };
        struct failure {};
    };
    NETCOM_PACKET(leave_players) {
        struct answer {};
        struct failure {};
    };
    NETCOM_PACKET(join_players) {
        std::string name;
        color32 color;
        bool is_ai;

        struct answer {};
        struct failure {
            enum class reason {
                too_many_players = 0
            } rsn;
        };
    };
}
}

namespace packet {
    NETCOM_PACKET(player_list) {
        struct player_t {
            actor_id_t id;
            std::string ip, name;
            color32 color;
            bool is_ai;
        };
        std::vector<player_t> players;
    };
    NETCOM_PACKET(player_connected) {
        actor_id_t id;
        std::string ip, name;
        color32 color;
        bool is_ai;
    };
    NETCOM_PACKET(player_disconnected) {
        actor_id_t id;
        enum class reason {
            connection_lost,
            left,
            auto_kicked
        } rsn;
    };
}

struct player_collection_traits {
    using full_packet   = packet::player_list;
    using add_packet    = packet::player_connected;
    using remove_packet = packet::player_disconnected;
};

namespace server {
    class player_list {
    public :
        player_list(netcom& net, config::state& conf);

        struct auto_kick_policy {
            bool ai_first = false;
            bool older_first = false;
        };

        void set_max_player(std::uint32_t max);
        void set_max_player(std::uint32_t max, auto_kick_policy p);
        std::uint32_t max_player() const;

    private :
        void remove_player_(ctl::ptr_vector<player>::iterator iter,
            packet::player_disconnected::reason rsn);

        netcom& net_;
        config::state& conf_;

        std::uint32_t           max_player_;
        ctl::ptr_vector<player> players_;

        scoped_connection_pool pool_;
        shared_collection<player_collection_traits> collection_;
    };
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_player_list.hpp"
#endif

#endif
