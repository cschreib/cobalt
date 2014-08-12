#ifndef SERVER_INSTANCE_HPP
#define SERVER_INSTANCE_HPP

#include <config.hpp>
#include "server_netcom.hpp"

namespace config {
    class state;
}

namespace server {
    class instance {
        server::netcom net_;
        scoped_connection_pool pool_;

        std::string admin_password_;

    public :
        explicit instance(config::state& conf);
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
