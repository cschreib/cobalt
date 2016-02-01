#ifndef SERVER_SPACE_OBJECT_HPP
#define SERVER_SPACE_OBJECT_HPP

#include <space.hpp>
#include <uuid.hpp>
#include <std_addon.hpp>

namespace server {
    class space_object;

    using space_cell = space::cell<server::space_object>;

    class space_object {
        const uuid_t id_;
        space_cell* cell_;

    public :
        explicit space_object(uuid_t);
        virtual ~space_object() = default;

        virtual void serialize(serialized_packet& p) const = 0;
        virtual void deserialize(serialized_packet& p) = 0;

        uuid_t id() const;
        virtual std::uint16_t type() const = 0;

        space_cell* cell();
        const space_cell* cell() const;
        void notify_parent_cell(space_cell* c);
    };

    class space_object_factory {
        struct object_factory {
            std::uint16_t type;

            explicit object_factory(std::uint16_t t) : type(t) {}
            virtual ~object_factory() = default;
            virtual std::unique_ptr<space_object> make() const = 0;
            virtual std::unique_ptr<space_object> make(uuid_t id) const = 0;
        };

        template<typename T>
        struct object_factory_impl : object_factory {
            explicit object_factory_impl(std::uint16_t t) : object_factory(t) {}
            std::unique_ptr<space_object> make() const override {
                std::unique_ptr<space_object> obj;

                // Pre-allocate memory to get uuid before object is constructed
                void* buffer = operator new(sizeof(T));
                uuid_t id = make_uuid(buffer);

                // Construct object
                try {
                    obj = new (buffer) T(id);
                } catch (...) {
                    operator delete(buffer);
                }

                return obj;
            }

            std::unique_ptr<space_object> make(uuid_t id) const override {
                return std::make_unique<T>(id);
            }
        };

        struct factory_cmp {
            using T = std::unique_ptr<object_factory>;
            bool operator() (const T& n1, const T& n2) const {
                return n1->type < n2->type;
            }
            bool operator() (const T& n1, std::uint16_t i) const {
                return n1->type < i;
            }
            bool operator() (std::uint16_t i, const T& n2) const {
                return i < n2->type;
            }
        };

        ctl::sorted_vector<std::unique_ptr<object_factory>, factory_cmp> factories_;

    public :

        template<typename T>
        void add_factory(std::uint16_t type) {
            factories_.insert(std::make_unique<object_factory_impl<T>>(type));
        }

        std::unique_ptr<space_object> make(std::uint16_t type) const;
        std::unique_ptr<space_object> make(std::uint16_t type, uuid_t id) const;
    };
}

#endif
