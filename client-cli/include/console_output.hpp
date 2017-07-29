#ifndef CONSOLE_OUTPUT_HPP
#define CONSOLE_OUTPUT_HPP

#include <string.hpp>
#include <log.hpp>
#include <signal.hpp>
#include <color32.hpp>
#include <lock_free_queue.hpp>
#include <array>
#include <SFML/Window/Event.hpp>

namespace sf {
    class Font;
    class RenderTarget;
}

class console_output {
    ctl::lock_free_queue<string::unicode> out_queue_;
    std::vector<string::unicode> lines_;
    std::size_t firstline_ = -1;

    mutable std::size_t line_per_page_ = 0;

    const sf::Font& font_regular_;
    const sf::Font& font_bold_;
    std::size_t charsize_, inter_line_;
    color32 color_;

    void move_page_(int delta);
    void page_up_();
    void page_down_();

public :
    console_output(const sf::Font& font_regular, const sf::Font& font_bold,
        std::size_t charsize, std::size_t inter_line, const color32& col);

    void add_line(string::unicode line);
    void poll_messages();

    void on_event(const sf::Event& e);

    void draw(sf::RenderTarget& target) const;

    signal_t<void()> on_updated;
};

class console_logger : public logger_base {
    console_output& console_;
    string::unicode buffer_;

    std::array<string::unicode,8> color_palette_;

    void print_(color::set s) override;
    void print_(color::reset_t s) override;
    void print_(color::bold_t s) override;

public :
    explicit console_logger(console_output&, const std::array<color32,8>& cp);

    void print_stamp() override;

    bool is_open() const override;
    using logger_base::print;
    void print(const std::string& s) override;
    void endl() override;
};

#endif
