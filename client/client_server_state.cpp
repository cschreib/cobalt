#include "client_server_state.hpp"
#include "client_server_instance.hpp"

namespace client {
namespace server_state {
    base::base(server_instance& serv, server::state_id id, std::string name) :
        id_(id), name_(std::move(name)), serv_(serv),
        net_(serv.get_netcom()), out_(serv.get_log()) {}

    const std::string& base::name() const {
        return name_;
    }

    server::state_id base::id() const {
        return id_;
    }
}
}
