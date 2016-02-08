#include "sfml_wrapper.hpp"

sf::Color to_sfml(const color32& c) {
    return sf::Color(c.r, c.g, c.b, c.a);
}
