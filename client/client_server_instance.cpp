#include "client_server_instance.hpp"

namespace client {
    server_instance::server_instance(config::state& conf, logger& log) :
        shutdown_(false), conf_(conf), out_(log), net_(conf_, out_) {

        pool_ << conf.bind("netcom.server_ip", server_ip_);
        pool_ << conf.bind("netcom.server_port", server_port_);
        pool_ << conf.bind("admin.password", admin_password_);

        pool_ << net_.watch_message([this](const message::server::will_shutdown&) {
            shutdown();
        });

        pool_ << net_.watch_message([this](const message::server::connection_failed& msg) {
            on_connection_failed.dispatch(msg.rsn);
        });

        on_connected.connect([this]() {
            pool_ << net_.send_request(client::netcom::server_actor_id,
                make_packet<request::server::current_state>(),
                [this](const client::netcom::request_answer_t<request::server::current_state>& msg) {
                    if (msg.failed) {
                        out_.error("could not determine current server state");
                        shutdown();
                    } else {
                        set_state_(msg.answer.state);
                    }
                }
            );

            if (!admin_password_.empty()) {
                pool_ << net_.send_request(client::netcom::server_actor_id,
                    make_packet<request::server::admin_rights>(admin_password_),
                    [this](const client::netcom::request_answer_t<request::server::admin_rights>& msg) {
                        if (msg.failed) {
                            on_admin_rights_denied.dispatch(msg.failure.rsn);
                        } else {
                            on_admin_rights_granted.dispatch();
                            is_admin_ = true;
                        }
                    }
                );
            }
        });

        pool_ << net_.watch_message([this](const message::server::changed_state& msg) {
            set_state_(msg.new_state);
        });

        // TODO
        // set_state<server_state::iddle>();

        // TODO: move the player list to the configure state
        // pool_ << plist_.on_list_received.connect([this]() {
        //     if (plist_.empty()) {
        //         out_.note("player list received (empty)");
        //     } else {
        //         out_.note("player list received:");
        //         for (const client::player& p : plist_) {
        //             out_.note(" - id=", p.id, ", ip=", p.ip, ", name=", p.name, ", color=",
        //                 p.color, ", ai=", p.is_ai);
        //         }
        //     }
        // });
        // pool_ << plist_.on_connect_fail.connect([]() {
        //     out_.error("could not read player list");
        // });
        // pool_ << plist_.on_join.connect([](client::player& p) {
        //     out_.note("joined as player \"", p.name, "\"");
        // });
        // pool_ << plist_.on_leave.connect([]() {
        //     out_.note("left player list");
        // });
        // pool_ << plist_.on_join_fail.connect([]() {
        //     out_.error("could not join as player");
        // });
        // pool_ << plist_.on_player_connected.connect([](client::player& p) {
        //     out_.note("new player connected: id=", p.id, ", ip=", p.ip, ", name=",
        //         p.name, ", color=", p.color, ", ai=", p.is_ai);
        // });
        // pool_ << plist_.on_player_disconnected.connect([](const client::player& p) {
        //     out_.note("player disconnected: id=", p.id, ", name=", p.name);
        // });
        // pool_ << plist_.on_disconnect.connect([]() {
        //     out_.note("player list was disconnected");
        // });
    }

    void server_instance::set_state_(server::state_id sid) {
        switch (sid) {
        case server::state_id::configure : {
            // setup_configure_();
            break;
        }
        case server::state_id::iddle : {
            // setup_iddle_();
            break;
        }
        case server::state_id::game : {
            // setup_game_();
            break;
        }
        }
    }

    logger& server_instance::get_log() {
        return out_;
    }

    config::state& server_instance::get_conf() {
        return conf_;
    }

    client::netcom& server_instance::get_netcom() {
        return net_;
    }

    bool server_instance::is_running() const {
        return net_.is_running();
    }

    void server_instance::shutdown() {
        shutdown_ = true;
    }

    void server_instance::run() {
        on_connecting.dispatch(server_ip_, server_port_);

        // Attempt connecting to the server
        net_.run(server_ip_, server_port_);
        while (net_.is_running() && !net_.is_connected()) {
            sf::sleep(sf::milliseconds(5));
            net_.process_packets();
        }

        if (!net_.is_running()) {
            // We failed, the signal will be triggered by the netcom
        } else {
            // We succeeded!
            on_connected.dispatch();
        }

        bool was_running = net_.is_running();
        bool asked_shutdown = false;
        while (net_.is_running()) {
            sf::sleep(sf::milliseconds(5));

            if (shutdown_) {
                on_disconnecting.dispatch();
                current_state_ = nullptr;
                net_.shutdown();
                shutdown_ = false;
                asked_shutdown = true;
            }

            net_.process_packets();

            on_iter.dispatch();
        }

        if (was_running && asked_shutdown) {
            on_disconnected.dispatch();
        } else {
            on_unexpected_disconnected.dispatch();
        }

        net_.flush_packets();
        net_.process_packets();
    }

    // void server_instance::setup_iddle_() {
    //     plist_.disconnect();
    //     auto stbl = lua_.create_table("state");
    //     stbl.set_function("start_configure", [this] {
    //         pool_ << net_.send_request(client::netcom::server_actor_id,
    //             make_packet<request::server::new_game>(),
    //             [](const client::netcom::request_answer_t<request::server::new_game>& msg) {
    //                 if (msg.failed) {
    //                     out_.error("could not switch to configure state");
    //                 } else {
    //                     setup_configure_();
    //                 }
    //             }
    //         );
    //     });
    // }

    // void server_instance::setup_configure_() {
    //     if (!plist_.is_connected()) {
    //         plist_.connect();
    //     }
    // }

    // void server_instance::setup_game_() {
    //     if (!plist_.is_connected()) {
    //         plist_.connect();
    //     }
    // }

    bool server_instance::is_admin() const {
        return is_admin_;
    }
}
