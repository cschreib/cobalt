#include <server_instance.hpp>
#include <server_state_configure.hpp>
#include <server_state_game.hpp>
#include <log.hpp>
#include <config.hpp>
#include <signal.h>

server::instance* gserv = nullptr;

#ifdef POSIX
void sigint_handler(int) {
    gserv->shutdown();
}
#endif

int main(int argc, const char* argv[]) {
    std::string conf_file = "server.conf";
    if (argc > 1) {
        conf_file = argv[1];
    }

    bool stop = false;
    while (!stop) {
        config::state conf;
        conf.parse_from_file(conf_file);
        auto& clog = cout.add_output<cout_logger>(conf);
        auto scl = ctl::make_scoped([&]() {
            cout.remove_output(clog);
        });

        logger out;
        out.add_output<file_logger>(conf, "server");
        out.add_output<cout_logger>(conf);

        out.note("read configuration");

        server::instance serv(conf, out);
        gserv = &serv;

        server::netcom& net = serv.get_netcom();

        #ifdef POSIX
        struct sigaction act;
        act.sa_handler = sigint_handler;
        sigaction(SIGINT, &act, nullptr);

        auto sc = ctl::make_scoped([]() {
            sigaction(SIGINT, nullptr, nullptr);
        });
        #endif

        out.note("starting server");

        try {
            scoped_connection_pool pool;

            pool << net.watch_message([&](const message::unhandled_message& msg) {
                out.warning("unhandled message: ", get_packet_name(msg.packet_id));
            });
            pool << net.watch_message([&](const message::unhandled_request& msg) {
                out.warning("unhandled request: ", get_packet_name(msg.packet_id));
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

            pool << net.watch_message([&](const message::server::changed_state& msg) {
                std::string state = "unknown";
                switch (msg.new_state) {
                    case server::state_id::idle :
                        state = "idle";
                        break;
                    case server::state_id::configure :
                        state = "configure";
                        break;
                    case server::state_id::game :
                        state = "game";
                        break;
                    default: break;
                }
                out.note("state changed to '", state,"'");
            });

            pool << net.watch_message([&](const message::server::configure_generating& msg) {
                out.note("begin generating new universe...");
            });

            pool << net.watch_message([&](const message::server::configure_generated& msg) {
                if (msg.failed) {
                    out.error("generation of universe failed");
                    out.reason(msg.reason);
                } else {
                    out.note("universe generated successfuly");
                }
            });

            pool << net.watch_message([&](const message::server::configure_loading& msg) {
                out.note("loading universe...");
            });

            pool << net.watch_message([&](const message::server::configure_loaded& msg) {
                if (msg.failed) {
                    out.error("loading of universe failed");
                    out.reason(msg.reason);
                } else {
                    out.note("universe loaded successfuly");
                }
            });

            pool << net.watch_message([&](const message::server::game_load_progress& msg) {
                out.note("loading: ", msg.current_step, "/", msg.num_steps,
                    " (", msg.current_step_name, ")");
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

        conf.save_to_file(conf_file);

        if (!stop) {
            out.note("restarting server in one second...");
            sf::sleep(sf::seconds(1));
        } else {
            out.note("terminating program");
            out.print("--------------------------------");
        }

        gserv = nullptr;
    }

    return 0;
}
