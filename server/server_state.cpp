#include "server_state.hpp"
#include "server_instance.hpp"

namespace server {
namespace state {
    base::base(server::instance& serv, server::state_id id, std::string name) :
        id_(id), name_(std::move(name)), serv_(serv),
        net_(serv.get_netcom()), out_(serv.get_log()) {}

    const std::string& base::name() const {
        return name_;
    }

    state_id base::id() const {
        return id_;
    }
}
}
