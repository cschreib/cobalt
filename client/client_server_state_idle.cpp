#include "client_server_state_idle.hpp"
#include "client_server_instance.hpp"
#include <server_state_idle.hpp>
#include <sol/sol.hpp>

namespace client {
namespace server_state {
    idle::idle(server_instance& serv) : server_state::base(serv, server::state_id::idle, "idle") {}

    void idle::start_new_game() {
        if (request_sent_) return;

        request_sent_ = true;

        pool_ << net_.send_request(client::netcom::server_actor_id,
            make_packet<request::server::new_game>(),
            [this](const client::netcom::request_answer_t<request::server::new_game>& ans) {
                request_sent_ = false;

                if (ans.failed) {
                    on_new_game_failed.dispatch();
                }
            }
        );
    }

    void idle::register_lua(sol::state& lua) {
        auto tbl = lua["server"].get_or_create<sol::table>();

        tbl.set_function("start_new_game", [this]() {
            start_new_game();
        });
    }

    void idle::unregister_lua(sol::state& lua) {
        auto tbl = lua["server"].get<sol::table>();

        tbl["start_new_game"] = sol::nil;
    }
}
}
