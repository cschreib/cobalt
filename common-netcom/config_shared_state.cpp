#include "config_shared_state.hpp"

namespace config {
    shared_state::shared_state(netcom& net, std::string name) :
        shared_(net.make_shared_collection<shared_collection_traits>(std::move(name))) {

        shared_.make_collection_packet([this](packet::config_state& st) {
            std::istringstream ss;
            save(ss);
            st.serialized = ss.str();
        });

        on_value_changed.connect([this](const std::string& name, const std::string& value) {
            shared_.add_item(name, value);
        });

        collection_.connect();
    }

    shared_state_observer::shared_state_observer(netcom& net, actor_id_t aid, const std::string& name) {
        pool_ << net.send_request(aid, make_packet<request::get_shared_collection_id>(name),
            [this](const netcom_base::answer_t<request::get_shared_collection_id>& ans) {

            shared_ = net.make_shared_collection_observer(ans.id);

            shared_.on_received.connect([this](const packet::config_state& st) {
                std::ostringstream ss(st.serialized);
                parse(ss);
            });

            shared_.on_add_item.connect([this](const packet::config_value_changed& val) {
                set_raw_value(val.name, val.value);
            });

            collection_.connect(aid);
        });
    }
}
