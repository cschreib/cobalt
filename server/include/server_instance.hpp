#ifndef SERVER_INSTANCE_HPP
#define SERVER_INSTANCE_HPP

#include <config.hpp>
#include <log.hpp>
#include "server_netcom.hpp"
#include "server_player_list.hpp"

namespace config {
    class state;
}

namespace server {
    class instance {
        logger log_;

        netcom net_;
        scoped_connection_pool pool_;

        bool   running_ = false;
        bool   shutdown_ = false;
        double shutdown_timer_;
        double shutdown_countdown_ = 0.0;

        std::string admin_password_;

        player_list plist_;

    public :
        explicit instance(config::state& conf);

        logger& get_log();
        netcom& get_netcom();

        bool is_running() const;
        void run();
    };
}

namespace request {
namespace server {
    NETCOM_PACKET(admin_rights) {
        std::string password;

        struct answer {};
        struct failure {
            enum class reason {
                wrong_password
            } rsn;
        };
    };

    NETCOM_PACKET(shutdown) {
        NETCOM_REQUIRES("admin");

        struct answer {};
        struct failure {};
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_instance.hpp"
#endif

#endif
