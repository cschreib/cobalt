#include "server_space_object.hpp"

namespace server {
    space_object::space_object(uuid_t id) : id_(id) {}

    uuid_t space_object::id() const {
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

    std::unique_ptr<space_object> space_object_factory::make(std::uint16_t type) const {
        auto iter = factories_.find(type);
        if (iter == factories_.end()) return nullptr;

        return (*iter)->make();
    }

    std::unique_ptr<space_object> space_object_factory::make(std::uint16_t type, uuid_t id) const {
        auto iter = factories_.find(type);
        if (iter == factories_.end()) return nullptr;

        return (*iter)->make(id);
    }
}
