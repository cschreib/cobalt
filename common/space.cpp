#include "space.hpp"
#include "string.hpp"

namespace space {
namespace exception {
    base::base(const std::string& s) : std::runtime_error(s) {}
    base::base(const char* s) : std::runtime_error(s) {}

    base::~base() noexcept {}

    cell_occupied::cell_occupied() :
        base("this cell already contains an object") {}

    invalid_direction::invalid_direction() :
        base("invalid direction provided") {}

    invalid_position::invalid_position(const vec_t& pos) :
        base("invalid position, goes out of the universe's boundaries: "+string::convert(pos)) {}
}

namespace impl {
    bool get_cell_id(std::size_t id, direction dir, std::size_t& next_id) {
        switch (id) {
        case sub_cell::TL :
            switch (dir) {
            case direction::LEFT  : next_id = sub_cell::TR; return false;
            case direction::UP    : next_id = sub_cell::BL; return false;
            case direction::RIGHT : next_id = sub_cell::TR; return true;
            case direction::DOWN  : next_id = sub_cell::BL; return true;
            }
        case sub_cell::TR :
            switch (dir) {
            case direction::LEFT  : next_id = sub_cell::TL; return true;
            case direction::UP    : next_id = sub_cell::BR; return false;
            case direction::RIGHT : next_id = sub_cell::TL; return false;
            case direction::DOWN  : next_id = sub_cell::BR; return true;
            }
        case sub_cell::BR :
            switch (dir) {
            case direction::LEFT  : next_id = sub_cell::BL; return true;
            case direction::UP    : next_id = sub_cell::TR; return true;
            case direction::RIGHT : next_id = sub_cell::BL; return false;
            case direction::DOWN  : next_id = sub_cell::TR; return false;
            }
        case sub_cell::BL :
            switch (dir) {
            case direction::LEFT  : next_id = sub_cell::BR; return false;
            case direction::UP    : next_id = sub_cell::TL; return true;
            case direction::RIGHT : next_id = sub_cell::BR; return true;
            case direction::DOWN  : next_id = sub_cell::TL; return false;
            }
        default : throw exception::invalid_direction();
        }
    }
}
}
