#ifndef SERVER_STATE_HPP
#define SERVER_STATE_HPP

#include "server_netcom.hpp"

namespace server {
    class instance;

    enum class state_id : std::uint8_t {
        idle,
        configure,
        game
    };

    namespace state {
        class base {
            const server::state_id id_;
            const std::string name_;

        protected :
            server::instance& serv_;
            netcom& net_;
            logger& out_;

            base(server::instance& serv, server::state_id id, std::string name);

        public :
            virtual ~base() = default;

            const std::string& name() const;
            state_id id() const;

            virtual void register_callbacks() {}
        };
    }
}

#endif
