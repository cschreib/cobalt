#include "server_netcom.hpp"
#include "print.hpp"
#include <iostream>

int main(int argc, const char* argv[]) {
    server::netcom net;

    bool stop = false;
    server::netcom::watch_pool_t pool;

    pool << net.watch_message<message::unhandled_message>([&](message::unhandled_message msg) {
        warning("unhandled message: ", msg.message_id);
    });
    pool << net.watch_message<message::server::internal::unknown_client>(
        [&](message::server::internal::unknown_client msg) {
        warning("unknown client: ", msg.id);
    });

    pool << net.watch_message<message::server::internal::cannot_listen_port>(
        [&](message::server::internal::cannot_listen_port msg) {
        error("cannot listen to port ", msg.port);
        stop = true;
    });
    pool << net.watch_message<message::server::internal::start_listening_port>(
        [&](message::server::internal::start_listening_port msg) {
        note("now listening to port ", msg.port);
    });

    pool << net.watch_message<message::server::internal::client_connected>(
        [&](message::server::internal::client_connected msg) {
        note("new client connected (", msg.id, ") from ", msg.ip);
    });
    pool << net.watch_message<message::server::internal::client_disconnected>(
        [&](message::server::internal::client_disconnected msg) {
        note("client ", msg.id, " disconnected");
        std::string rsn = "?";
        switch (msg.rsn) {
            case message::server::internal::client_disconnected::reason::connection_lost :
                rsn = "connection lost"; break;
        }
        reason(rsn);
    });

    pool << net.watch_request<request::ping>(
        [](server::netcom::request_t<request::ping>& r, request::ping req){
        note("ping client ", r.from());
        r.answer(request::ping::answer{});
    });

    net.set_max_client(5);
    net.run(4444);

    while (!stop) {
        sf::sleep(sf::milliseconds(5));
        net.process_packets();
    }

    note("server stopped");

    return 0;
}
