#ifndef CLIENT_SERVER_STATE_CONFIGURE_HPP
#define CLIENT_SERVER_STATE_CONFIGURE_HPP

#include "client_server_state.hpp"
#include "client_player_list.hpp"
#include <server_state_configure.hpp>
#include <config_shared_state.hpp>

namespace client {
namespace server_state {
    class game;

    class configure : public server_state::base {
        using generator_list_t = server::state::configure::generator_list_t;

        config::shared_state_observer config_;
        config::shared_state_observer generator_config_;
        scoped_connection_pool pool_;

        std::unique_ptr<client::player_list> plist_;
        generator_list_t available_generators_;
        std::string generator_;

        void transition_to_(server_state::base& st) override;

    public :
        explicit configure(server_instance& serv);

        std::string get_current_generator() const;

        void register_lua(sol::state& lua) override;
        void unregister_lua(sol::state& lua) override;
    };
}
}

#endif
