#include "server_space_object.hpp"

namespace server {
    space_object::space_object(std::uint32_t id) : id_(id) {}

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


    std::unique_ptr<space_object> space_object_factory::make(std::uint16_t type,
        std::uint32_t id) const {
        auto iter = factories_.find(type);
        if (iter == factories_.end()) return nullptr;

        return (*iter)->make(id);
    }
}
