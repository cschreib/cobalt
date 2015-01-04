#ifndef SERVER_STATE_HPP
#define SERVER_STATE_HPP

#include "server_netcom.hpp"

namespace server {
    class instance;

    enum class state_id : std::uint8_t {
        iddle,
        configure,
        game,
        test
    };

    namespace state {
        class base {
            const std::string name_;

        protected :
            server::instance& serv_;
            netcom& net_;
            logger& out_;

            base(server::instance& serv, std::string name);

        public :
            virtual ~base() = default;

            const std::string& name() const;
        };

        template<state_id ID>
        class base_impl : public base {
        protected :
            base_impl(server::instance& serv, std::string name) :
                base(serv, name) {}

        public :
            virtual ~base_impl() = default;

            static const server::state_id id = ID;
        };

        template<state_id ID>
        const server::state_id base_impl<ID>::id;
    }
}

#endif
