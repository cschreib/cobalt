#ifndef CONFIG_SHARED_STATE_HPP
#define CONFIG_SHARED_STATE_HPP

#include "shared_collection.hpp"
#include "netcom_base.hpp"

namespace packet {
    NETCOM_PACKET(config_state) {
        std::string serialized;
    };

    NETCOM_PACKET(config_value_changed) {
        std::string name;
        std::string value;
    };

    NETCOM_PACKET(config_empty) {};

    NETCOM_PACKET(config_clear) {};
}

namespace config {
    class shared_state : private state {
        struct shared_collection_traits {
            using full_packet   = packet::config_state;
            using add_packet    = packet::config_value_changed;
            using remove_packet = packet::config_empty;
            using clear_packet  = packet::config_clear;
        };

        friend class shared_state_observer;

        shared_collection<shared_collection_traits> shared_;

    public :
        template<typename Netcom>
        shared_state(Netcom& net, std::string name) :
            shared_(net.template make_shared_collection<shared_collection_traits>(
                std::move(name))) {
            shared_.make_collection_packet([this](packet::config_state& st) {
                std::ostringstream ss;
                save(ss);
                st.serialized = ss.str();
            });

            on_value_changed.connect([this](const std::string& name, const std::string& value) {
                shared_.add_item(name, value);
            });

            shared_.connect();
        }

        void clear();

        using state::on_value_changed;
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
        scoped_connection_pool pool_;
        actor_id_t aid_;

    public :
        template<typename Netcom>
        shared_state_observer(Netcom& net, actor_id_t aid, const std::string& name) : aid_(aid) {
            pool_ << net.send_request(aid, make_packet<request::get_shared_collection_id>(name),
                [&,this](const netcom_base::request_answer_t<request::get_shared_collection_id>& msg) {

                shared_ = net.template make_shared_collection_observer
                    <shared_state::shared_collection_traits>(msg.answer.id);

                shared_.on_received.connect([this](const packet::config_state& st) {
                    std::istringstream ss(st.serialized);
                    parse(ss);
                });

                shared_.on_add_item.connect([this](const packet::config_value_changed& val) {
                    set_raw_value(val.name, val.value);
                });

                shared_.on_clear.connect([this](const packet::config_clear& val) {
                    clear();
                });

                shared_.connect(aid_);
            });
        }

        using state::on_value_changed;
        using state::save;
        using state::save_to_file;
        using state::get_value;
        using state::get_raw_value;
        using state::value_exists;
        using state::bind;
    };
}

#ifndef NO_AUTOGEN
#include "autogen/packets/config_shared_state.hpp"
#endif

#endif
