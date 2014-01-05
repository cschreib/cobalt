#include "server_netcom.hpp"
#include "server_player_list.hpp"
#include "print.hpp"
#include "config.hpp"
#include <iostream>

int main(int argc, const char* argv[]) {
    config::state conf;
    conf.parse("server.conf");

    server::netcom net(conf);

    bool stop = false;
    scoped_connection_pool pool;

    pool << net.watch_message([&](message::unhandled_message msg) {
        warning("unhandled message: ", msg.message_id);
    });
    pool << net.watch_message([&](message::server::internal::unknown_client msg) {
        warning("unknown client: ", msg.id);
    });

    pool << net.watch_message([&](message::server::internal::cannot_listen_port msg) {
        error("cannot listen to port ", msg.port);
        stop = true;
    });
    pool << net.watch_message([&](message::server::internal::start_listening_port msg) {
        note("now listening to port ", msg.port);
    });

    pool << net.watch_message([&](message::server::internal::client_connected msg) {
        note("new client connected (", msg.id, ") from ", msg.ip);
    });
    pool << net.watch_message([&](message::server::internal::client_disconnected msg) {
        note("client ", msg.id, " disconnected");
        std::string rsn = "?";
        switch (msg.rsn) {
            case message::server::internal::client_disconnected::reason::connection_lost :
                rsn = "connection lost"; break;
        }
        reason(rsn);
    });

    pool << net.watch_request([](server::netcom::request_t<request::ping>&& req){
        note("ping client ", req.from);
        req.answer();
    });

    server::player_list plist(net, conf);

    net.run();

    while (!stop) {
        sf::sleep(sf::milliseconds(5));
        net.process_packets();
    }

    note("server stopped");

    conf.save("server.conf");

    return 0;
}
