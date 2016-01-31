#include "server_space_object.hpp"

namespace server {
    std::uint32_t space_object::id() const {
        return id_;
    }

    space_cell* space_object::cell() {
        return cell_;
    }

    const space_cell* space_object::cell() const {
        return cell_;
    }

    void space_object::notify_parent_cell(space::cell<space_object>* c) {
        cell_ = c;
    }
}
