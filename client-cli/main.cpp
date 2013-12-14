#include "client_netcom.hpp"
#include "server_netcom.hpp"
#include "time.hpp"
#include "print.hpp"
#include <iostream>

int main(int argc, const char* argv[]) {
    client::netcom net;

    bool stop = false;
    client::netcom::watch_pool_t pool;

    pool << net.watch_message<message::unhandled_message>([&](message::unhandled_message msg) {
        warning("unhandled message: ", msg.message_id);
    });

    pool << net.watch_message<message::server::connection_failed>([&](
        message::server::connection_failed msg) {
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
    pool << net.watch_message<message::server::connection_established>(
        [](message::server::connection_established msg) {
        note("connection established");
    });
    pool << net.watch_message<message::server::connection_denied>([&]
        (message::server::connection_denied msg) {
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
    pool << net.watch_message<message::server::connection_granted>(
        [](message::server::connection_granted msg) {
        note("connection granted!");
    });

    net.run("127.0.0.1", 4444);

    double start = now();
    while (!stop) {
        sf::sleep(sf::milliseconds(5));
        net.process_packets();
        double n = now();
        if (n - start > 2) {
            start = n;
            net.send_request(client::netcom::server_actor_id, request::ping{},
            [n](request::ping::answer){
                double tn = now();
                note("pong from server (", seconds_str(tn - n), ")");
            }, [](request::ping::failure){
                warning("failure to pong ?!");
            }, [](){
                error("server does not know how to pong...");
            });
        }
    }

    std::cout << "stopped." << std::endl;

    return 0;
}
