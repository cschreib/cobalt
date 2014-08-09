#include "server_netcom.hpp"
#include "server_player_list.hpp"
#include "server_instance.hpp"
#include "print.hpp"
#include "config.hpp"
#include <time.hpp>
#include <iostream>

int main(int argc, const char* argv[]) {
    config::state conf;
    conf.parse("server.conf");

    server::netcom net(conf);
    conf.bind("netcom.debug_packets", net.debug_packets);

    bool stop = false;
    scoped_connection_pool pool;

    pool << net.watch_message([&](const message::unhandled_message& msg) {
        warning("unhandled message: ", msg.message_id);
    });
    pool << net.watch_message([&](const message::server::internal::unknown_client& msg) {
        warning("unknown client: ", msg.id);
    });

    pool << net.watch_message([&](const message::server::internal::cannot_listen_port& msg) {
        error("cannot listen to port ", msg.port);
        stop = true;
    });
    pool << net.watch_message([&](const message::server::internal::start_listening_port& msg) {
        note("now listening to port ", msg.port);
    });

    pool << net.watch_message([&](const message::client_connected& msg) {
        note("new client connected (", msg.id, ") from ", msg.ip);
    });
    pool << net.watch_message([&](const message::client_disconnected& msg) {
        note("client ", msg.id, " disconnected");
        std::string rsn = "?";
        switch (msg.rsn) {
            case message::client_disconnected::reason::connection_lost :
                rsn = "connection lost"; break;
        }
        reason(rsn);
    });

    pool << net.watch_request([](server::netcom::request_t<request::server::ping>&& req){
        note("ping client ", req.packet.from);
        req.answer();
    });

    std::string admin_password;
    conf.get_value("admin.password", admin_password, admin_password);

    pool << net.watch_request(
        [&](server::netcom::request_t<request::server::admin_rights>&& req) {
        if (req.arg.password == admin_password) {
            net.grant_credentials(req.packet.from, {"admin"});
            req.answer();
        } else {
            req.fail(request::server::admin_rights::failure::reason::wrong_password);
        }
    });

    double shutdown_timer = 4;
    conf.get_value("shutdown.timer", shutdown_timer, shutdown_timer);

    bool shutdown = false;
    double shutdown_countdown = 0;
    pool << net.watch_request(
    [&](server::netcom::request_t<request::server::shutdown>&& req) {
        shutdown_countdown = shutdown_timer;
        shutdown = true;
        req.answer();
    });

    server::player_list plist(net, conf);

    net.run();

    double last = now();
    while (!stop) {
        double thetime = now();
        sf::sleep(sf::milliseconds(5));
        net.process_packets();

        if (shutdown) {
            shutdown_countdown -= thetime - last;
            if (shutdown_countdown < 0) {
                stop = true;
            }
        }

        last = thetime;
    }

    note("server stopped");

    conf.save("server.conf");

    return 0;
}
