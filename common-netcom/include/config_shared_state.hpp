#ifndef CONFIG_SHARED_STATE_HPP
#define CONFIG_SHARED_STATE_HPP

#include "shared_collection.hpp"
#include "any.hpp"

namespace packet {
    NETCOM_PACKET(config_state) {
        std::string serialized;
    };

    NETCOM_PACKET(config_value_changed) {
        std::string name;
        std::string value;
    };

    NETCOM_PACKET(config_empty) {};
}

namespace config {
    class shared_state : private state {
        struct shared_collection_traits {
            using full_packet   = packet::config_state;
            using add_packet    = packet::config_value_changed;
            using remove_packet = packet::config_empty;
        };

        friend class shared_state_observer;

        shared_collection<shared_collection_traits> shared_;

    public :
        shared_state(netcom_base& net, std::string name);

        using state::parse;
        using state::parse_from_file;
        using state::save;
        using state::save_to_file;
        using state::get_value;
        using state::get_raw_value;
        using state::set_value;
        using state::set_raw_value;
        using state::value_exists;
        using state::bind;
    };

    class shared_state_observer : private state {
        shared_collection_observer<shared_state::shared_collection_traits> shared_;

    public :
        shared_state_observer(netcom_base& net, actor_id_t aid, const std::string& name);

        using state::save;
        using state::save_to_file;
        using state::get_value;
        using state::get_raw_value;
        using state::value_exists;
        using state::bind;
    };
}

#endif
