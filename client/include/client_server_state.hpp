#ifndef CLIENT_SERVER_STATE_HPP
#define CLIENT_SERVER_STATE_HPP

#include "client_netcom.hpp"
#include <server_state.hpp>

namespace sol {
    class state;
}

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

            virtual void transition_to_(server_state::base& st);
            virtual void end_of_transition_();

        public :
            virtual ~base();

            const std::string& name() const;
            server::state_id id() const;

            void transition_to(server_state::base& st);

            virtual void register_lua(sol::state& lua);
            virtual void unregister_lua(sol::state& lua);
        };
    }
}

#endif
