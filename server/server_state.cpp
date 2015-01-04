#include "server_state.hpp"
#include "server_instance.hpp"

namespace server {
namespace state {
    base::base(server::instance& serv, std::string name) :
        name_(std::move(name)), serv_(serv),
        net_(serv.get_netcom()), out_(serv.get_log()) {}

    const std::string& base::name() const {
        return name_;
    }
}
}
