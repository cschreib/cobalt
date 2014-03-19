#include "client_netcom.hpp"
#include <server_netcom.hpp>
#include "client_player_list.hpp"
#include <server_player_list.hpp>
#include <time.hpp>
#include <print.hpp>
#include <config.hpp>
#include <iostream>

int main(int argc, const char* argv[]) {
    std::string conf_file = "client.conf";
    if (argc > 1) {
        conf_file = argv[1];
    }

    config::state conf;
    conf.parse(conf_file);

    client::netcom net;
    conf.bind("netcom.debug_packets", net.debug_packets);

    bool stop = false;
    scoped_connection_pool pool;

    pool << net.watch_message([&](const message::unhandled_message& msg) {
        warning("unhandled message: ", msg.message_id);
    });

    pool << net.watch_message([&](const message::server::connection_failed& msg) {
        stop = true;
        error("connection failed");
        std::string rsn = "?";
        switch (msg.rsn) {
            case message::server::connection_failed::reason::cannot_authenticate :
                rsn = "cannot authenticate"; break;
            case message::server::connection_failed::reason::disconnected :
                rsn = "disconnected"; break;
            case message::server::connection_failed::reason::timed_out :
                rsn = "timed out"; break;
        }
        reason(rsn);
    });
    pool << net.watch_message([](const message::server::connection_established& msg) {
        note("connection established");
    });
    pool << net.watch_message([&](const message::server::connection_denied& msg) {
        stop = true;
        error("connection denied");
        std::string rsn = "?";
        switch (msg.rsn) {
            case message::server::connection_denied::reason::too_many_clients :
                rsn = "too many clients"; break;
            case message::server::connection_denied::reason::unexpected_packet :
                rsn = "unexpected packet received"; break;
        }
        reason(rsn);
    });
    pool << net.watch_message([&](const message::server::connection_granted& msg) {
        note("connection granted (id=", msg.id, ")!");
    });

    client::player_list plist(net);

    pool << plist.on_join.connect([](client::player& p) {
        print("joined as player \"", p.name, "\"");
    });
    pool << plist.on_leave.connect([]() {
        print("left player list");
    });
    pool << plist.on_join_fail.connect([]() {
        error("could not join as player");
    });

    std::string behavior = "watcher";
    conf.get_value("player.behavior", behavior, behavior);
    print("client behavior: \"", behavior, "\"");

    if (behavior == "player") {
        std::string player_name = "kalith";
        conf.get_value("player.name", player_name, player_name);
        color32 player_color = color32::blue;
        conf.get_value("player.color", player_color, player_color);
        plist.join_as(player_name, player_color, false);
    }

    pool << plist.on_player_connected.connect([](client::player& p) {
        print("new player connected: id=", p.id, ", ip=", p.ip, ", name=",
            p.name, ", color=", p.color, ", ai=", p.is_ai);
    });
    pool << plist.on_player_disconnected.connect([](const client::player& p) {
        print("player disconnected: id=", p.id, ", name=", p.name);
    });

    std::string server_ip = "127.0.0.1";
    conf.get_value("netcom.server_ip", server_ip, server_ip);
    std::uint16_t server_port = 4444;
    conf.get_value("netcom.serer_port", server_port, server_port);

    net.run(server_ip, server_port);
    while (!stop && !net.is_connected()) {
        sf::sleep(sf::milliseconds(5));
        net.process_packets();
    }

    double start = now();
    double last = start;
    bool end = false;
    while (!stop) {
        sf::sleep(sf::milliseconds(5));
        net.process_packets();
        if (!net.is_connected()) break;

        double n = now();
        if (behavior == "ponger" && n - last > 2) {
            last = n;
            net.send_request(client::netcom::server_actor_id, request::ping{},
            [n](const client::netcom::request_answer_t<request::ping>& msg) {
                if (msg.failed) {
                    if (msg.unhandled) {
                        error("server does not know how to pong...");
                    } else {
                        warning("failure to pong ?!");
                    }
                } else {
                    double tn = now();
                    note("pong from server (", seconds_str(tn - n), ")");
                }
            });
        }

        if (n - start > 3 && !end && behavior == "player") {
            end = true;
            plist.leave();
            pool << plist.on_leave.connect([&]() {
                net.terminate();
            });
        }
    }

    std::cout << "stopped." << std::endl;

    conf.save(conf_file);

    return 0;
}
