#include "console_input.hpp"
#include "console_output.hpp"
#include "sfml_wrapper.hpp"
#include "work_loop.hpp"
#include <lock_free_queue.hpp>
#include <time.hpp>
#include <log.hpp>
#include <config.hpp>
#include <string.hpp>
#include <range.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/System/Sleep.hpp>
#include <thread>
#include <iostream>

int main(int argc, const char* argv[]) {
    std::string conf_file = "client.conf";
    if (argc > 1) {
        conf_file = argv[1];
    }

    // Configure logging
    config::state conf;
    conf.parse_from_file(conf_file);

    // Configure console window
    std::size_t wx = 640, wy = 480;
    conf.get_value("console.window.width", wx);
    conf.get_value("console.window.height", wy);

    sf::RenderWindow window(sf::VideoMode(wx, wy), "cobalt console ("+conf_file+")");
    bool repaint = true;

    // Configure fonts
    std::size_t charsize = 12;
    conf.get_value("console.charsize", charsize);
    std::string fontname = "fonts/DejaVuSansMono.ttf";
    conf.get_value("console.font.regular", fontname);
    std::string bfontname = "fonts/DejaVuSansMono-Bold.ttf";
    conf.get_value("console.font.bold", bfontname);

    sf::Font font_regular;
    font_regular.loadFromFile(fontname);
    sf::Font font_bold;
    font_bold.loadFromFile(bfontname);

    // Configure font colors
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

    // Initialize logger
    logger out;
    out.add_output<file_logger>(conf, "client");
    out.add_output<cout_logger>(conf);

    // Initialize worker thread
    work_loop worker(conf, out);

    // Configure console input
    std::string prompt = "> ";
    conf.get_value("console.prompt", prompt);

    console_input edit_box(
        string::to_unicode(prompt), font_regular, charsize, console_color
    );

    edit_box.on_updated.connect([&](const string::unicode&) {
        repaint = true;
    });

    edit_box.on_text_entered.connect([&](const string::unicode& s) {
        worker.execute(string::to_utf8(s));
    });

    edit_box.on_autocompletion_query.connect([&](const string::unicode& s) {
        worker.autocompletion_query(string::to_utf8(s));
    });

    // Configure console output
    std::size_t inter_line = 3;
    conf.get_value("console.inter_line", inter_line);

    console_output message_list(
        font_regular, font_bold, charsize, inter_line, console_color
    );

    auto& clog = out.add_output<console_logger>(message_list, color_palette);
    auto scl = ctl::make_scoped([&]() {
        out.remove_output(clog);
    });

    message_list.on_updated.connect([&]() {
        repaint = true;
    });

    // Start work
    worker.run();

    // Rendering and input loop
    double refresh_delay = 1.0;
    conf.get_value("console.refresh_delay", refresh_delay);

    double prev = now();
    while (window.isOpen() && worker.is_running()) {
        double current = now();
        if (repaint || (current - prev > refresh_delay)) {
            window.clear(to_sfml(console_background));
            edit_box.draw(window);
            message_list.draw(window);
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
                    window.setView(sf::View(sf::FloatRect(0, 0, e.size.width, e.size.height)));
                    repaint = true;
                    break;
                default: break;
            }

            edit_box.on_event(e);
            message_list.on_event(e);
        }

        message_list.poll_messages();

        {
            bool found = false;
            std::vector<std::string> acq;
            while (worker.autocompletion_results.pop(acq)) {
                found = !acq.empty();
            }

            if (found) {
                if (acq.size() == 1) {
                    edit_box.autocomplete(string::to_unicode(acq[0]));
                } else {
                    out.print("autocompletion candidates:");
                    std::string common = acq[0];
                    for (std::size_t i = 0; i < acq.size(); ++i) {
                        out.print("  ", acq[i]);
                        if (i != 0) {
                            std::size_t n = std::min(common.size(), acq[i].size());
                            auto iter = std::mismatch(
                                common.begin(), common.begin()+n, acq[i].begin()
                            );

                            common.erase(iter.first, common.end());
                        }
                    }

                    if (!common.empty()) {
                        edit_box.autocomplete(string::to_unicode(common));
                    }
                }
            }
        }
    }

    worker.wait_for_shutdown();

    out.note("stopped.");

    conf.save_to_file(conf_file);

    return 0;
}
