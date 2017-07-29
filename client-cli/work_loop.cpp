#include "work_loop.hpp"
#include <client_netcom.hpp>
#include <server_netcom.hpp>
#include <client_player_list.hpp>
#include <server_player_list.hpp>
#include <server_state_iddle.hpp>
#include <server_state_configure.hpp>
#include <server_instance.hpp>
#include <string.hpp>

work_loop::work_loop(config::state& conf) : plist_(net_), shutdown_(false) {
    conf.bind("netcom.debug_packets", net_.debug_packets);

    pool_ << net_.watch_message([this](const message::unhandled_message& msg) {
        cout.warning("unhandled message: ", get_packet_name(msg.packet_id));
    });
    pool_ << net_.watch_message([this](const message::unhandled_request& msg) {
        cout.warning("unhandled request: ", get_packet_name(msg.packet_id));
    });

    pool_ << net_.watch_message([this](const message::server::connection_failed& msg) {
        cout.error("connection failed");
        std::string rsn = "?";
        switch (msg.rsn) {
            case message::server::connection_failed::reason::cannot_authenticate :
                rsn = "cannot authenticate"; break;
            case message::server::connection_failed::reason::disconnected :
                rsn = "disconnected"; break;
            case message::server::connection_failed::reason::timed_out :
                rsn = "timed out"; break;
        }
        cout.reason(rsn);
        net_.shutdown();
    });
    pool_ << net_.watch_message([](const message::server::connection_established& msg) {
        cout.note("connection established");
    });
    pool_ << net_.watch_message([this](const message::server::connection_denied& msg) {
        cout.error("connection denied");
        std::string rsn = "?";
        switch (msg.rsn) {
            case message::server::connection_denied::reason::too_many_clients :
                rsn = "too many clients"; break;
            case message::server::connection_denied::reason::unexpected_packet :
                rsn = "unexpected packet received"; break;
        }
        cout.reason(rsn);
        net_.shutdown();
    });
    pool_ << net_.watch_message([this](const message::server::connection_granted& msg) {
        cout.note("connection granted (id=", msg.id, ")!");
    });

    pool_ << net_.watch_message([this](const message::credentials_granted& msg) {
        std::vector<std::string> creds;
        for (auto& c : msg.cred) creds.push_back(c);
        cout.note("new credentials acquired: ", string::collapse(creds, ", "));
    });

    pool_ << net_.watch_message([this](const message::credentials_removed& msg) {
        std::vector<std::string> creds;
        for (auto& c : msg.cred) creds.push_back(c);
        cout.note("credentials removed: ", string::collapse(creds, ", "));
    });

    pool_ << net_.watch_message([this](const message::server::will_shutdown& msg) {
        cout.note("server will shutdown in ", msg.countdown, "sec");
    });

    pool_ << plist_.on_list_received.connect([this]() {
        if (plist_.empty()) {
            cout.note("player list received (empty)");
        } else {
            cout.note("player list received:");
            for (const client::player& p : plist_) {
                cout.note(" - id=", p.id, ", ip=", p.ip, ", name=", p.name, ", color=",
                    p.color, ", ai=", p.is_ai);
            }
        }
    });
    pool_ << plist_.on_connect_fail.connect([]() {
        cout.error("could not read player list");
    });
    pool_ << plist_.on_join.connect([](client::player& p) {
        cout.note("joined as player \"", p.name, "\"");
    });
    pool_ << plist_.on_leave.connect([]() {
        cout.note("left player list");
    });
    pool_ << plist_.on_join_fail.connect([]() {
        cout.error("could not join as player");
    });
    pool_ << plist_.on_player_connected.connect([](client::player& p) {
        cout.note("new player connected: id=", p.id, ", ip=", p.ip, ", name=",
            p.name, ", color=", p.color, ", ai=", p.is_ai);
    });
    pool_ << plist_.on_player_disconnected.connect([](const client::player& p) {
        cout.note("player disconnected: id=", p.id, ", name=", p.name);
    });
    pool_ << plist_.on_disconnect.connect([]() {
        cout.note("player list was disconnected");
    });

    pool_ << net_.watch_message([this](const message::server::changed_state& msg) {
        std::string state_name;
        switch (msg.new_state) {
            case server::state_id::configure : {
                state_name = "configure";
                if (!plist_.is_connected()) {
                    plist_.connect();
                }
                break;
            }
            case server::state_id::iddle : {
                state_name = "iddle";
                plist_.disconnect();
                break;
            }
            case server::state_id::game : {
                state_name = "game";
                break;
            }
        }

        cout.note("server is now in the '", state_name, "' state");
    });

    conf.get_value("console.prompt_", prompt_, prompt_);
    conf.get_value("netcom.server_ip", server_ip_, server_ip_);
    conf.get_value("netcom.server_port", server_port_, server_port_);

    if (conf.value_exists("admin.password")) {
        conf.get_value("admin.password", admin_password_);
    }

    open_lua_();
}

work_loop::~work_loop() {}

void work_loop::open_lua_() {
    lua_.open_libraries(sol::lib::base, sol::lib::math);
    lua_.set_function("print", [](std::string msg) { cout.print(msg); });
}

void work_loop::server_connect_() {
    if (net_.is_running()) {
        cout.warning("already connected to server");
        return;
    }

    cout.note("connecting to server...");

    net_.run(server_ip_, server_port_);
    while (net_.is_running() && !net_.is_connected()) {
        sf::sleep(sf::milliseconds(5));
        net_.process_packets();
    }

    cout.note("connected to server");

    net_.send_request(client::netcom::server_actor_id,
        make_packet<request::server::current_state>(),
        [](const client::netcom::request_answer_t<request::server::current_state>& msg) {
            if (msg.failed) {
                cout.error("could not determine current server state");
            } else {
                std::string state = "unknown";
                switch (msg.answer.state) {
                    case server::state_id::iddle : state = "iddle"; break;
                    case server::state_id::configure : state = "configure"; break;
                    case server::state_id::game : state = "game"; break;
                    default: break;
                }
                cout.note("server is in the "+state+" state");
            }
        }
    );

    if (!admin_password_.empty()) {
        net_.send_request(client::netcom::server_actor_id,
            make_packet<request::server::admin_rights>(admin_password_),
            [](const client::netcom::request_answer_t<request::server::admin_rights>& msg) {
                if (msg.failed) {
                    cout.error("admin rights denied");
                    std::string rsn;
                    switch (msg.failure.rsn) {
                        case request::server::admin_rights::failure::reason::wrong_password :
                            rsn = "wrong password provided";
                            break;
                    }
                    cout.reason(rsn);
                }
            }
        );
    }
}

void work_loop::run_() {
    server_connect_();

    bool was_connected = true;
    while (true) {
        sf::sleep(sf::milliseconds(5));

        if (net_.is_running()) {
            net_.process_packets();
        } else if (was_connected) {
            cout.note("disconnected from server");
            was_connected = false;
        }

        std::string cmd;
        while (commands_.pop(cmd)) {
            cout.print(prompt_, cmd);
            execute_(cmd);
        }

        if (shutdown_) {
            if (net_.is_running()) {
                cout.note("disconnecting from server");
                net_.shutdown();
            } else {
                break;
            }
        }
    }
}

void work_loop::execute_(const std::string& cmd) {
    if (cmd == "exit") {
        shutdown_ = true;
    } else {
        try {
            lua_.script(cmd);
        } catch (sol::error& e) {
            cout.error(string::erase_begin(e.what(), std::string("lua: error: ").size()));
        }
    }
}

void work_loop::start() {
    worker_ = std::thread(&work_loop::run_, this);
}

void work_loop::execute(std::string cmd) {
    cmd = string::trim(cmd);
    if (!cmd.empty()) {
        commands_.push(cmd);
    }
}

bool work_loop::is_stopped() const {
    return shutdown_.load();
}

void work_loop::stop() {
    shutdown_ = true;
    if (worker_.joinable()) worker_.join();
}
