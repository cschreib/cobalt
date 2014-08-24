#include "server_instance.hpp"

namespace server {
    instance::instance(config::state& conf) :
        log_("server", conf), net_(conf, log_), shutdown_(false) {

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

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::shutdown>&& req) {
            shutdown();
            req.answer();
        });
    }

    logger& instance::get_log() {
        return log_;
    }

    server::netcom& instance::get_netcom() {
        return net_;
    }

    bool instance::is_running() const {
        return net_.is_running();
    }

    void instance::shutdown() {
        shutdown_ = true;
    }

    void instance::run() {
        net_.run();
        while (net_.is_running()) {
            sf::sleep(sf::milliseconds(5));

            if (shutdown_) {
                net_.shutdown();
                shutdown_ = false;
            }

            // Receive and send packets
            net_.process_packets();
            if (!net_.is_connected()) continue;

            // Update server logic
        }
    }
}
