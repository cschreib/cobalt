#include "client_server_state.hpp"
#include "client_server_instance.hpp"

namespace client {
namespace server_state {
    base::base(server_instance& serv, server::state_id id, std::string name) :
        id_(id), name_(std::move(name)), serv_(serv),
        net_(serv.get_netcom()), out_(serv.get_log()) {}

    base::~base() {}

    const std::string& base::name() const {
        return name_;
    }

    server::state_id base::id() const {
        return id_;
    }

    void base::transition_to_(server_state::base& st) {}
    void base::end_of_transition_() {}

    void base::transition_to(server_state::base& st) {
        transition_to_(st);
        st.end_of_transition_();
    }

    void base::register_lua(sol::state& lua) {}
    void base::unregister_lua(sol::state& lua) {}
}
}
