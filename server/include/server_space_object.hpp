#ifndef SERVER_SPACE_OBJECT_HPP
#define SERVER_SPACE_OBJECT_HPP

#include <space.hpp>

namespace server {
    class space_object;

    using space_cell = space::cell<server::space_object>;

    class space_object {
        std::uint32_t id_;
        space_cell* cell_;

    public :
        virtual ~space_object() = default;

        virtual void serialize(serialized_packet& p) const = 0;

        std::uint32_t id() const;
        virtual std::uint16_t type() const = 0;

        space_cell* cell();
        const space_cell* cell() const;
        void notify_parent_cell(space_cell* c);
    };
}

#endif
