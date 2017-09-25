#include "work_loop.hpp"
#include <string.hpp>

work_loop::work_loop(config::state& conf, logger& log) : conf_(conf), out_(log),
    running_(false), stop_(false) {

    conf_.get_value("console.prompt", prompt_);
    conf_.get_value("netcom.auto_reconnect", auto_reconnect_);
    conf_.get_value("netcom.auto_reconnect_delay", auto_reconnect_delay_);

    open_lua_();
}

work_loop::~work_loop() {
    wait_for_shutdown();
}

void work_loop::open_lua_() {
    lua_.open_libraries(sol::lib::base, sol::lib::math);
    lua_.set_function("print", [this](std::string msg) { out_.print(msg); });

    auto stbl = lua_.create_table("server");
    stbl.set_function("connect", [this] {
        if (server_) {
            out_.error("server is already connected");
        } else {
            reconnect_now_ = true;
        }
    });
    stbl.set_function("disconnect", [this] {
        if (!server_) {
            out_.error("server is already disconnected");
        } else {
            server_->shutdown();
        }
    });
    stbl.set_function("connect_to", [this](std::string ip, std::uint16_t port) {
        if (server_) {
            out_.error("server is already connected");
        } else {
            conf_.set_value("netcom.server_ip", ip);
            conf_.set_value("netcom.server_port", port);

            reconnect_now_ = true;
        }
    });

    auto ctbl = lua_.create_table("config");
    ctbl.set_function("set", [this](std::string key, std::string value) {
        conf_.set_value(key, value);
    });
    ctbl.set_function("get", [this](std::string key) {
        sol::optional<std::string> ret;
        std::string value;
        if (!conf_.get_value(key, value)) {
            out_.error("no value exists for '", key, "'");
        } else {
            ret.set(value);
        }
        return ret;
    });
}

void work_loop::execute_(const std::string& cmd) {
    out_.print(prompt_, cmd);

    if (cmd == "exit") {
        shutdown();
    } else {
        try {
            lua_.script(cmd);
        } catch (sol::error& e) {
            out_.error(string::erase_begin(e.what(), std::string("lua: error: ").size()));
        }
    }
}

void work_loop::execute(std::string cmd) {
    cmd = string::trim(cmd);
    if (server_) {
        if (!cmd.empty()) {
            commands_.push(cmd);
        }
    } else {
        execute_(cmd);
    }
}

bool work_loop::is_running() const {
    return running_;
}

void work_loop::run() {
    running_ = true;
    worker_ = std::thread(&work_loop::run_, this);
}

void work_loop::shutdown() {
    stop_ = true;

    if (server_) {
        server_->shutdown();
    }
}

void work_loop::wait_for_shutdown() {
    shutdown();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void work_loop::connect_() {
    client::server_instance serv(conf_, out_);

    server_ = &serv;
    auto scs = ctl::make_scoped([&]() {
        server_ = nullptr;
    });

    serv.on_connecting.connect([this](std::string ip, std::uint16_t port) {
        out_.note("connecting to server (", ip, ":", port, ")...");
    });

    serv.on_connected.connect([this]()     { out_.note("connected to server");          });
    serv.on_disconnecting.connect([this]() { out_.note("disconnecting from server..."); });

    serv.on_disconnected.connect([this]() {
        out_.note("disconnected from server");
    });

    serv.on_unexpected_disconnected.connect([this]() {
        out_.note("disconnected from server unexpectedly");
        if (auto_reconnect_) {
            out_.note("will try to reconnect in ", auto_reconnect_delay_, " seconds");
        }
    });

    serv.on_connection_failed.connect([this](message::server::connection_failed::reason r) {
        out_.error("connection failed");
        std::string rsn = "?";
        switch (r) {
        case message::server::connection_failed::reason::cannot_authenticate :
            rsn = "cannot authenticate"; break;
        case message::server::connection_failed::reason::unreachable :
            rsn = "servers is unreachable"; break;
        case message::server::connection_failed::reason::disconnected :
            rsn = "disconnected"; break;
        case message::server::connection_failed::reason::timed_out :
            rsn = "timed out"; break;
        }
        out_.reason(rsn);
    });

    serv.on_iter.connect([this]() {
        std::string cmd;
        while (commands_.pop(cmd)) {
            execute_(cmd);
        }
    });

    serv.on_admin_rights_denied.connect([this](request::server::admin_rights::failure::reason r) {
        out_.error("admin rights denied");
        std::string rsn;
        switch (r) {
        case request::server::admin_rights::failure::reason::wrong_password :
            rsn = "wrong password provided";
            break;
        }
        out_.reason(rsn);
    });

    serv.on_admin_rights_granted.connect([this]() {
        out_.warning("you have admin rights on this server");
    });

    auto& net = serv.get_netcom();

    pool_ << net.watch_message([this](const message::unhandled_message& msg) {
        out_.warning("unhandled message: ", get_packet_name(msg.packet_id));
    });
    pool_ << net.watch_message([this](const message::unhandled_request& msg) {
        out_.warning("unhandled request: ", get_packet_name(msg.packet_id));
    });

    pool_ << net.watch_message([this](const message::server::connection_established& msg) {
        out_.note("connection established");
    });
    pool_ << net.watch_message([this](const message::server::connection_denied& msg) {
        out_.error("connection denied");
        std::string rsn = "?";
        switch (msg.rsn) {
        case message::server::connection_denied::reason::too_many_clients :
            rsn = "too many clients"; break;
        case message::server::connection_denied::reason::unexpected_packet :
            rsn = "unexpected packet received"; break;
        }
        out_.reason(rsn);
    });
    pool_ << net.watch_message([this](const message::server::connection_granted& msg) {
        out_.note("connection granted (id=", msg.id, ")!");
    });

    pool_ << net.watch_message([this](const message::credentials_granted& msg) {
        std::vector<std::string> creds;
        for (auto& c : msg.cred) creds.push_back(c);
        out_.note("new credentials acquired: ", string::collapse(creds, ", "));
    });

    pool_ << net.watch_message([this](const message::credentials_removed& msg) {
        std::vector<std::string> creds;
        for (auto& c : msg.cred) creds.push_back(c);
        out_.note("credentials removed: ", string::collapse(creds, ", "));
    });

    serv.on_state_left.connect([this](client::server_state::base& s) {
        s.unregister_lua(lua_);
        out_.note("leaving the '", s.name(), "' server state");
    });

    serv.on_state_entered.connect([this](client::server_state::base& s) {
        s.register_lua(lua_);
        out_.note("server is now in the '", s.name(), "' state");
    });

    pool_ << net.watch_message([this](const message::server::will_shutdown& msg) {
        out_.note("server will shutdown in less than ", msg.countdown, "sec");
    });

    serv.run();
}

void work_loop::run_() {
    auto scr = ctl::make_scoped([&]() {
        running_ = false;
    });

    double disconnected_time = 0;

    while (!stop_) {
        if (reconnect_now_ ||
            (auto_reconnect_ && (now() - disconnected_time > auto_reconnect_delay_))) {
            reconnect_now_ = false;

            connect_();

            disconnected_time = now();
        }

        sf::sleep(sf::milliseconds(50));
    }
}
