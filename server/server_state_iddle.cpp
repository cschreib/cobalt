#include "server_state_iddle.hpp"
#include "server_state_configure.hpp"
#include "server_instance.hpp"

namespace server {
namespace state {
    iddle::iddle(server::instance& serv) : base_impl(serv, "iddle") {
        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::new_game>&& req) {
                serv_.set_state<state::configure>();
                req.answer();
            }
        );
    }
}
}
