#include "console_input.hpp"
#include "console_output.hpp"
#include "sfml_wrapper.hpp"
#include <client_netcom.hpp>
#include <server_netcom.hpp>
#include <client_player_list.hpp>
#include <server_player_list.hpp>
#include <server_state_iddle.hpp>
#include <server_state_configure.hpp>
#include <server_instance.hpp>
#include <lock_free_queue.hpp>
#include <time.hpp>
#include <log.hpp>
#include <config.hpp>
#include <string.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Window/Event.hpp>
#include <iostream>

int main(int argc, const char* argv[]) {
    std::string conf_file = "client.conf";
    if (argc > 1) {
        conf_file = argv[1];
    }

    config::state conf;
    conf.parse_from_file(conf_file);
    cout.add_output<file_logger>(conf, "file");
    // cout.add_output<cout_logger>(conf);

    std::size_t wx = 640, wy = 480;
    conf.get_value("console.window.width", wx);
    conf.get_value("console.window.height", wy);

    sf::RenderWindow window(sf::VideoMode(wx, wy), "cobalt console ("+conf_file+")");
    bool repaint = true;

    std::size_t charsize = 12;
    conf.get_value("console.charsize", charsize);
    std::string fontname = "fonts/DejaVuSansMono.ttf";
    conf.get_value("console.font.regular", fontname);
    std::string bfontname = "fonts/DejaVuSansMono-Bold.ttf";
    conf.get_value("console.font.bold", bfontname);

    color32 console_background = color32::black;
    color32 console_color = color32::white;
    std::array<color32,8> color_palette = {{
        color32::black, color32::red, color32::green, color32(255,255,0),
        color32::blue, color32(255,0,255), color32(0,255,255), color32::white
    }};
    std::array<const char*,8> color_names = {{
        "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"
    }};
    conf.get_value("console.text_color", console_color);
    conf.get_value("console.background_color", console_background);
    for (std::size_t i : range(color_palette)) {
        conf.get_value("console.color_palette."+std::string(color_names[i]),
            color_palette[i]
        );
    }

    sf::Font font_regular;
    font_regular.loadFromFile(fontname);
    sf::Font font_bold;
    font_bold.loadFromFile(bfontname);

    std::string prompt = "> ";
    conf.get_value("console.prompt", prompt);

    ctl::lock_free_queue<std::string> commands;
    console_input tb(string::to_unicode(prompt),
        font_regular, charsize, console_color);
    tb.on_updated.connect([&](const string::unicode&) {
        repaint = true;
    });

    tb.on_text_entered.connect([&](const string::unicode& s) {
        if (!s.empty()) {
            commands.push(string::to_utf8(s));
        }
    });

    std::size_t inter_line = 3;
    conf.get_value("console.inter_line", inter_line);

    console_output st(font_regular, font_bold,
        charsize, inter_line, console_color);
    cout.add_output<console_logger>(st, color_palette);

    st.on_updated.connect([&]() {
        repaint = true;
    });

    double refresh_delay = 1.0;
    conf.get_value("console.refresh_delay", refresh_delay);

    std::atomic<bool> shutdown(false);

    std::thread worker([&]() {
        client::netcom net;
        conf.bind("netcom.debug_packets", net.debug_packets);

        scoped_connection_pool pool;

        pool << net.watch_message([&](const message::unhandled_message& msg) {
            cout.warning("unhandled message: ", get_packet_name(msg.packet_id));
        });
        pool << net.watch_message([&](const message::unhandled_request& msg) {
            cout.warning("unhandled request: ", get_packet_name(msg.packet_id));
        });

        pool << net.watch_message([&](const message::server::connection_failed& msg) {
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
            net.shutdown();
        });
        pool << net.watch_message([](const message::server::connection_established& msg) {
            cout.note("connection established");
        });
        pool << net.watch_message([&](const message::server::connection_denied& msg) {
            cout.error("connection denied");
            std::string rsn = "?";
            switch (msg.rsn) {
                case message::server::connection_denied::reason::too_many_clients :
                    rsn = "too many clients"; break;
                case message::server::connection_denied::reason::unexpected_packet :
                    rsn = "unexpected packet received"; break;
            }
            cout.reason(rsn);
            net.shutdown();
        });
        pool << net.watch_message([&](const message::server::connection_granted& msg) {
            cout.note("connection granted (id=", msg.id, ")!");
        });

        pool << net.watch_message([&](const message::credentials_granted& msg) {
            std::vector<std::string> creds;
            for (auto& c : msg.cred) creds.push_back(c);
            cout.note("new credentials acquired: ", string::collapse(creds, ", "));
        });

        pool << net.watch_message([&](const message::credentials_removed& msg) {
            std::vector<std::string> creds;
            for (auto& c : msg.cred) creds.push_back(c);
            cout.note("credentials removed: ", string::collapse(creds, ", "));
        });

        pool << net.watch_message([&](const message::server::will_shutdown& msg) {
            cout.note("server will shutdown in ", msg.countdown, "sec");
        });


        client::player_list plist(net);

        pool << plist.on_list_received.connect([&plist]() {
            if (plist.empty()) {
                cout.note("player list received (empty)");
            } else {
                cout.note("player list received:");
                for (const client::player& p : plist) {
                    cout.note(" - id=", p.id, ", ip=", p.ip, ", name=", p.name, ", color=",
                        p.color, ", ai=", p.is_ai);
                }
            }
        });
        pool << plist.on_connect_fail.connect([]() {
            cout.error("could not read player list");
        });
        pool << plist.on_join.connect([](client::player& p) {
            cout.note("joined as player \"", p.name, "\"");
        });
        pool << plist.on_leave.connect([]() {
            cout.note("left player list");
        });
        pool << plist.on_join_fail.connect([]() {
            cout.error("could not join as player");
        });
        pool << plist.on_player_connected.connect([](client::player& p) {
            cout.note("new player connected: id=", p.id, ", ip=", p.ip, ", name=",
                p.name, ", color=", p.color, ", ai=", p.is_ai);
        });
        pool << plist.on_player_disconnected.connect([](const client::player& p) {
            cout.note("player disconnected: id=", p.id, ", name=", p.name);
        });
        pool << plist.on_disconnect.connect([]() {
            cout.note("player list was disconnected");
        });

        pool << net.watch_message([&](const message::server::changed_state& msg) {
            std::string state_name;
            switch (msg.new_state) {
                case server::state_id::configure : {
                    state_name = "configure";
                    if (!plist.is_connected()) {
                        plist.connect();
                    }
                    break;
                }
                case server::state_id::iddle : {
                    state_name = "iddle";
                    plist.disconnect();
                    break;
                }
                case server::state_id::game : {
                    state_name = "game";
                    break;
                }
            }

            cout.note("server is now in the '", state_name, "' state");
        });

        std::string server_ip = "127.0.0.1";
        conf.get_value("netcom.server_ip", server_ip, server_ip);
        std::uint16_t server_port = 4444;
        conf.get_value("netcom.server_port", server_port, server_port);

        auto server_connect = [&]() {
            cout.note("connecting to server...");

            net.run(server_ip, server_port);
            while (net.is_running() && !net.is_connected()) {
                sf::sleep(sf::milliseconds(5));
                net.process_packets();
            }

            cout.note("connected to server");
        };

        server_connect();

        bool was_connected = true;
        while (true) {
            sf::sleep(sf::milliseconds(5));

            if (net.is_running()) {
                net.process_packets();
            } else if (was_connected) {
                cout.note("disconnected from server");
                was_connected = false;
            }

            std::string cmd;
            while (commands.pop(cmd)) {
                cmd = string::trim(cmd);
                cout.print(prompt, cmd);
                if (cmd == "exit") {
                    shutdown = true;
                } else if (cmd == "reconnect") {
                    if (!net.is_running()) {
                        server_connect();
                    }
                }
            }

            if (shutdown) {
                if (net.is_running()) {
                    cout.note("disconnecting from server");
                    net.shutdown();
                } else {
                    break;
                }
            }
        }
    });

    double prev = now();
    while (window.isOpen()) {
        double current = now();
        if (repaint || (current - prev > refresh_delay)) {
            window.clear(to_sfml(console_background));
            tb.draw(window);
            st.draw(window);
            window.display();
            repaint = false;
            prev = current;
        }

        sf::sleep(sf::milliseconds(5));

        sf::Event e;
        while (window.pollEvent(e)) {
            switch (e.type) {
                case sf::Event::Closed:
                    window.close();
                    break;
                case sf::Event::Resized:
                    window.setView(sf::View(sf::FloatRect(
                        0, 0, e.size.width, e.size.height
                    )));
                    repaint = true;
                    break;
                default: break;
            }

            tb.on_event(e);
        }

        st.poll_messages();

        if (shutdown) {
            break;
        }
    }

    shutdown = true;

    if (worker.joinable()) worker.join();

    cout.note("stopped.");
    conf.save_to_file(conf_file);

    return 0;
}
