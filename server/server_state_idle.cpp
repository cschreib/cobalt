#include "server_state_idle.hpp"
#include "server_state_configure.hpp"
#include "server_instance.hpp"

namespace server {
namespace state {
    idle::idle(server::instance& serv) : base(serv, server::state_id::idle, "idle") {
        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::new_game>&& req) {
                serv_.set_state<state::configure>();
                req.answer();
            }
        );
    }
}
}
