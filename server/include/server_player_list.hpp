#ifndef SERVER_PLAYER_LIST_HPP
#define SERVER_PLAYER_LIST_HPP

#include <color32.hpp>
#include <ptr_vector.hpp>
#include <connection_handler.hpp>
#include "server_netcom.hpp"
#include "server_player.hpp"

namespace request {
namespace client {
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
    NETCOM_PACKET(list_players) {
        struct answer {
            struct player_t {
                actor_id_t id;
                std::string ip, name;
                color32 color;
                bool is_ai;
            };
            std::vector<player_t> players;
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
        bool is_ai;
    };
    NETCOM_PACKET(player_disconnected) {
        actor_id_t id;
        enum class reason {
            connection_lost,
            auto_kicked
        } rsn;
    };
}
}

namespace server {
    class player_list {
    public :
        explicit player_list(netcom& net);

        struct auto_kick_policy {
            bool ai_first = false;
            bool older_first = false;
        };

        void set_max_player(std::uint32_t max);
        void set_max_player(std::uint32_t max, auto_kick_policy p);
        std::uint32_t max_player() const;

    private :
        void remove_player_(ptr_vector<player>::iterator iter,
            message::server::player_disconnected::reason rsn);

        netcom& net_;

        std::uint32_t      max_player_;
        ptr_vector<player> players_;

        scoped_connection_pool_t pool_;
    };
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_player_list.hpp"
#endif

#endif
