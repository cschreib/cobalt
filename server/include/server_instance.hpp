#ifndef SERVER_INSTANCE_HPP
#define SERVER_INSTANCE_HPP

#include <log.hpp>
#include "server_netcom.hpp"
#include "server_state.hpp"

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

    NETCOM_PACKET(current_state) {
        struct answer {
            ::server::state_id state;
        };
        struct failure {};
    };

    NETCOM_PACKET(shutdown) {
        NETCOM_REQUIRES("admin");

        struct answer {};
        struct failure {};
    };

    NETCOM_PACKET(stop_and_idle) {
        struct answer {};
        struct failure {};
    };
}
}

namespace message {
namespace server {
    NETCOM_PACKET(changed_state) {
        ::server::state_id new_state;
    };
}
}

namespace config {
    class state;
}

namespace server {
    class instance {
        logger& log_;

        config::state& conf_;
        netcom net_;
        scoped_connection_pool pool_;

        std::atomic<bool> shutdown_;

        std::string admin_password_;

        std::unique_ptr<server::state::base> current_state_;

    public :
        explicit instance(config::state& conf, logger& log);

        logger& get_log();
        netcom& get_netcom();
        config::state& get_conf();

        bool is_running() const;
        void run();
        void shutdown();

        template<typename T, typename ... Args>
        T& set_state(Args&& ... args) {
            return set_state(std::make_unique<T>(*this, std::forward<Args>(args)...));
        }

        template<typename T>
        T& set_state(std::unique_ptr<T> st) {
            if (current_state_ != nullptr) {
                net_.send_message(netcom::all_actor_id,
                    make_packet<message::server::changed_state>(st->id()));
            }

            T& ret = *st;
            current_state_ = std::move(st);
            return ret;
        }
    };
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_instance.hpp"
#endif

#endif
