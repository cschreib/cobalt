#include "server_instance.hpp"
#include "server_state_iddle.hpp"

namespace server {
    instance::instance(config::state& conf) :
        log_([&]() {
            logger log;
            log.add_output<file_logger>(conf, "server");
            log.add_output<cout_logger>(conf);
            return log;
        }()), conf_(conf), net_(conf_, log_), shutdown_(false) {

        pool_ << conf_.bind("admin.password", admin_password_);

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
            [this](server::netcom::request_t<request::server::current_state>&& req) {
            if (current_state_) {
                req.answer(current_state_->id());
            } else {
                req.fail();
            }
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::shutdown>&& req) {
            req.answer();
            shutdown();
        });

        set_state<server::state::iddle>();
    }

    logger& instance::get_log() {
        return log_;
    }

    config::state& instance::get_conf() {
        return conf_;
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
                current_state_ = nullptr;
                net_.shutdown();
                shutdown_ = false;
            }

            // Receive and send packets
            net_.process_packets();
        }
    }
}
