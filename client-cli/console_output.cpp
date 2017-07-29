#include "console_output.hpp"
#include "sfml_wrapper.hpp"
#include <range.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

console_output::console_output(const sf::Font& font_regular, const sf::Font& font_bold,
    std::size_t charsize, std::size_t inter_line, const color32& col) :
    font_regular_(font_regular), font_bold_(font_bold),
    charsize_(charsize), inter_line_(inter_line), color_(col) {}

void console_output::add_line(string::unicode line) {
    out_queue_.push(line);
}

void console_output::poll_messages() {
    bool updated = false;
    string::unicode line;
    while (out_queue_.pop(line)) {
        lines_.push_back(line);
        updated = true;
    }

    if (updated) {
        on_updated.dispatch();
    }
}

void console_output::move_page_(int delta) {
    if (delta > 0) {
        if (lines_.size() <= line_per_page_) {
            firstline_ = 0;
        } else {
            if (firstline_ == std::size_t(-1)) {
                firstline_ = lines_.size() - line_per_page_;
            }

            if (firstline_ > std::size_t(delta)) {
                firstline_ -= delta;
            } else {
                firstline_ = 0;
            }
        }
    } else {
        if (lines_.size() <= line_per_page_) {
            firstline_ = -1;
        } else {
            if (firstline_ != std::size_t(-1)) {
                firstline_ += std::size_t(-delta);
                if (firstline_ > lines_.size()-line_per_page_) {
                    firstline_ = -1;
                }
            }
        }
    }

    on_updated.dispatch();
}

void console_output::page_up_() {
    move_page_(line_per_page_/2);
}

void console_output::page_down_() {
    move_page_(-int(line_per_page_)/2);
}

void console_output::on_event(const sf::Event& e) {
    switch (e.type) {
    case sf::Event::KeyPressed:
        switch (e.key.code) {
            case sf::Keyboard::Key::PageUp:
                page_up_();
                break;
            case sf::Keyboard::Key::PageDown:
                page_down_();
                break;
            default: break;
        }
        break;
    case sf::Event::MouseWheelMoved:
        move_page_(e.mouseWheel.delta);
        break;
    default: break;
    }
}

void console_output::draw(sf::RenderTarget& target) const {
    std::vector<std::vector<sf::Text>> lines;

    auto current_text = [&]() -> sf::Text& {
        return lines.back().back();
    };

    auto new_text = [&]() {
        std::size_t nt = lines.back().size();
        if (nt != 0 && lines.back().back().getString().isEmpty()) return;

        lines.back().push_back(sf::Text("", font_regular_, charsize_));
        auto& txt = current_text();
        txt.setColor(to_sfml(color_));

        if (nt != 0) {
            auto& prev = *(lines.back().end()-2);
            auto bounds = prev.findCharacterPos(prev.getString().getSize());
            txt.setPosition(bounds.x, 2);
        } else {
            txt.setPosition(0, 2);
        }
    };

    auto new_line = [&]() {
        lines.push_back(std::vector<sf::Text>());
        new_text();
    };

    auto add_char = [&](string::unicode_char c) {
        auto& txt = current_text();
        string::unicode s = txt.getString().getData();
        s += c;
        txt.setString(s);

        if (txt.findCharacterPos(s.size()).x > target.getSize().x) {
            std::size_t p = s.size();
            std::size_t gp = s.size()-1;
            while (p != 0) {
                if (s[p-1] == string::to_unicode(' ') ||
                    s[p-1] == string::to_unicode('\t')) {
                    gp = p;
                    break;
                }

                --p;
            }

            string::unicode ns = s.substr(gp);
            s.erase(gp);
            txt.setString(s);
            new_line();

            auto& ntxt = current_text();
            ntxt.setString(ns);
            ntxt.setColor(txt.getColor());
            ntxt.setFont(*txt.getFont());
        }
    };

    for (auto& line : lines_) {
        new_line();

        bool escape = false;

        bool coloring = false;
        sf::Color col;
        std::size_t color_char = 0;
        std::string tmp_col;

        for (auto c : line) {
            if (coloring) {
                tmp_col += string::to_utf8(c);
                if (color_char % 2 != 0) {
                    if (color_char == 1) {
                        col.a = string::hex_to_uchar(tmp_col);
                    } else if (color_char == 3) {
                        col.r = string::hex_to_uchar(tmp_col);
                    } else if (color_char == 5) {
                        col.g = string::hex_to_uchar(tmp_col);
                    } else if (color_char == 7) {
                        col.b = string::hex_to_uchar(tmp_col);
                    }
                    tmp_col.clear();
                }

                ++color_char;
                if (color_char == 8) {
                    new_text();
                    current_text().setColor(col);
                    coloring = false;
                }
            } else if (c == string::to_unicode('|')) {
                if (escape) {
                    add_char(c);
                    escape = false;
                } else {
                    escape = true;
                }
            } else if (escape) {
                escape = false;
                if (c == string::to_unicode('r')) {
                    new_text();
                    current_text().setColor(to_sfml(color_));
                } else if (c == string::to_unicode('c')) {
                    col = sf::Color::White;
                    coloring = true;
                    color_char = 0;
                } else if (c == string::to_unicode('b')) {
                    new_text();
                    current_text().setFont(font_bold_);
                } else if (c == string::to_unicode('n')) {
                    new_text();
                    current_text().setFont(font_regular_);
                }
            } else {
                add_char(c);
            }
        }
    }

    float p0 = 0.2*charsize_;
    float line_height = charsize_ + inter_line_;

    line_per_page_ = floor((target.getSize().y - 1.5*charsize_)/line_height);

    std::size_t firstline = firstline_;
    std::size_t lastline = lines.size();
    if (firstline == std::size_t(-1)) {
        firstline = (lines.size() > line_per_page_ ? lines.size() - line_per_page_ : 0);
    } else {
        lastline = firstline + line_per_page_;
        if (lastline > lines.size()) {
            lastline = lines.size();
        }
    }

    for (std::size_t i : range(firstline, lines)) {
        auto& line = lines[i];
        for (auto& t : line) {
            sf::Vector2f p = t.getPosition();
            p.y = round(p0);
            t.setPosition(p);

            target.draw(t);
        }

        p0 += line_height;
    }
}

string::unicode color_to_code(const color32& c) {
    string::unicode s = string::to_unicode("|c");
    s += string::to_unicode(string::uchar_to_hex(c.a));
    s += string::to_unicode(string::uchar_to_hex(c.r));
    s += string::to_unicode(string::uchar_to_hex(c.g));
    s += string::to_unicode(string::uchar_to_hex(c.b));
    return s;
}

console_logger::console_logger(console_output& console, const std::array<color32,8>& cp) :
    console_(console) {

    for (std::size_t i : range(cp)) {
        color_palette_[i] = color_to_code(cp[i]);
    }

    color_ = true;
    stamp_ = true;
}

void console_logger::print_(color::set s) {
    switch (s.col_) {
        case color::black :   buffer_ += color_palette_[0]; break;
        case color::red :     buffer_ += color_palette_[1]; break;
        case color::green :   buffer_ += color_palette_[2]; break;
        case color::yellow :  buffer_ += color_palette_[3]; break;
        case color::blue :    buffer_ += color_palette_[4]; break;
        case color::magenta : buffer_ += color_palette_[5]; break;
        case color::cyan :    buffer_ += color_palette_[6]; break;
        case color::white :   buffer_ += color_palette_[7]; break;
        case color::normal :  buffer_ += string::to_unicode("|r"); break;
    }

    if (s.bold_) buffer_ += string::to_unicode("|b");
    else         buffer_ += string::to_unicode("|n");
}

void console_logger::print_(color::reset_t s) {
    buffer_ += string::to_unicode("|r|n");
}

void console_logger::print_(color::bold_t s) {
    buffer_ += string::to_unicode("|b");
}

void console_logger::print_stamp() {
    if (stamp_) {
        print(color::set(color::normal,true)); print("[");
        print(color::set(color::cyan,  true)); print(today_str("/"));
        print(color::set(color::normal,true)); print("||");
        print(color::set(color::green, true)); print(time_of_day_str(":"));
        print(color::set(color::normal,true)); print("] ");
        print(color::set(color::normal,false));
    }
}

bool console_logger::is_open() const {
    return true;
}

void console_logger::print(const std::string& s) {
    buffer_ += string::to_unicode(s);
}

void console_logger::endl() {
    console_.add_line(buffer_);
    buffer_.clear();
}
