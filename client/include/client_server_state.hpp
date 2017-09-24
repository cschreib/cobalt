#ifndef CLIENT_SERVER_STATE_HPP
#define CLIENT_SERVER_STATE_HPP

#include "client_netcom.hpp"
#include <server_state.hpp>

namespace client {
    class server_instance;

    namespace server_state {
        class base {
            const server::state_id id_;
            const std::string name_;

        protected :
            server_instance& serv_;
            netcom& net_;
            logger& out_;

            base(server_instance& serv, server::state_id id, std::string name);

        public :
            virtual ~base() = default;

            const std::string& name() const;
            server::state_id id() const;
        };
    }
}

#endif
