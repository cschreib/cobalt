#include "server_netcom.hpp"
#include "print.hpp"
#include <iostream>

int main(int argc, const char* argv[]) {
    server::netcom net;

    bool stop = false;
    server::netcom::watch_pool_t pool;

    pool << net.watch_message<message::unhandled_message>([&](message_packet_id_t id) {
        warning("unhandled message: ", id);
    });
    pool << net.watch_message<message::server::internal::unknown_client>([&](actor_id_t id) {
        warning("unknown client: ", id);
    });

    pool << net.watch_message<message::server::internal::cannot_listen_port>([&](std::uint16_t port) {
        error("cannot listen to port ", port);
        stop = true;
    });
    pool << net.watch_message<message::server::internal::start_listening_port>([&](std::uint16_t port) {
        note("now listening to port ", port);
    });

    pool << net.watch_message<message::server::internal::client_connected>([&](
        actor_id_t id, std::string ip) {
        note("new client connected (", id, ") from ", ip);
    });
    pool << net.watch_message<message::server::internal::client_disconnected>([&](actor_id_t id, 
        message::server::internal::client_disconnected::reason r) {
        note("client ", id, " disconnected");
        std::string rsn = "?";
        switch (r) {
            case message::server::internal::client_disconnected::reason::connection_lost :
                rsn = "connection lost"; break;
        }
        reason(rsn);
    });

    pool << net.watch_request<request::ping>([](server::netcom::request_t<request::ping>& r){
        note("ping client ", r.from());
        r.answer();
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
