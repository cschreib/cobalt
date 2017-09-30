#ifndef CONSOLE_INPUT_HPP
#define CONSOLE_INPUT_HPP

#include <string.hpp>
#include <signal.hpp>
#include <color32.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

namespace sf {
    class Event;
    class Font;
}

class console_input {
    std::size_t     charsize_;
    string::unicode prompt_;
    string::unicode content_;
    std::size_t     pos_ = 0;
    std::size_t     spos_ = 0;

    std::vector<string::unicode> history_;
    string::unicode saved_content_;
    std::size_t hpos_ = 0;

    mutable sf::Text           text_;
    mutable sf::RectangleShape carret_;
    mutable sf::RectangleShape highlight_;

    bool selecting_ = false;
    bool selected_ = false;

    void history_to_present_();
    void erase_selection_();
    void clear_selection_();

public :
    explicit console_input(const string::unicode& prompt, const sf::Font& f,
        std::size_t cs, const color32& col);

    void recall_previous();
    void recall_next();

    void erase_before();
    void erase_after();

    void insert(string::unicode_char c);
    void clear();
    std::size_t size() const;

    void move_backward();
    void move_forward();
    void move_first();
    void move_last();

    void ask_autocomplete();
    void autocomplete(string::unicode text);

    string::unicode content() const;

    void on_event(const sf::Event& e);

    signal_t<void(const string::unicode&)> on_updated;
    signal_t<void(const string::unicode&)> on_text_entered;
    signal_t<void(const string::unicode&)> on_autocompletion_query;

    void draw(sf::RenderTarget& target) const;
};

#endif
