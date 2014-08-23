#include "server_instance.hpp"
#include "log.hpp"
#include "config.hpp"
#include <iostream>

int main(int argc, const char* argv[]) {
    config::state conf;
    conf.parse("server.conf");

    server::instance serv(conf);
    logger& out = serv.get_log();
    server::netcom& net = serv.get_netcom();

    out.note("starting server");

    bool stop = false;
    while (!stop) {
        try {
            scoped_connection_pool pool;

            pool << net.watch_message([&](const message::unhandled_message& msg) {
                out.warning("unhandled message: ", msg.message_id);
            });
            pool << net.watch_message([&](const message::server::internal::unknown_client& msg) {
                out.warning("unknown client: ", msg.id);
            });

            pool << net.watch_message([&](const message::server::internal::cannot_listen_port& msg) {
                out.error("cannot listen to port ", msg.port);
            });
            pool << net.watch_message([&](const message::server::internal::start_listening_port& msg) {
                out.note("now listening to port ", msg.port);
            });

            pool << net.watch_message([&](const message::client_connected& msg) {
                out.note("new client connected (", msg.id, ") from ", msg.ip);
            });
            pool << net.watch_message([&](const message::client_disconnected& msg) {
                out.note("client ", msg.id, " disconnected");
                std::string rsn = "?";
                switch (msg.rsn) {
                    case message::client_disconnected::reason::connection_lost :
                        rsn = "connection lost"; break;
                }
                out.reason(rsn);
            });

            pool << net.watch_request([&](server::netcom::request_t<request::server::ping>&& req){
                out.note("ping client ", req.packet.from);
                req.answer();
            });

            serv.run();

            out.note("server stopped");
            stop = true;
        } catch (std::exception& e) {
            out.error("exception caught");
            out.error(e.what());
        } catch (...) {
            out.error("unhandled exception");
        }

        if (!stop) {
            out.note("restarting server");
        }

        conf.save("server.conf");
    }

    out.note("terminating program");
    out.print("--------------------------------");

    return 0;
}
