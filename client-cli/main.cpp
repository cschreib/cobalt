#include "client_netcom.hpp"
#include <server_netcom.hpp>
#include "client_player_list.hpp"
#include <server_player_list.hpp>
#include <server_instance.hpp>
#include <time.hpp>
#include <log.hpp>
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
        cout.warning("unhandled message: ", msg.message_id);
    });

    pool << net.watch_message([&](const message::server::connection_failed& msg) {
        stop = true;
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
    });
    pool << net.watch_message([](const message::server::connection_established& msg) {
        cout.note("connection established");
    });
    pool << net.watch_message([&](const message::server::connection_denied& msg) {
        stop = true;
        cout.error("connection denied");
        std::string rsn = "?";
        switch (msg.rsn) {
            case message::server::connection_denied::reason::too_many_clients :
                rsn = "too many clients"; break;
            case message::server::connection_denied::reason::unexpected_packet :
                rsn = "unexpected packet received"; break;
        }
        cout.reason(rsn);
    });
    pool << net.watch_message([&](const message::server::connection_granted& msg) {
        cout.note("connection granted (id=", msg.id, ")!");
    });

    pool << net.watch_message([&](const message::credentials_granted& msg) {
        cout.note("new credentials acquired:");
        for (auto& c : msg.cred) {
            cout.note(" - ", c);
        }
    });

    pool << net.watch_message([&](const message::credentials_removed& msg) {
        cout.note("credentials removed:");
        for (auto& c : msg.cred) {
            cout.note(" - ", c);
        }
    });

    client::player_list plist(net);

    pool << plist.on_list_received.connect([&plist]() {
        if (plist.empty()) {
            cout.print("player list received (empty)");
        } else {
            cout.print("player list received:");
            for (const client::player& p : plist) {
                cout.print(" - id=", p.id, ", ip=", p.ip, ", name=", p.name, ", color=",
                    p.color, ", ai=", p.is_ai);
            }
        }
    });
    pool << plist.on_connect_fail.connect([]() {
        cout.error("could not read player list");
    });
    pool << plist.on_join.connect([](client::player& p) {
        cout.print("joined as player \"", p.name, "\"");
    });
    pool << plist.on_leave.connect([]() {
        cout.print("left player list");
    });
    pool << plist.on_join_fail.connect([]() {
        cout.error("could not join as player");
    });
    pool << plist.on_player_connected.connect([](client::player& p) {
        cout.print("new player connected: id=", p.id, ", ip=", p.ip, ", name=",
            p.name, ", color=", p.color, ", ai=", p.is_ai);
    });
    pool << plist.on_player_disconnected.connect([](const client::player& p) {
        cout.print("player disconnected: id=", p.id, ", name=", p.name);
    });

    plist.connect();

    std::string behavior = "watcher";
    conf.get_value("player.behavior", behavior, behavior);
    cout.print("client behavior: \"", behavior, "\"");

    if (behavior == "player") {
        std::string player_name = "kalith";
        conf.get_value("player.name", player_name, player_name);
        color32 player_color = color32::blue;
        conf.get_value("player.color", player_color, player_color);
        plist.join_as(player_name, player_color, false);
    }

    if (behavior == "admin") {
        std::string password = "";
        conf.get_value("admin.password", password);
        net.send_request(client::netcom::server_actor_id,
            make_packet<request::server::admin_rights>(password),
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

    std::string server_ip = "127.0.0.1";
    conf.get_value("netcom.server_ip", server_ip, server_ip);
    std::uint16_t server_port = 4444;
    conf.get_value("netcom.server_port", server_port, server_port);

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
            net.send_request(client::netcom::server_actor_id, request::server::ping{},
            [n](const client::netcom::request_answer_t<request::server::ping>& msg) {
                if (msg.failed) {
                    if (msg.unhandled) {
                        cout.error("server does not know how to pong...");
                    } else {
                        cout.warning("server failed to pong ?!");
                    }
                } else {
                    double tn = now();
                    cout.note("pong from server (", seconds_str(tn - n), ")");
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

        if (n - start > 3 && !end && behavior == "admin") {
            end = true;
            pool << net.send_request(client::netcom::server_actor_id,
                request::server::shutdown{},
                [&](const client::netcom::request_answer_t<request::server::shutdown>& msg) {
                    if (msg.failed) {
                        if (msg.missing_credentials.empty()) {
                            cout.error("server will not shutdown");
                        } else {
                            cout.error("insufficient credentials to shutdown server");
                            cout.note("missing:");
                            for (auto& c : msg.missing_credentials) {
                                cout.note(" - ", c);
                            }
                        }
                    } else {
                        cout.note("server will shutdown soon, disconnecting...");
                        stop = true;
                    }
                }
            );
        }
    }

    std::cout << "stopped." << std::endl;

    conf.save(conf_file);

    return 0;
}
