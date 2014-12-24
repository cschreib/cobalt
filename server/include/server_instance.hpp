#ifndef SERVER_INSTANCE_HPP
#define SERVER_INSTANCE_HPP

#include <config.hpp>
#include <log.hpp>
#include "server_netcom.hpp"
#include "server_state.hpp"

namespace config {
    class state;
}

namespace server {
    class instance {
        logger log_;

        netcom net_;
        scoped_connection_pool pool_;

        std::atomic<bool> shutdown_;

        std::string admin_password_;

        std::unique_ptr<server::state::base> current_state_;

    public :
        explicit instance(config::state& conf);

        logger& get_log();
        netcom& get_netcom();

        bool is_running() const;
        void run();
        void shutdown();

        template<typename T, typename ... Args>
        void set_state(Args&& ... args) {
            current_state_ = std::make_unique<T>(net_, log_, std::forward<Args>(args)...);
        }
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
