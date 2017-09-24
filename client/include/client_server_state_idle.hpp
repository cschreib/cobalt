#ifndef CLIENT_SERVER_STATE_IDLE_HPP
#define CLIENT_SERVER_STATE_IDLE_HPP

#include "client_server_state.hpp"

namespace client {
namespace server_state {
    class idle : public server_state::base {
        scoped_connection_pool pool_;
        bool request_sent_ = false;

    public :
        explicit idle(server_instance& serv);

        void start_new_game();
        signal_t<void()> on_new_game_failed;

        void register_lua(sol::state& lua) override;
        void unregister_lua(sol::state& lua) override;
    };
}
}

#endif
