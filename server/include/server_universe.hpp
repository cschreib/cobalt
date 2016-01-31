#ifndef SERVER_UNIVERSE_HPP
#define SERVER_UNIVERSE_HPP

#include <string>
#include <memory>
#include <space.hpp>
#include "server_serializable.hpp"

namespace server {
    class universe_serializer;
    class space_object;

    using space_universe = space::universe<server::space_object>;

    class universe {
        friend class universe_serializer;

        std::unique_ptr<space_universe> space_;

    public :
        universe() = default;
        virtual ~universe() = default;

        std::unique_ptr<universe_serializer> make_serializer();
    };

    struct universe_serializer_internal_buffer;

    class universe_serializer : public server::serializable {
        universe& universe_;

        std::unique_ptr<universe_serializer_internal_buffer> buffer_;

    public :
        explicit universe_serializer(universe& uni);
        ~universe_serializer();

        void save_data() override;
        void serialize(const std::string& dir) const override;

        void deserialize(const std::string& dir) override;
        void load_data_first_pass() override;
        void load_data_second_pass() override;

        void clear() override;

        bool is_valid_directory(const std::string& dir) const override;
    };
}

#endif
