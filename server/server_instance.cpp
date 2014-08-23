#include "server_instance.hpp"
#include <time.hpp>

namespace server {
    instance::instance(config::state& conf) :
        log_("server", conf), net_(conf), shutdown_timer_(4.0), plist_(net_, conf) {

        pool_ << conf.bind("netcom.debug_packets", net_.debug_packets)
              << conf.bind("admin.password", admin_password_)
              << conf.bind("shutdown.timer", shutdown_timer_);

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
            shutdown_countdown_ = shutdown_timer_;
            shutdown_ = true;
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
        return running_;
    }

    void instance::run() {
        running_ = true;
        auto sc = ctl::make_scoped([this]() { running_ = false; });

        net_.run();

        double last = now();
        bool stop = false;

        while (!stop) {
            sf::sleep(sf::milliseconds(5));

            double thetime = now();
            double dt = thetime - last;
            last = thetime;

            net_.process_packets();

            if (shutdown_) {
                shutdown_countdown_ -= dt;
                if (shutdown_countdown_ < 0) {
                    stop = true;
                }
            }

            if (!net_.is_connected()) continue;
        }
    }
}
