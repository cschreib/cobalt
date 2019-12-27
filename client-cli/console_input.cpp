#include "console_input.hpp"
#include "sfml_wrapper.hpp"
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

console_input::console_input(const string::unicode& prompt, const sf::Font& f,
    std::size_t cs, const color32& col) :
    charsize_(cs), prompt_(prompt), text_(prompt, f, cs) {

    on_updated.connect([this](const string::unicode& s) {
        text_.setString(prompt_+s);
    });

    text_.setPosition(0,0);
    text_.setFillColor(to_sfml(col));
    carret_.setOrigin(0,-0.2*charsize_);
    carret_.setSize(sf::Vector2f(1.0, charsize_));
    highlight_.setOrigin(0,-0.2*charsize_);
    highlight_.setFillColor(sf::Color(255,255,255,128));
}


void console_input::history_to_present_() {
    if (hpos_ != history_.size()) {
        hpos_ = history_.size();
        saved_content_.clear();
    }
}

void console_input::erase_selection_() {
    if (spos_ < pos_) std::swap(spos_, pos_);
    content_.erase(pos_, spos_-pos_);
    on_updated.dispatch(content_);
    selected_ = false;
}

void console_input::clear_selection_() {
    selected_ = false;
}

void console_input::recall_previous() {
    if (hpos_ == 0) return;

    if (selected_) {
        clear_selection_();
    }

    if (hpos_ == history_.size()) {
        saved_content_ = content_;
    }

    --hpos_;

    content_ = history_[hpos_];
    pos_ = content_.size();
    on_updated.dispatch(content_);
}

void console_input::recall_next() {
    if (hpos_ == history_.size()) return;

    if (selected_) {
        clear_selection_();
    }

    ++hpos_;

    if (hpos_ == history_.size()) {
        content_ = saved_content_;
        pos_ = content_.size();
    } else {
        content_ = history_[hpos_];
        pos_ = content_.size();
    }

    on_updated.dispatch(content_);
}

void console_input::erase_before() {
    history_to_present_();

    if (selected_) {
        erase_selection_();
    } else {
        if (pos_ != 0) {
            content_.erase(pos_-1, 1);
            --pos_;
            on_updated.dispatch(content_);
        }
    }
}

void console_input::erase_after() {
    history_to_present_();

    if (selected_) {
        erase_selection_();
    } else {
        if (pos_ != content_.size()) {
            content_.erase(pos_, 1);
            on_updated.dispatch(content_);
        }
    }
}

void console_input::insert(string::unicode_char c) {
    history_to_present_();

    selecting_ = false;
    if (selected_) {
        erase_selection_();
    }

    content_.insert(content_.begin()+pos_, c);
    ++pos_;
    on_updated.dispatch(content_);
}

void console_input::clear() {
    if (content_.empty()) return;
    history_.push_back(content_);
    hpos_ = history_.size();
    pos_ = 0;
    content_.clear();
    on_updated.dispatch(content_);
}

std::size_t console_input::size() const {
    return content_.size();
}

void console_input::move_backward() {
    if (!selecting_ && selected_) {
        selected_ = false;
        on_updated.dispatch(content_);
    } else {
        if (pos_ != 0) {
            --pos_;
            on_updated.dispatch(content_);
        }
    }
}

void console_input::move_forward() {
    if (!selecting_ && selected_) {
        selected_ = false;
        on_updated.dispatch(content_);
    } else {
        if (pos_ != content_.size()) {
            ++pos_;
            on_updated.dispatch(content_);
        }
    }
}

void console_input::move_first() {
    bool updated = false;
    if (!selecting_ && selected_) {
        selected_ = false;
        updated = true;
    }

    if (pos_ != 0) {
        pos_ = 0;
        updated = true;
    }

    if (updated) {
        on_updated.dispatch(content_);
    }
}

void console_input::move_last() {
    bool updated = false;
    if (!selecting_ && selected_) {
        selected_ = false;
        updated = true;
    }

    if (pos_ != content_.size()) {
        pos_ = content_.size();
        updated = true;
    }

    if (updated) {
        on_updated.dispatch(content_);
    }
}

void console_input::ask_autocomplete() {
    if (!selected_) {
        auto p0 = content_.find_last_of(string::to_unicode("\t\n ()"), pos_);
        if (p0 == content_.npos) {
            p0 = 0;
        } else {
            ++p0;
        }

        string::unicode str = content_.substr(p0, pos_-p0);
        on_autocompletion_query.dispatch(str);
    }
}

void console_input::autocomplete(string::unicode text) {
    selected_ = false;

    auto p0 = content_.find_last_of(string::to_unicode("\t\n ()"), pos_);
    if (p0 == content_.npos) {
        p0 = 0;
    } else {
        ++p0;
    }

    content_.erase(p0, pos_-p0);
    content_.insert(p0, text);
    pos_ = p0 + text.size();

    on_updated.dispatch(content_);
}

string::unicode console_input::content() const {
    return content_;
}

void console_input::on_event(const sf::Event& e) {
    switch (e.type) {
    case sf::Event::TextEntered:
        if (e.text.unicode == 13) {
            on_text_entered.dispatch(content_);
            clear();
        } else if (e.text.unicode == 8) {
            erase_before();
        } else if (e.text.unicode == 127) {
            erase_after();
        } else if (e.text.unicode == 9) {
            // Tab
        } else {
            insert(e.text.unicode);
        }
        break;
    case sf::Event::KeyPressed:
        switch (e.key.code) {
            case sf::Keyboard::Key::LShift:
            case sf::Keyboard::Key::RShift:
                selecting_ = true;
                spos_ = pos_;
                break;
            case sf::Keyboard::Key::Left:
                move_backward();
                break;
            case sf::Keyboard::Key::Right:
                move_forward();
                break;
            case sf::Keyboard::Key::Up:
                recall_previous();
                break;
            case sf::Keyboard::Key::Down:
                recall_next();
                break;
            case sf::Keyboard::Key::Home:
                move_first();
                break;
            case sf::Keyboard::Key::End:
                move_last();
                break;
            case sf::Keyboard::Key::Tab:
                ask_autocomplete();
                break;
            default: break;
        }
        break;
    case sf::Event::KeyReleased:
        switch (e.key.code) {
            case sf::Keyboard::Key::LShift:
            case sf::Keyboard::Key::RShift:
                if (selecting_) {
                    selecting_ = false;
                    selected_ = spos_ != pos_;
                }
                break;
            default: break;
        }
        break;
    default: break;
    }
}

void console_input::draw(sf::RenderTarget& target) const {
    text_.setPosition(0,target.getSize().y - round(1.3*charsize_));

    sf::Vector2f cpos = text_.findCharacterPos(pos_+prompt_.size());
    std::size_t rpos = 0;
    string::unicode rcontent = content_;
    while (cpos.x > target.getSize().x && rpos != content_.size()) {
        ++rpos;
        rcontent = content_.substr(rpos);
        text_.setString(prompt_+rcontent);
        cpos = text_.findCharacterPos((pos_ - rpos)+prompt_.size());
    }

    target.draw(text_);
    carret_.setPosition(cpos);
    target.draw(carret_);

    if (selecting_ || selected_) {
        std::size_t ip0 = (pos_ > rpos ? pos_ - rpos : 0);
        std::size_t ip1 = (spos_ > rpos ? spos_ - rpos : 0);
        sf::Vector2f p0 = text_.findCharacterPos(ip0+prompt_.size());
        sf::Vector2f p1 = text_.findCharacterPos(ip1+prompt_.size());
        if (spos_ < pos_) std::swap(p0, p1);

        highlight_.setPosition(p0);
        highlight_.setSize((p1-p0)+sf::Vector2f(0.0,charsize_));
        target.draw(highlight_);
    }
}
