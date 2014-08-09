#include "server_instance.hpp"

namespace server {
    instance::instance(config::state& conf) : net_(conf) {
        pool_ << conf.bind("admin.password", admin_password_);

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::admin_rights>&& req) {
            if (req.arg.password == admin_password_) {
                net_.grant_credentials(req.packet.from, {"admin"});
                req.answer();
            } else {
                req.fail(request::server::admin_rights::failure::reason::wrong_password);
            }
        });
    }
}
