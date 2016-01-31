#ifndef SERVER_UNIVERSE_SERIALIZE_HPP
#define SERVER_UNIVERSE_SERIALIZE_HPP

#include "server_universe.hpp"

namespace server {
    namespace v1 {
        static const std::string version_header = "SCUV1";

        struct universe_header {
            std::string   header = version_header;
            std::uint16_t depth  = 0;
            std::uint32_t nobject = 0;
        };

        struct universe_serializer_internal_buffer {
            universe_header header;
            std::vector<serialized_packet> objects;
        };
    }
}

#endif
